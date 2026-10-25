# gnumach-harness

Test harness suite for the **GNU Mach** microkernel (specifically targeting the **AArch64** architecture) leveraging the **QEMU libqtest** protocol.

## Prerequisites

* C compiler with C99 support (such as GCC)
* GNU Make
* QEMU system emulator for AArch64 (`qemu-system-aarch64`)
* AArch64 cross-compilation toolchain (`aarch64-linux-gnu-gcc`, `aarch64-linux-gnu-ld`)
* GLib 2.0 development headers and library (`libglib-2.0`)
* GNU indent (`indent`) for code formatting

## Repository Structure

```
gnumach-harness/
├── COPYING                 <- GNU General Public License v3
├── Makefile                <- GNU Make build system (all, check, clean, indent)
├── CONTEXT.md              <- Knowledge base index
├── PLAN.md                 <- Strategic project roadmap
├── .agents/                <- Agent development guidelines (c99.md)
├── .contexts/              <- Architectural Decision Records (ADRs) and notes
├── .plans/                 <- Detailed operational roadmap phases
├── include/
│   └── qtest.h             <- Public QTest client API header and assertions
├── src/
│   ├── qtest_client.c      <- QTest socket protocol driver
│   └── qtest_process.c     <- QEMU process launcher and session isolation
├── tests/
│   ├── test_bootstrap_module.c <- Initial deterministic test case
│   └── modules/
│       ├── payload_example.S   <- Standalone AArch64 deterministic micro-payload
│       └── linker.ld           <- Linker script for the test module
└── gnumach/                <- Git submodule (aarch64 branch)
```

## Usage

### Building and Running Tests

```bash
# Build and execute the test suite
make check

# Run tests with parallel execution
make check -j4

# Run tests with AddressSanitizer and UndefinedBehaviorSanitizer enabled
make check SANITIZE=1

# Format C source code according to GNU coding standards
make indent
```

### Running with a Specific GNU Mach Binary

To execute tests against a specific precompiled GNU Mach kernel binary:

```bash
GNUMACH_BIN=/path/to/gnumach.elf make check
```

## License

This project is licensed under the terms of the **GNU General Public License version 3 or later** (GPL-3.0-or-later). See the [COPYING](COPYING) file for full details.
