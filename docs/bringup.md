# Phase 1, step 1: bring-up

Goal: the green LED blinks and the board prints to a serial monitor on my
laptop, with FreeRTOS running two tasks.

## What the firmware does

1. Sets the clock to 80 MHz (internal MSI oscillator into the PLL, trimmed by
   the 32.768 kHz crystal on the Nucleo).
2. Sets up the green LED (LD3, pin PB3) and USART2 at 115200 baud. USART2 goes
   to the ST-LINK, which shows up on the laptop as a USB serial port.
3. Prints a hello message, the clock speed, and whether the crystal started.
4. Starts FreeRTOS with two tasks:
   - `blink` toggles the LED every 500 ms.
   - `heartbeat` prints the uptime and the free FreeRTOS heap once a second.

If anything fatal happens (a fault, a stack overflow, running out of heap),
the board prints `FATAL: <reason>` and blinks the LED fast forever.

## Files, in reading order

| File | What it is |
|---|---|
| `firmware/stm32/platformio.ini` | Project settings: board, framework (ST's HAL), upload tool, serial speed |
| `firmware/stm32/fpu.py` | Turns on the hardware floating point unit for every file |
| `firmware/stm32/src/main.c` | Startup order, the two tasks, FreeRTOS error hooks |
| `firmware/stm32/include/board.h`, `src/board.c` | Clock tree, LED, UART, `fatal_error()` |
| `firmware/stm32/src/stm32l4xx_hal_msp.c` | Pin setup for the UART (which pin, which alternate function) |
| `firmware/stm32/src/stm32l4xx_it.c` | Interrupt handlers; SysTick feeds both the HAL and FreeRTOS |
| `firmware/stm32/src/retarget.c` | Makes `printf` go out of the UART |
| `firmware/stm32/include/FreeRTOSConfig.h` | Kernel settings: tick rate, heap size, interrupt priorities |
| `firmware/stm32/include/stm32l4xx_hal_conf.h` | Which HAL modules are compiled in |
| `firmware/stm32/lib/FreeRTOS/` | The FreeRTOS kernel, copied unchanged from V11.1.0 |

## Why it's built this way

- **PlatformIO instead of CubeIDE.** Every file is plain text in the repo and
  builds from one command (`pio run`), so CI can check the build on every
  push. It still uses ST's HAL drivers, the same ones CubeIDE would.
- **FreeRTOS copied into the repo.** The build doesn't depend on a package
  someone could change later. Only the files for this chip are kept: the
  Cortex-M4F port and the `heap_4` allocator.
- **One SysTick, two users.** The HAL needs a 1 ms tick for `HAL_Delay()`
  and its timeouts, and FreeRTOS needs a tick to schedule tasks. Both run at
  1 kHz, so the one SysTick interrupt calls both. FreeRTOS only gets called
  after the scheduler has started.
- **Interrupt priorities.** On Cortex-M a lower number is more urgent. The
  kernel runs at the least urgent level (15). Any interrupt that talks to
  FreeRTOS must be at 5 or above, so the kernel can briefly mask it inside
  critical sections. This matters in step 3, when the 200 Hz timer interrupt
  wakes a task.
- **`xTaskDelayUntil` instead of `vTaskDelay`.** It wakes at fixed steps from
  the last wake-up, so the period doesn't drift. The same idea becomes
  important for steady sampling.
- **Fail loudly.** Every error path ends in `fatal_error()`, which prints the
  reason and blinks the LED fast. A silent hang would be much harder to debug
  from a photo or a pasted log.

## Build check

Compiled by GitHub Actions on every push (`.github/workflows/ci.yml`), with
PlatformIO's ststm32 platform 20.0.0. The CI job also checks that the
firmware really was built for the hardware FPU. First CI build (run
36919115612): 17,112 of 262,144 bytes of flash and 17,044 of 65,536 bytes of
RAM. Most of the RAM is the 16 KB FreeRTOS heap.

## My checklist

No wiring for this step. Just the Nucleo and a USB cable.

### One-time setup

1. Install VS Code from https://code.visualstudio.com.
2. In VS Code, click the Extensions icon on the left (four squares), search
   for **PlatformIO IDE**, and click Install. Wait until the bottom-right
   corner says it's finished, then restart VS Code if it asks.
3. Get the code: press `Ctrl+Shift+P` (`Cmd+Shift+P` on a Mac), type
   **Git: Clone**, paste `https://github.com/hashimbabar/shot-track`, and
   pick a folder to save it in.
4. Switch to the working branch: click the branch name in the bottom-left
   corner of VS Code and pick `origin/claude/new-session-razeb4`. (Skip this
   once the branch is merged into main.)
5. Open the project: **File > Open Folder** and pick
   `shot-track/firmware/stm32`. It has to be this folder, the one that
   contains `platformio.ini`.
   - The first time, PlatformIO downloads the ARM compiler and ST's drivers.
     This takes a few minutes. Wait for the progress messages in the
     bottom-right to stop.
6. Drivers:
   - **Windows:** usually installed automatically when you plug the board
     in. If upload later says no ST-LINK was found, install ST's
     "STSW-LINK009" driver from st.com and replug the board.
   - **macOS:** nothing to install.
   - **Linux:** install PlatformIO's udev rules (search "PlatformIO 99-platformio-udev.rules"),
     then unplug and replug the board.

### Every time

7. Plug the Nucleo into the laptop with a USB cable that carries data (some
   cables only charge). The power LED should light up.
8. **Build:** click the checkmark icon in the blue bar at the bottom of VS
   Code. The terminal should end with `[SUCCESS]`.
9. **Upload:** click the right-arrow icon next to it. The terminal should
   end with `[SUCCESS]`.
10. **Serial monitor:** click the plug icon in the same bar. A terminal opens
    at 115200 baud.
11. Press the black reset button on the Nucleo so the start of the output
    prints while the monitor is open.

### What I should see

- The green LED (LD3) blinks: on for half a second, off for half a second.
- In the monitor:

```
hello from shottrack
system clock: 80000000 Hz
32 kHz crystal: ok
starting scheduler
beat 0, uptime 0 ms, free heap <number> bytes
beat 1, uptime 1000 ms, free heap <number> bytes
beat 2, uptime 2000 ms, free heap <number> bytes
...
```

- A new `beat` line every second, with `uptime` going up by exactly 1000.
- The free heap number should stay the same on every line.

Before uploading again, close the monitor (trash-can icon on its terminal
tab) so it isn't holding the serial port.

### What to paste back

- The first 10 or so lines of the monitor output, copied as text.
- Whether the LED blinks slowly (working), blinks fast (fatal error), or
  doesn't blink at all.

### If something goes wrong, paste this

| Problem | Paste or photograph |
|---|---|
| Build fails | The whole terminal output from the build |
| Upload fails | The whole terminal output from the upload, and your OS |
| Monitor is blank | Which serial ports show up (run `pio device list` in the PlatformIO terminal), and whether the LED blinks |
| Garbage characters | A screenshot of the monitor |
| LED blinks fast | Any line starting with `FATAL:` |

## Results

TODO: fill in after it runs on the board (date, output, anything that went
wrong).

## Heads-up for step 2

On the L432KC, solder bridges SB16 and SB18 connect PB6/PB7 to PA6/PA5 for
Arduino Nano compatibility. PB6/PB7 are I2C1, which is the obvious choice for
the MPU-6050. I'll check the Nucleo-32 user manual (UM1956) and pick pins
that avoid this before giving you the wiring table.
