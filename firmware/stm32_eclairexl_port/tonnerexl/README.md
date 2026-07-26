# TonnereXL port — first slice

Port of the EclaireXL ZPU firmware to STM32F407 (ThreadX/USBX/FileX), FPGA
accessed over FSMC. This tarball is the first, host-provable slice: the FPGA
HAL, the 16-bit register refactor, the SimpleFile-on-FileX adapter, the
four-thread skeleton, the interface contract, and a host test suite.

## IMPORTANT: there are TWO separate builds. Do not overlay this on your project.

The port sources (`Platform/`, `Filesystem/`, `App/`) are shared by both builds.
Everything else differs. **Do not extract this over your STM32Cube project** —
the `host/` build and its dependencies are Linux-only and must stay separate.

```
Platform/     fpga_bus HAL + 16-bit register map   (shared)
Filesystem/   SimpleFile/SimpleDir on FileX         (shared)
App/          four-thread ThreadX skeleton          (shared)
Tests/        host unit tests                        (host build only)
host/         host (Linux) test build CMake          (host build only)
docs/         interface contract (review this)
```

## Build 1 — STM32 firmware (on-target)

Uses YOUR project's ST 6.1.10 ThreadX/USBX/FileX. This tarball's `host/` dir and
FetchContent play NO part here.

Add to your existing STM32Cube CMake build:

1. Source files:
   - `Platform/fpga_bus_stm32.c`   (NOT fpga_bus_fake.c) — native 16-bit HAL
   - `Filesystem/simplefile_filex.c`
   - `App/app_threads.c`, `App/app_service_steps.c`
2. Include dirs: `Platform/`, `Filesystem/`, `App/`
3. Compile define: `FPGA_BUS_STM32`
4. Ensure the FileX generic port include is on the path (your project already
   has it): `Middlewares/ST/filex/ports/generic/inc`

Verified: all four sources compile clean for cortex-m4 (`-Wall -Wextra -Werror`)
against ST 6.1.10 headers. `fpga_bus_stm32.c` assumes the FSMC is already
initialised (as your RAM test does) and does raw 16-bit accesses at
`FPGA_BUS_BASE` (default `0x60000000`, one constant in `fpga_bus_map.h`).

Not yet wired (compile-clean, TODO markers only): CD/WP/IRQ GPIO pins, and the
SD lifecycle / menu / drive logic (those service_step functions are stubs).

## Build 2 — host unit tests (Linux)

Self-contained. Pulls upstream Eclipse ThreadX + FileX (Linux ports) via
FetchContent — no manual `third_party/` step. The `host_tests` binary boots the
real ThreadX scheduler and runs FileX on a RAM disk; the FPGA is the in-memory
fake backend (`FPGA_BUS_FAKE`).

```
cd host
cmake -B build -S .
cmake --build build -j
./build/host_tests            # or: ctest --test-dir build --output-on-failure
```

Built with `-Wall -Wextra -Werror` and ASan/UBSan on by default
(`-DTONNEREXL_SANITIZE=OFF` to disable). Expected: `86 checks, 0 failed`.

Note: the ThreadX Linux port's stack builder triggers a benign UBSan
"misaligned store" note at startup — it's in upstream's `tx_thread_stack_build.c`,
not port code, and does not affect results.

## Next

Review `docs/fpga_interface.md` — it's the STM32<->FPGA contract your FSMC
adaptor RTL matches, and it ends with a numbered TODO(mark) list of the
decisions the RTL pins down (FSMC offsets, read-latch vs stable, commit-on-high,
IRQ pin + status register, identity magic). Firmware uses documented defaults
until each is decided, each held in one named constant.
