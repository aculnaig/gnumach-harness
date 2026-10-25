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
#include <glib.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_LINE_LEN 4096

static int
qtest_readline (int fd, char *buf, size_t max_len)
{
  size_t idx = 0;

  while (idx + 1 < max_len)
    {
      char c;
      ssize_t rc = recv (fd, &c, 1, 0);
      if (rc <= 0)
	{
	  if (rc < 0 && (errno == EINTR || errno == EAGAIN))
	    continue;
	  return -1;
	}

      if (c == '\r')
	continue;

      if (c == '\n')
	{
	  buf[idx] = '\0';
	  return (int) idx;
	}

      buf[idx++] = c;
    }

  buf[idx] = '\0';
  return (int) idx;
}

char *
qtest_send_cmd (qtest_session_t *s, const char *fmt, ...)
{
  va_list args;
  char *cmd;
  char line[MAX_LINE_LEN];
  size_t len;
  ssize_t sent;

  if (!s || s->sock_fd < 0)
    return NULL;

  va_start (args, fmt);
  cmd = g_strdup_vprintf (fmt, args);
  va_end (args);

  if (!cmd)
    return NULL;

  /* Append newline if not present */
  if (!g_str_has_suffix (cmd, "\n"))
    {
      char *tmp = g_strconcat (cmd, "\n", NULL);
      g_free (cmd);
      cmd = tmp;
    }

  len = strlen (cmd);
  sent = send (s->sock_fd, cmd, len, 0);
  g_free (cmd);

  if (sent < (ssize_t) len)
    return NULL;

  /* Read response line, skipping comments and asynchronous messages */
  while (1)
    {
      if (qtest_readline (s->sock_fd, line, sizeof (line)) < 0)
	return NULL;

      /* Skip empty lines or comment lines starting with '#' */
      if (line[0] == '\0' || line[0] == '#')
	continue;

      break;
    }

  return g_strdup (line);
}

static uint64_t
qtest_read_primitive (qtest_session_t *s, const char *cmd_name, uint64_t addr)
{
  char *resp;
  uint64_t val = 0;

  resp = qtest_send_cmd (s, "%s 0x%" PRIx64 "\n", cmd_name, addr);
  if (!resp)
    {
      fprintf (stderr, "Error: %s 0x%" PRIx64 " failed (no response)\n",
	       cmd_name, addr);
      return 0;
    }

  if (g_str_has_prefix (resp, "OK "))
    {
      val = g_ascii_strtoull (resp + 3, NULL, 16);
    }
  else
    {
      fprintf (stderr, "Error: %s 0x%" PRIx64 " returned: %s\n",
	       cmd_name, addr, resp);
    }

  g_free (resp);
  return val;
}

static void
qtest_write_primitive (qtest_session_t *s, const char *cmd_name,
		       uint64_t addr, uint64_t val)
{
  char *resp;

  resp = qtest_send_cmd (s, "%s 0x%" PRIx64 " 0x%" PRIx64 "\n",
			 cmd_name, addr, val);
  if (!resp)
    {
      fprintf (stderr, "Error: %s 0x%" PRIx64 " failed (no response)\n",
	       cmd_name, addr);
      return;
    }

  if (!g_str_has_prefix (resp, "OK"))
    {
      fprintf (stderr, "Error: %s 0x%" PRIx64 " returned: %s\n",
	       cmd_name, addr, resp);
    }

  g_free (resp);
}

uint8_t
qtest_readb (qtest_session_t *s, uint64_t addr)
{
  return (uint8_t) qtest_read_primitive (s, "readb", addr);
}

uint16_t
qtest_readw (qtest_session_t *s, uint64_t addr)
{
  return (uint16_t) qtest_read_primitive (s, "readw", addr);
}

uint32_t
qtest_readl (qtest_session_t *s, uint64_t addr)
{
  return (uint32_t) qtest_read_primitive (s, "readl", addr);
}

uint64_t
qtest_readq (qtest_session_t *s, uint64_t addr)
{
  return qtest_read_primitive (s, "readq", addr);
}

void
qtest_writeb (qtest_session_t *s, uint64_t addr, uint8_t val)
{
  qtest_write_primitive (s, "writeb", addr, val);
}

void
qtest_writew (qtest_session_t *s, uint64_t addr, uint16_t val)
{
  qtest_write_primitive (s, "writew", addr, val);
}

void
qtest_writel (qtest_session_t *s, uint64_t addr, uint32_t val)
{
  qtest_write_primitive (s, "writel", addr, val);
}

void
qtest_writeq (qtest_session_t *s, uint64_t addr, uint64_t val)
{
  qtest_write_primitive (s, "writeq", addr, val);
}

bool
qtest_memread (qtest_session_t *s, uint64_t addr, void *buf, size_t len)
{
  char *resp;
  uint8_t *dest = (uint8_t *) buf;

  if (!buf || len == 0)
    return false;

  resp = qtest_send_cmd (s, "read 0x%" PRIx64 " 0x%zx\n", addr, len);
  if (!resp)
    return false;

  if (!g_str_has_prefix (resp, "OK 0x"))
    {
      g_free (resp);
      return false;
    }

  const char *hex = resp + 5;
  for (size_t i = 0; i < len; ++i)
    {
      char byte_str[3] = { hex[i * 2], hex[i * 2 + 1], '\0' };
      if (!byte_str[0] || !byte_str[1])
	{
	  g_free (resp);
	  return false;
	}
      dest[i] = (uint8_t) g_ascii_strtoull (byte_str, NULL, 16);
    }

  g_free (resp);
  return true;
}

bool
qtest_memwrite (qtest_session_t *s, uint64_t addr, const void *buf,
		size_t len)
{
  const uint8_t *src = (const uint8_t *) buf;
  char *hex_str;
  char *resp;

  if (!buf || len == 0)
    return false;

  hex_str = g_malloc (len * 2 + 1);
  for (size_t i = 0; i < len; ++i)
    snprintf (hex_str + (i * 2), 3, "%02x", src[i]);

  resp = qtest_send_cmd (s, "write 0x%" PRIx64 " 0x%zx 0x%s\n",
			 addr, len, hex_str);
  g_free (hex_str);

  if (!resp)
    return false;

  bool ok = g_str_has_prefix (resp, "OK");
  g_free (resp);
  return ok;
}
