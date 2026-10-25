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

#include "qtest.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define TARGET_RAM_ADDR_MAGIC 0x40001000ULL
#define TARGET_RAM_ADDR_STATUS 0x40001008ULL

#define EXPECTED_MAGIC 0x474E554D41434821ULL	/* "GNUMACH!" */
#define EXPECTED_STATUS 0xCAFEBABEULL

int
main (int argc, char *argv[])
{
  struct qtest_config cfg;
  qtest_session_t *s;
  const char *gnumach_bin = NULL;
  const char *payload_elf = "tests/modules/payload_example.elf";
  char guest_loader_arg[512];
  uint64_t actual_magic = 0;
  uint32_t actual_status = 0;
  bool verified = false;

  (void) argc;
  (void) argv;

  printf
    ("=== Running test_bootstrap_module (deterministic QTest memory check) ===\n");

  memset (&cfg, 0, sizeof (cfg));
  cfg.qemu_bin = "qemu-system-aarch64";
  cfg.machine = "virt";
  cfg.cpu = "cortex-a53";
  cfg.memory = "1G";
  cfg.timeout_sec = 5;

  /* Check for GNUMACH_BIN in environment or local submodule build */
  gnumach_bin = getenv ("GNUMACH_BIN");
  if (!gnumach_bin || access (gnumach_bin, R_OK) != 0)
    {
      if (access ("./gnumach/gnumach.elf", R_OK) == 0)
	gnumach_bin = "./gnumach/gnumach.elf";
      else if (access ("./gnumach/gnumach", R_OK) == 0)
	gnumach_bin = "./gnumach/gnumach";
      else
	gnumach_bin = NULL;
    }

  if (gnumach_bin && access (gnumach_bin, R_OK) == 0)
    {
      printf ("Using kernel: %s\n", gnumach_bin);
      cfg.kernel_bin = gnumach_bin;
      cfg.kernel_append = "console=ttyAMA0";
      snprintf (guest_loader_arg, sizeof (guest_loader_arg),
		"guest-loader,addr=0x40080000,kernel=%s,bootargs='example'",
		payload_elf);
      cfg.extra_device = guest_loader_arg;
    }
  else
    {
      printf
	("Note: GNU Mach kernel binary not specified; running payload directly as kernel\n");
      cfg.kernel_bin = payload_elf;
    }

  s = qtest_init (&cfg);
  QTEST_ASSERT (s != NULL);

  /* Poll guest memory until deterministic signature is written */
  for (int i = 0; i < 20; ++i)
    {
      actual_magic = qtest_readq (s, TARGET_RAM_ADDR_MAGIC);
      actual_status = qtest_readl (s, TARGET_RAM_ADDR_STATUS);

      if (actual_magic == EXPECTED_MAGIC && actual_status == EXPECTED_STATUS)
	{
	  verified = true;
	  break;
	}

      struct timespec req = { 0, 50000000L };	/* 50 ms */
      nanosleep (&req, NULL);
    }

  /* Test block memory read */
  uint8_t mem_buf[12];
  bool mem_ok =
    qtest_memread (s, TARGET_RAM_ADDR_MAGIC, mem_buf, sizeof (mem_buf));
  QTEST_ASSERT (mem_ok);

  /* Assertions */
  QTEST_ASSERT (verified);
  QTEST_ASSERT_EQ (actual_magic, EXPECTED_MAGIC);
  QTEST_ASSERT_EQ (actual_status, EXPECTED_STATUS);

  qtest_quit (s);

  printf
    ("[PASS] test_bootstrap_module: Verified deterministic magic 0x%016"
     PRIx64 " and status 0x%08" PRIx32 "\n", actual_magic, actual_status);

  return EXIT_SUCCESS;
}
