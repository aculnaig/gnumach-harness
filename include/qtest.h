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

#ifndef GNUMACH_HARNESS_QTEST_H
#define GNUMACH_HARNESS_QTEST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

/**
 * struct qtest_config - Configuration parameters to launch a QEMU instance.
 */
struct qtest_config
{
  const char *qemu_bin;		/* Path to qemu binary (default: qemu-system-aarch64) */
  const char *machine;		/* Machine type (default: virt) */
  const char *cpu;		/* CPU model (default: cortex-a53) */
  const char *memory;		/* Memory size string (default: 1G) */
  const char *kernel_bin;	/* Path to kernel binary (optional) */
  const char *kernel_append;	/* Kernel command line arguments (optional) */
  const char *extra_device;	/* Extra device argument, e.g. guest-loader (optional) */
  int timeout_sec;		/* Timeout in seconds (default: 10) */
};

/**
 * struct qtest_session - Handle for an active QEMU QTest session.
 */
typedef struct qtest_session
{
  pid_t qemu_pid;
  int sock_fd;
  char tmp_dir[128];
  char sock_path[256];
  int timeout_sec;
} qtest_session_t;

/* Session lifecycle functions */
qtest_session_t *qtest_init (const struct qtest_config *cfg);
void qtest_quit (qtest_session_t * s);

/* Low-level command execution */
char *qtest_send_cmd (qtest_session_t * s, const char *fmt, ...);

/* Memory access primitives */
uint8_t qtest_readb (qtest_session_t * s, uint64_t addr);
uint16_t qtest_readw (qtest_session_t * s, uint64_t addr);
uint32_t qtest_readl (qtest_session_t * s, uint64_t addr);
uint64_t qtest_readq (qtest_session_t * s, uint64_t addr);

void qtest_writeb (qtest_session_t * s, uint64_t addr, uint8_t val);
void qtest_writew (qtest_session_t * s, uint64_t addr, uint16_t val);
void qtest_writel (qtest_session_t * s, uint64_t addr, uint32_t val);
void qtest_writeq (qtest_session_t * s, uint64_t addr, uint64_t val);

bool qtest_memread (qtest_session_t * s, uint64_t addr, void *buf,
		    size_t len);
bool qtest_memwrite (qtest_session_t * s, uint64_t addr, const void *buf,
		     size_t len);

/* Assertions for test cases */
#define QTEST_ASSERT(expr)                                                     \
  do                                                                           \
    {                                                                          \
      if (!(expr))                                                             \
        {                                                                      \
          fprintf (stderr, "[FAIL] %s:%d: Assertion '%s' failed\n",            \
                   __FILE__, __LINE__, #expr);                                 \
          exit (EXIT_FAILURE);                                                 \
        }                                                                      \
    }                                                                          \
  while (0)

#define QTEST_ASSERT_EQ(actual, expected)                                      \
  do                                                                           \
    {                                                                          \
      uint64_t _act = (uint64_t) (actual);                                     \
      uint64_t _exp = (uint64_t) (expected);                                   \
      if (_act != _exp)                                                        \
        {                                                                      \
          fprintf (stderr,                                                     \
                   "[FAIL] %s:%d: Expected 0x%lx (%lu), got 0x%lx (%lu)\n",     \
                   __FILE__, __LINE__, (unsigned long) _exp,                   \
                   (unsigned long) _exp, (unsigned long) _act,                 \
                   (unsigned long) _act);                                      \
          exit (EXIT_FAILURE);                                                 \
        }                                                                      \
    }                                                                          \
  while (0)

#endif /* GNUMACH_HARNESS_QTEST_H */
