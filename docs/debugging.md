# Debugging with GDB over ST-LINK

The Nucleo's built-in ST-LINK is a debug probe as well as a programmer.
PlatformIO drives it through OpenOCD and GDB, so there is nothing extra to
install.

## In VS Code (easiest)

1. Open the repo folder (the one with `platformio.ini`) in VS Code with the
   PlatformIO extension installed.
2. Plug in the Nucleo.
3. In the blue PlatformIO bar at the bottom, click the environment name
   and pick **env:nucleo_l432kc_debug**.
4. Open the **Run and Debug** panel (Ctrl+Shift+D) and choose
   **PIO Debug**, then press F5. PlatformIO builds the debug version
   (`-Og`, full debug info), flashes it, and stops at `main()`.
5. Set breakpoints by clicking left of a line number. F5 continues, F10
   steps over, F11 steps in.
6. The left panel shows variables, the call stack, and **Peripherals**,
   which lists the STM32 registers by name (from the SVD file). Use it to
   check, for example, that `TIM6 → ARR` is 49.

## From the command line

```
pio debug -e nucleo_l432kc_debug --interface=gdb -- -x .pioinit
```

`env:nucleo_l432kc_debug` in `platformio.ini` is the normal firmware built
with `-Og -g3` and set to stop at `main()`. PlatformIO starts OpenOCD for
the onboard ST-LINK and connects GDB to it; there's nothing else to install.

Useful GDB commands once it stops:

```
break fatal_error          # stop on any fatal error, then 'bt' to see who called it
break HardFault_Handler
continue
bt                         # call stack
info locals
p g_stats                  # all the counters at once
p/x hi2c1.ErrorCode        # HAL I2C error bits (AF = NACK, BERR, ARLO, TIMEOUT)
x/4wx 0x40001000           # raw TIM6 registers (CR1, CR2, reserved, DIER), RM0394 memory map
monitor reset halt         # reset the chip and stop at the first instruction
```

## Breakpoints worth setting first

| Breakpoint | Why |
|---|---|
| `fatal_error` | Every unrecoverable error ends here. `bt` shows the cause |
| `HardFault_Handler` | Bad pointer, stack overflow into bad memory, etc. |
| `vApplicationStackOverflowHook` | A task overran its stack. The `name` argument says which |
| `mpu6050_init` return value | If the IMU never comes up, step through and see which register write fails |

## Reading a HardFault

When the CPU faults, it pushes R0-R3, R12, LR, PC and xPSR onto the stack
of whatever was running. In `HardFault_Handler`:

```
p/x *(uint32_t *)0xE000ED28    # CFSR: which fault (bits documented in the Cortex-M4 generic user guide)
p/x *(uint32_t *)0xE000ED2C    # HFSR: bit 30 FORCED = a lower-level fault escalated
p/x *(uint32_t *)0xE000ED38    # BFAR: faulting address, if CFSR.BFARVALID is set
info registers sp
x/8wx $sp                      # stacked R0 R1 R2 R3 R12 LR PC xPSR (if MSP was in use)
info symbol <stacked PC>       # which function faulted
```

If the fault happened in a task, the frame is on the process stack, so use
`$psp` instead of `$sp`. Bit 2 of the LR value in the handler says which
(0 = MSP, 1 = PSP).

## FreeRTOS-specific checks

- **Stack usage:** `uxTaskGetStackHighWaterMark(handle)` returns the
  fewest free words a task has ever had. Below ~32 words is too tight.
  `configCHECK_FOR_STACK_OVERFLOW = 2` already catches most overflows.
- **Heap:** `xPortGetFreeHeapSize()` is printed at boot.
- **Task states:** with the scheduler stopped at a breakpoint, `p *pxCurrentTCB`
  shows the running task's name and stack pointer.

## Timing with the logic analyzer

The firmware drives two debug pins (wiring in [wiring.md](wiring.md)):

- **PA8 (D9):** high during each IMU read. The period between rising
  edges should be 5.000 ms. Any 10 ms gap is a missed sample.
- **PA11 (D10):** high during each SD write. If a long PA11 pulse lines up
  with a gap on PA8, a write is blocking sampling.

In PulseView: sample at ≥ 1 MHz, add a "Timing" decoder on CH0 to get the
period of every pulse, and look at the minimum and maximum.

## Checking I2C transactions with a logic analyzer (PulseView)

Use this when the IMU misbehaves: no ACK, wrong WHO_AM_I, or numbers that
look like the wrong register. It shows exactly which bytes went on the bus,
so you can compare them byte by byte with the register map.

**Hook-up** (any cheap 8-channel "Saleae-compatible" analyzer):

| Analyzer | Connect to | Notes |
|---|---|---|
| CH0 | SCL, Nucleo D1 (PA9) | Clip onto the jumper at the GY-521, or a breadboard hole in the same row |
| CH1 | SDA, Nucleo D0 (PA10) | |
| GND | Nucleo GND | Always. Without it the capture is noise |

**PulseView setup:**

1. Install PulseView (sigrok). For the Saleae clones pick the
   **fx2lafw** driver. On Windows, sigrok's Zadig tool installs the USB
   driver the first time.
2. Sample rate **8 MHz** or more (at least 10x the 400 kHz bus), and
   1-2 M samples, which is a few hundred ms.
3. Trigger: falling edge on CH1 (SDA). The first falling SDA while SCL
   is high is the START of a transaction.
4. Add the **I²C** decoder: SCL = CH0, SDA = CH1. Set "address format"
   to *unshifted* so it shows 0x68 instead of 0xD0.
5. Press the Nucleo's reset button just after starting the capture, to
   catch the init sequence.

**What a correct capture looks like** (addresses as 7-bit):

| Transaction | Bytes on the bus | Source |
|---|---|---|
| WHO_AM_I read | START, `0x68 W` ACK, `0x75` ACK, repeated START, `0x68 R` ACK, `0x68` NACK, STOP | first thing `mpu6050_init` does |
| Reset | START, `0x68 W` ACK, `0x6B` ACK, `0x80` ACK, STOP | |
| Wake | START, `0x68 W` ACK, `0x6B` ACK, `0x01` ACK, STOP | ~100 ms after the reset |
| Config writes | `0x1A`=`0x03`, `0x19`=`0x04`, `0x1B`=`0x18`, `0x1C`=`0x18`, each its own transaction | same order as `test_init_writes_registers_in_expected_order` |
| Each sample | START, `0x68 W`, `0x3B`, repeated START, `0x68 R`, 14 data bytes (ACK on all but the last, NACK on the last), STOP | every 5 ms, inside a PA8 pulse |

The init sequence should match the unit test's expected list exactly. If
the test passes but the bus shows something different, the bug is in the
HAL adapter (`firmware/src/i2c_bus.c`) or the wiring, not the driver.

**Reading the failures:**

- **Address NACKed** (`0x68 W` then NACK): nothing at 0x68. Check power,
  and AD0: if it's high, the address is 0x69.
- **SCL or SDA never goes high:** missing pull-ups, or a line shorted to
  GND.
- **Wrong register in the data:** the register byte after `0x68 W` is the
  one being read or written. Compare it with `mpu6050_regs.h` and the
  register map.
- **Stretched or stuck clock:** SCL held low by the sensor for a long time.
  Note how long; the HAL times out after 5 ms (`I2C_TIMEOUT_MS`).

Save captures you use as evidence in `docs/captures/` (PulseView's `.sr`
file plus a screenshot), and write what they showed in
[bug-log.md](bug-log.md).

## Debug vs release builds

`pio debug` builds with optimisation mostly off, so timing is slower than
the normal `pio run` build. Do timing measurements on the normal build.
