/*
 * Copyright (C) 2026 Free Software Foundation, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#define _POSIX_C_SOURCE 200809L

#include "qtest.h"

#include <errno.h>
#include <fcntl.h>
#include <glib.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define DEFAULT_TIMEOUT_SEC 10

static int
connect_unix_socket (const char *path, int timeout_sec)
{
  int fd = -1;
  struct sockaddr_un addr;
  struct timespec req = { 0, 50000000L };	/* 50 ms */
  int elapsed_ms = 0;
  int max_ms = timeout_sec * 1000;

  if (strlen (path) >= sizeof (addr.sun_path))
    {
      fprintf (stderr, "Error: socket path too long: %s\n", path);
      return -1;
    }

  memset (&addr, 0, sizeof (addr));
  addr.sun_family = AF_UNIX;
  strncpy (addr.sun_path, path, sizeof (addr.sun_path) - 1);

  while (elapsed_ms < max_ms)
    {
      fd = socket (AF_UNIX, SOCK_STREAM, 0);
      if (fd >= 0)
	{
	  if (connect (fd, (struct sockaddr *) &addr, sizeof (addr)) == 0)
	    return fd;

	  close (fd);
	  fd = -1;
	}

      nanosleep (&req, NULL);
      elapsed_ms += 50;
    }

  return -1;
}

qtest_session_t *
qtest_init (const struct qtest_config *cfg)
{
  qtest_session_t *s;
  char qtest_arg[512];
  char *argv[32];
  int argc = 0;
  pid_t pid;
  int timeout_sec;

  s = calloc (1, sizeof (*s));
  if (!s)
    {
      perror ("calloc");
      return NULL;
    }

  timeout_sec = (cfg
		 && cfg->timeout_sec >
		 0) ? cfg->timeout_sec : DEFAULT_TIMEOUT_SEC;
  s->timeout_sec = timeout_sec;

  /* Create temporary directory for isolated socket */
  snprintf (s->tmp_dir, sizeof (s->tmp_dir), "/tmp/qtest-XXXXXX");
  if (!mkdtemp (s->tmp_dir))
    {
      perror ("mkdtemp");
      free (s);
      return NULL;
    }

  snprintf (s->sock_path, sizeof (s->sock_path), "%s/qtest.sock", s->tmp_dir);
  snprintf (qtest_arg, sizeof (qtest_arg), "unix:%s,server=on,wait=off",
	    s->sock_path);

  /* Build argument vector */
  argv[argc++] =
    (char *) ((cfg && cfg->qemu_bin) ? cfg->qemu_bin : "qemu-system-aarch64");
  argv[argc++] = "-machine";
  argv[argc++] = (char *) ((cfg && cfg->machine) ? cfg->machine : "virt");
  argv[argc++] = "-cpu";
  argv[argc++] = (char *) ((cfg && cfg->cpu) ? cfg->cpu : "cortex-a53");
  argv[argc++] = "-m";
  argv[argc++] = (char *) ((cfg && cfg->memory) ? cfg->memory : "1G");
  argv[argc++] = "-nographic";
  argv[argc++] = "-qtest";
  argv[argc++] = qtest_arg;
  argv[argc++] = "-qtest-log";
  argv[argc++] = "/dev/null";

  if (cfg && cfg->kernel_bin)
    {
      argv[argc++] = "-kernel";
      argv[argc++] = (char *) cfg->kernel_bin;
    }

  if (cfg && cfg->kernel_append)
    {
      argv[argc++] = "-append";
      argv[argc++] = (char *) cfg->kernel_append;
    }

  if (cfg && cfg->extra_device)
    {
      argv[argc++] = "-device";
      argv[argc++] = (char *) cfg->extra_device;
    }

  argv[argc] = NULL;

  pid = fork ();
  if (pid < 0)
    {
      perror ("fork");
      rmdir (s->tmp_dir);
      free (s);
      return NULL;
    }

  if (pid == 0)
    {
      /* Child process: redirect standard output/error to /dev/null */
      int devnull = open ("/dev/null", O_RDWR);
      if (devnull >= 0)
	{
	  dup2 (devnull, STDOUT_FILENO);
	  dup2 (devnull, STDERR_FILENO);
	  close (devnull);
	}
      execvp (argv[0], argv);
      _exit (127);
    }

  s->qemu_pid = pid;

  /* Connect to QTest socket */
  s->sock_fd = connect_unix_socket (s->sock_path, timeout_sec);
  if (s->sock_fd < 0)
    {
      fprintf (stderr, "Error: could not connect to QEMU qtest socket '%s'\n",
	       s->sock_path);
      qtest_quit (s);
      return NULL;
    }

  return s;
}

void
qtest_quit (qtest_session_t *s)
{
  if (!s)
    return;

  if (s->sock_fd >= 0)
    {
      close (s->sock_fd);
      s->sock_fd = -1;
    }

  if (s->qemu_pid > 0)
    {
      int status;
      kill (s->qemu_pid, SIGTERM);

      /* Wait with short timeout for graceful termination */
      for (int i = 0; i < 10; ++i)
	{
	  if (waitpid (s->qemu_pid, &status, WNOHANG) > 0)
	    {
	      s->qemu_pid = -1;
	      break;
	    }
	  struct timespec req = { 0, 50000000L };	/* 50 ms */
	  nanosleep (&req, NULL);
	}

      if (s->qemu_pid > 0)
	{
	  kill (s->qemu_pid, SIGKILL);
	  waitpid (s->qemu_pid, &status, 0);
	  s->qemu_pid = -1;
	}
    }

  if (s->sock_path[0])
    {
      unlink (s->sock_path);
      s->sock_path[0] = '\0';
    }

  if (s->tmp_dir[0])
    {
      rmdir (s->tmp_dir);
      s->tmp_dir[0] = '\0';
    }

  free (s);
}
