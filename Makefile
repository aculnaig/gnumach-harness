# Copyright (C) 2026 Free Software Foundation, Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

CC ?= gcc
CROSS_CC ?= aarch64-linux-gnu-gcc
CROSS_LD ?= aarch64-linux-gnu-ld

CFLAGS ?= -std=c99 -pedantic -Wall -Wextra -Werror -O2 -g
INCLUDES = -Iinclude

GLIB_CFLAGS := $(shell pkg-config --cflags glib-2.0)
GLIB_LIBS := $(shell pkg-config --libs glib-2.0)

ALL_CFLAGS = $(CFLAGS) $(INCLUDES) $(GLIB_CFLAGS)
ALL_LIBS = $(GLIB_LIBS)

ifeq ($(SANITIZE),1)
ALL_CFLAGS += -fsanitize=address,undefined
ALL_LIBS += -fsanitize=address,undefined
endif

SRC = src/qtest_process.c src/qtest_client.c
OBJS = $(SRC:.c=.o)

TEST_SRCS = tests/test_bootstrap_module.c
TEST_BINS = $(TEST_SRCS:tests/%.c=%)

MODULE_SRC = tests/modules/payload_example.S
MODULE_OBJ = tests/modules/payload_example.o
MODULE_ELF = tests/modules/payload_example.elf

.PHONY: all check clean distclean indent help

all: $(TEST_BINS) $(MODULE_ELF)

%.o: %.c
	$(CC) $(ALL_CFLAGS) -c $< -o $@

$(MODULE_OBJ): $(MODULE_SRC)
	$(CROSS_CC) -c $< -o $@

$(MODULE_ELF): $(MODULE_OBJ) tests/modules/linker.ld
	$(CROSS_LD) -T tests/modules/linker.ld $< -o $@

test_bootstrap_module: tests/test_bootstrap_module.o $(OBJS)
	$(CC) $(ALL_CFLAGS) $^ -o $@ $(ALL_LIBS)

tests/test_bootstrap_module.o: tests/test_bootstrap_module.c
	$(CC) $(ALL_CFLAGS) -c $< -o $@

check: all
	@echo "=========================================="
	@echo " Running gnumach-harness test suite"
	@echo "=========================================="
	@for test in $(TEST_BINS); do \
		echo "Running $$test..."; \
		./$$test || exit 1; \
	done
	@echo "=========================================="
	@echo " All tests passed successfully!"
	@echo "=========================================="

clean:
	rm -f $(OBJS) tests/*.o $(TEST_BINS) $(MODULE_OBJ) $(MODULE_ELF)
	rm -f *.sock *.log *.trs

distclean: clean
	rm -rf /tmp/qtest-*

indent:
	indent -gnu include/*.h src/*.c tests/*.c

help:
	@echo "Available targets:"
	@echo "  all       - Build harness library, test runner and bootstrap payload"
	@echo "  check     - Run the test suite (supports -jN and SANITIZE=1)"
	@echo "  clean     - Remove build artifacts and temporary files"
	@echo "  distclean - Deep clean including temporary directories"
	@echo "  indent    - Format all C source files with GNU indent"
