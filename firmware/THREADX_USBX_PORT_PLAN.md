# TonnereXL ThreadX/USBX firmware port plan

## 1. Goal and boundaries

Create an STM32F407 firmware for TonnereXL that preserves the useful behaviour
of `eclairexl_firmware_reference`, but runs it as a ThreadX application and uses
USBX for USB host support. The ZPU will not execute firmware. Instead, the STM32
will access FPGA control/status registers and Atari memory/DMA windows through
FSMC.

This document is an implementation plan, not a commitment to preserve the ZPU
source layout or its low-level drivers. The first usable target is:

* one USB HID keyboard (with hub support retained);
* SD-card hot insertion/removal and a FAT filesystem;
* the AtariXL menu and file selector;
* four emulated SIO drives, initially ATR, then ATX;
* ROM/cartridge loading and core reset/configuration through the FPGA interface.

Freeze/save-state, mouse/joystick support, firmware/core update, video clock
configuration, and other reference-firmware extras should follow only after the
above path is stable.

## 2. Starting points and reuse policy

Use `stm32testusbproject` as the STM32/CubeMX build and board-support base. It
already configures ThreadX, the USBX HID host, STM32 OTG FS, SDIO, FSMC, and the
other board peripherals. Keep generated CubeMX files generated: application
code should live in new, clearly separated directories and CubeMX user-code
sections, with explicit additions in `CMakeLists.txt`.

Use `eclairexl_firmware_reference/firmware_eclairexl` as a behavioural
reference and port these higher-level components selectively:

* `menu.c`, `a800/mainmenu.c`, `fileselector.c`, and `actions.c` for menu flow;
* `atari_drive_emulator.c`, `atx.c`, and `atx_eclaire.c` for SIO behaviour;
* `cartridge.c` and ROM-loading portions of `main.h`;
* the `SimpleFile`/`SimpleDirEntry` interfaces as the maintained, portable file
  API used by the menu, image, and drive code;
* keyboard mapping and FPGA register bit meanings, rather than the old USB
  stack or ZPU pointer definitions.

Do **not** port the ZPU startup/linker code, polled ZPU USB stack, direct-SPI SD
driver, busy-wait pause register, native test shims, or hardware-specific
memory pointers unchanged. Also avoid copying the monolithic `main.h`: extract
its functions into ordinary `.c` modules with explicit ownership and headers.

Before copying reference code, record its applicable licence and provenance in
the new source tree.

## 3. Resolve the STM32/FPGA contract first

Firmware development should not begin against an informal FSMC map. Add one
versioned interface specification shared by RTL and firmware, then implement a
small `fpga_bus` HAL. The specification must define:

1. FSMC bank/base address, 16-bit bus byte addressing, address-line mapping,
   byte enables, read/write timing, `NWAIT` use, and required memory barriers.
2. A read-only interface magic value, semantic version, feature bits, and FPGA
   build identifier so incompatible firmware fails safely.
3. The ZPU-compatible input/output register offsets, reset values, access
   widths, read/write side effects, and which legacy fields are unsupported.
4. Atari address-space access: byte/word ordering, live versus paused access,
   and arbitration/coherency rules while the 6502 is running.
5. The STM32-to-Atari DMA engine: source/destination address spaces, length,
   direction, start/busy/done/error flags, alignment, maximum transfer, timeout,
   interrupt or polling behaviour, and abort/reset behaviour.
6. The SIO interface: command indication, RX/TX FIFO semantics, FIFO levels,
   divisor/baud control, framing errors, and any FPGA interrupt line to the MCU.
7. Atomicity rules for shared read-modify-write registers. Prefer dedicated
   set/clear registers, or serialize all updates in the firmware FPGA HAL.

The HAL should expose typed operations such as `fpga_reg_read/write`, core
pause/reset/configure, bounded Atari memory copies, DMA submit/wait/abort, and
SIO FIFO access. Higher layers must not contain raw FSMC pointers. Add startup
tests for interface identity, walking register bits, byte lanes, memory
boundaries, DMA lengths/directions, timeout recovery, and concurrent FPGA/core
access.

## 4. Recommended ThreadX architecture

Use four **application** threads. USBX also owns internal system threads; they
do not count as a fifth application responsibility. Interrupt service routines
should only acknowledge hardware, capture minimal state, and signal a ThreadX
event/semaphore.

| Application thread | Initial priority | Responsibility | Blocking model |
| --- | ---: | --- | --- |
| Drive/SIO | 5 (highest) | Service FPGA SIO events, parse commands, move sectors through the FPGA DMA/FIFOs, maintain D1-D4 state and timing | Wait on SIO event/IRQ with a short timeout; never poll in a tight loop |
| USB input | 10 | Consume USBX HID keyboard events, maintain key state, translate to Atari/menu events, handle attach/removal | Wait on USBX/semaphore; periodic wake only for key repeat/stuck-key recovery |
| SD lifecycle | 12 | Debounce card-detect GPIO, initialise/deinitialise SDIO, mount/unmount, publish media generation/state | Wait on GPIO event plus debounce timer; perform mount transitions outside ISR |
| Menu | 20 (lowest) | Render UI, consume input queue, browse files, change drive/core configuration | Wait on input/state queues; redraw only when dirty |

These are starting priorities to validate with ThreadX execution profiling. Use
preemption thresholds only if measurements demonstrate a need. Give every
thread a named, statically budgeted stack and enable ThreadX stack checking in
debug builds; replace the sample's ad-hoc 1 KiB keyboard stack only after
measuring high-water marks.

### Inter-thread communication

* `input_queue`: USB input thread produces semantic press/release/repeat events;
  menu consumes menu controls, while the keyboard mapper independently updates
  FPGA key state. Do not pass USBX instance pointers across threads.
* `media_events`: SD thread publishes `MOUNTED(generation)`, `REMOVING`,
  `UNMOUNTED`, and `ERROR` events to menu and drive threads.
* `drive_control_queue`: menu requests mount/eject/read-only changes; drive
  thread applies them at a safe SIO boundary and reports completion. The menu
  must not mutate live drive objects.
* `core_control`: centralise FPGA output-register changes in a service/API so
  keyboard injection, menu configuration, and drive turbo settings cannot lose
  one another's bit updates.

## 5. Filesystem and removal model

Maintain `SimpleFile` and `SimpleDirEntry` as the only filesystem API visible to
portable firmware code. On the STM32 they will be implemented **directly on top
of FileX** and the STM32 SDIO block-media driver; do not add FatFs or retain the
Petit FatFs backend. The adapter must use independent `FX_FILE` objects rather
than the reference implementation's global current-file assumptions, preserve
the existing status/seek/read/write/directory semantics where useful, and
document any deliberate API corrections. This keeps FileX types and calls out
of the menu, image parsers, and drive protocol code.

Create one ThreadX mutex with priority inheritance for filesystem **and media
lifecycle** operations. Every open/close/read/write/seek/directory/mount/unmount
operation must use the wrapper that acquires this mutex; callers must never
call the filesystem library directly. Keep critical sections bounded: directory
enumeration and large ROM copies should release between batches, while an
individual file seek plus read needed for one drive sector remains atomic.

Model media explicitly as `ABSENT -> DEBOUNCING -> INITIALISING -> MOUNTED ->
REMOVING/ERROR -> ABSENT`. Each mount increments a generation counter. Every
file/drive handle records that generation and fails once it becomes stale.
On removal:

1. the card-detect ISR signals the SD thread;
2. after debounce, the SD thread publishes `REMOVING` and rejects new opens;
3. the drive thread stops accepting filesystem-backed work, closes/invalidates
   images under the mutex, and acknowledges quiescence;
4. the menu discards directory caches and acknowledges;
5. the SD thread unmounts/deinitialises and publishes `UNMOUNTED`.

All waits need finite timeouts. A physically removed card may cause an active
SDIO transaction to fail; removal recovery must reset SDIO and must never leave
the filesystem mutex permanently owned. On insertion, mount read-only first if
the write-protect signal is active. Define dirty-write policy before enabling
writable ATRs (sync per completed write command initially, then optimise only
with fault testing).

## 6. Work phases and acceptance gates

### Phase 0 — inventory and baseline

* Reproduce a clean Debug and Release build of `stm32testusbproject` and capture
  flash/RAM/map usage.
* On hardware, record USB keyboard attach/type/remove behaviour, current board
  pin polarities (especially card detect/write protect), clocks, and FSMC
  timing.
* Make UART logging thread-safe and add assertions/fatal diagnostics that do not
  block the drive thread.

**Gate:** the sample runs for an hour through repeated keyboard/hub reconnects,
with measured thread and USBX memory headroom.

### Phase 1 — FPGA bus bring-up

* Finalise the contract in section 3 and implement the FSMC FPGA HAL.
* Validate interface/version detection, register reads and safe output writes.
* Pause/reset the Atari core, read/write test memory, and exercise DMA for odd
  and even lengths plus timeouts.
* Provide a host-build fake FPGA backend so register and core-control logic can
  be unit tested without hardware.

**Gate:** automated loopback/memory tests pass repeatedly and an interface
version mismatch leaves the Atari core in a defined safe state.

### Phase 2 — ThreadX skeleton and input

* Create static application objects, queues, event flags, mutex, timers, and the
  four threads in `App_ThreadX_Init`, checking every return status.
* Refactor the USBX callback to identify the keyboard client correctly and only
  publish attach/removal state; let the USB thread wait for and read HID events.
* Port keyboard translation and send matrix/key injection through the FPGA HAL.
* Add key release on USB removal and route hotkeys/menu controls through
  `input_queue`.

**Gate:** normal typing, modifiers, rollover limitations, hotkeys, repeat, hub
use, and unplug-while-key-down behave correctly without busy polling.

### Phase 3 — SD and filesystem service

* Enable and validate SDIO, card-detect EXTI, debounce, write protect, FileX,
  and its SDIO media driver.
* Implement the state machine, generation-tagged handles, mutex wrapper, and
  `SimpleFile` compatibility adapter.
* Add read/seek/write, directory, corruption/error, insertion/removal, and
  throughput tests. Test removal during each operation and while waiting for the
  mutex.

**Gate:** 1,000 insertion/removal cycles (including deliberately hostile
removal) complete without deadlock, stale-handle reuse, leaked objects, or a
required MCU reset.

### Phase 4 — minimal menu and ROM boot

* First render a diagnostic menu through FPGA/Atari memory access.
* Port menu primitives and file selector without ZPU globals or busy waits.
* Load required ROMs through bounded DMA transfers, then implement core
  configuration, reset, pause, and resume.
* Add cartridge loading only after the base XL boot path is reliable.

**Gate:** a keyboard can select a ROM/disk from SD, cold boot the AtariXL, leave
and re-enter the menu repeatedly, and receive a clear `media removed` result at
any screen.

### Phase 5 — drive emulation

* Separate the reference drive code into protocol state machine, image backend,
  and hardware transport. Replace ZPU UART/timer accesses with the FPGA HAL and
  ThreadX timing.
* Bring up read-only ATR D1 first, then multiple drives, writes, enhanced/turbo
  modes, and ATX timing/protection behaviour.
* Preallocate drive buffers and objects; do not allocate heap or log
  synchronously in the SIO service path. Measure command latency and mutex hold
  time. If menu directory work causes missed SIO deadlines, move from direct
  mutex competition to a dedicated storage-request owner while retaining the
  same filesystem API.

**Gate:** boot and stress a disk exerciser on D1-D4, verify reads/writes against
image hashes, meet normal and turbo SIO deadlines under simultaneous menu/USB
activity, and recover cleanly from card removal mid-command.

### Phase 6 — hardening and optional features

* Run long-duration, malformed-image, SD fault, USB hotplug, queue saturation,
  DMA timeout, and power-loss-on-write tests.
* Record worst-case thread stack, byte-pool, queue depth, SIO latency, filesystem
  mutex wait/hold time, and SD throughput; turn these into documented budgets.
* Add watchdog supervision based on per-thread heartbeats and a retained crash
  reason, without allowing a stalled menu to reset healthy drive servicing.
* Only then port freeze/save-state, joystick/mouse, ATX refinements, settings
  persistence, clock/video control, and update support one feature at a time.

**Gate:** the release soak/stress matrix passes with no unexplained resets,
deadlocks, missed normal-speed SIO deadlines, filesystem corruption, or memory
growth.

## 7. Proposed source layout

Keep third-party and generated code separate from the port:

```text
firmware/stm32testusbproject/
  App/
    app_init.c
    app_objects.c
    input/
    media/
    menu/
    drive/
    core/
  Platform/
    fpga_bus.c
    fpga_bus.h
    fpga_registers.h
    sd_block_device.c
  Filesystem/
    simplefile_filex.c
  Tests/
    host/
    hardware/
```

Keep HAL/ThreadX/FileX types below the `Platform`, RTOS, and `SimpleFile`
adaptation boundaries. This allows the menu, image parser, and SIO protocol
state machine to be compiled as native unit tests, following the useful
precedent of the reference firmware's native tests without carrying over its
fake globals.

## 8. Linux version and portability approach

Keep Linux as a supported target, but do not make portable firmware code depend
directly on ThreadX, USBX, FileX, STM32 HAL, or raw POSIX APIs. The common
approach is a ports-and-adapters (also called platform abstraction or hexagonal)
layout: share the product logic and provide a small platform implementation for
each operating environment.

Build two compositions from the same portable modules:

| Concern | STM32 composition | Linux composition |
| --- | --- | --- |
| Scheduling | ThreadX threads, queues, events, mutexes, timers | pthreads plus small queue/event/mutex/time adapters, or a deterministic single-process test runner |
| USB keyboard | USBX HID host adapter | Linux evdev or libusb adapter producing the same `InputEvent` values |
| Files | `SimpleFile`/`SimpleDirEntry` implemented directly with FileX | The same interfaces implemented with POSIX files/directories |
| FPGA | FSMC `fpga_bus` backend | simulated/in-memory backend initially; optional `/dev/uio` or other board backend later |
| Time/SIO wake-up | FPGA IRQ plus ThreadX time services | eventfd/condition/timer adapter and simulated SIO transport |
| Display/logging | Atari memory/DMA and queued embedded logger | existing curses/native display and normal Linux logging |

The shared code should include menu state and rendering commands, file
selection, keyboard-to-Atari mapping, cartridge/ROM rules, ATR/ATX parsing,
drive protocol state machines, settings validation, and core-control policy.
It should receive dependencies through explicit interfaces such as
`SimpleFile`, `FpgaBus`, `InputSource`, `Clock`, and `DriveTransport`. Avoid an
all-purpose compatibility header full of `#ifdef LINUX_BUILD`; select complete
adapter source files in CMake instead.

Do not try to make USBX manage real Linux USB devices. USBX is an embedded USB
stack and would still require a Linux-specific host-controller driver; Linux
already owns those devices. Likewise, the normal Linux build should use the
POSIX `SimpleFile` backend rather than mounting the host filesystem through
FileX. If useful later, a ThreadX host/simulation port can run targeted RTOS
integration tests, and FileX can be tested against a file-backed block device,
but neither should be the architecture of the Linux product build.

Keep the four logical services on both targets, but do not require their entry
functions to contain the business logic. Each thread should wait for an event,
call a bounded, portable `*_service_step()` function, then wait again. Linux
unit tests can call those step functions deterministically without threads;
Linux integration tests can run them in four pthreads to exercise ordering and
shutdown. RTOS primitives remain in thin launch/wait/queue adapters.

Add a native CMake target and CI job from the beginning. It should compile with
warnings-as-errors plus AddressSanitizer and UndefinedBehaviorSanitizer, run
unit tests with fake time/FPGA/input, and run scripted integration scenarios
for USB attach/remove, SD generation changes, menu/drive concurrency, malformed
images, and SIO timeouts. Keep the existing Linux reference implementation only
as an oracle while moving behaviour into the portable modules; do not create a
third copy of product logic.

## 9. Early decisions and information still required

Resolve these before Phase 1 is considered complete:

* final FPGA FSMC and DMA register specifications, and whether SIO/DMA completion
  has a dedicated STM32 interrupt;
* FSMC bus width/address wiring and FPGA/STM32 byte order;
* card-detect and write-protect pins, active levels, pull-ups, and EXTI routing;
* required FileX FAT variants/long-filename configuration, SDIO media-driver
  sector/cache requirements, and whether writable images/settings are required
  in the first release;
* Linux input backend (evdev or libusb), UI requirements, and whether Linux is
  simulation-only or must eventually control physical FPGA hardware;
* required day-one image types (ATR only or ATR+ATX), SIO turbo rates, drive
  count, ROM paths, cartridge types, and freeze support;
* who owns Atari display memory while the menu is visible, and whether menu
  drawing may use DMA safely while the core runs;
* acceptable response/timeout budgets for SIO, menu, mount, and USB input;
* firmware update/bootloader path and how incompatible FPGA/firmware versions
  are recovered.

The four-thread proposal is a sound starting point, provided the drive thread
has the highest priority, USBX's own threads are accounted for, SD removal is a
coordinated lifecycle rather than a boolean, and all filesystem entry points
share a priority-inheritance mutex with bounded hold times.
