# Debugging with GDB over ST-LINK

The Nucleo's built-in ST-LINK is a debug probe as well as a programmer.
PlatformIO drives it through OpenOCD and GDB, so there is nothing extra to
install.

## In VS Code (easiest)

1. Open the repo folder (the one with `platformio.ini`) in VS Code with the
   PlatformIO extension installed.
2. Plug in the Nucleo.
3. Open the **Run and Debug** panel (Ctrl+Shift+D) and choose
   **PIO Debug**, then press F5. PlatformIO builds a debug version
   (less optimisation, full debug info), flashes it, and stops at `main()`.
4. Set breakpoints by clicking left of a line number. F5 continues, F10
   steps over, F11 steps in.
5. The left panel shows variables, the call stack, and **Peripherals**,
   which lists the STM32 registers by name (from the SVD file). Use it to
   check, for example, that `TIM6 → ARR` is 49.

## From the command line

```
pio debug -e nucleo_l432kc --interface=gdb -- -x .pioinit
```

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

## Debug vs release builds

`pio debug` builds with optimisation mostly off, so timing is slower than
the normal `pio run` build. Do timing measurements on the normal build.
