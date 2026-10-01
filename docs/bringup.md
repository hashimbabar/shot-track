# Phase 1, step 1: bring-up

Goal: the green LED blinks and the board prints `hello` to a serial terminal
on my laptop.

## Why start here

Before writing the IMU driver I want to know the whole chain works: the
toolchain builds, the debugger flashes the chip, the clock is configured, and I
have a way to print. Without a print channel, every later bug is guesswork. If
a blink and a print work, any later problem is in my new code, not the setup.

## Board

These notes assume the **Nucleo-L432KC**. If I use the F401RE instead, the
differences are in the table at the bottom.

On the L432KC:
- User LED LD3 (green) is on **PB3**.
- **USART2** (TX on PA2, RX on PA15) is wired to the ST-LINK. The ST-LINK
  shows up on the laptop as a USB serial port, so no extra USB-to-serial
  adapter is needed.

## 1. Create the project in STM32CubeIDE

1. File > New > STM32 Project.
2. Go to the **Board Selector** tab, search for `NUCLEO-L432KC` and select it.
3. Project name: `shottrack`. Location: untick "use default" and point it at
   `firmware/stm32/` in this repo, so the project lives in git.
4. Targeted language: C. Project type: STM32Cube.
5. When it asks "Initialize all peripherals with their default mode?", say
   **Yes**. This gives LD3 on PB3 and USART2 at 115200 8N1 already set up.
6. In the `.ioc` view, check:
   - Connectivity > USART2: Asynchronous, 115200 baud, 8 bits, no parity,
     1 stop bit.
   - Pinout: PB3 is GPIO_Output with the label `LD3`.
   - Leave the clock at what the board selector picked for now.
7. Save the `.ioc` (Ctrl+S). It asks to generate code: Yes.

## 2. Add printf support

Copy `firmware/stm32/retarget.c` into `firmware/stm32/shottrack/Core/Src/`.
It replaces `_write()`, the low-level function that `printf()` calls to send
its bytes, so printf output goes out through USART2.

## 3. Edit main.c

Only write between the `/* USER CODE BEGIN */` and `/* USER CODE END */`
comments. CubeMX keeps those blocks when it regenerates the file and
overwrites everything else.

Includes:

```c
/* USER CODE BEGIN Includes */
#include <stdio.h>
/* USER CODE END Includes */
```

After the peripherals are set up, print once:

```c
/* USER CODE BEGIN 2 */
printf("hello from shottrack\r\n");
/* USER CODE END 2 */
```

Inside the `while (1)` loop:

```c
/* USER CODE BEGIN 3 */
HAL_GPIO_TogglePin(LD3_GPIO_Port, LD3_Pin);
printf("tick %lu ms\r\n", HAL_GetTick());
HAL_Delay(500);
/* USER CODE END 3 */
```

Notes:
- `\r\n` matters for two reasons. Newlib buffers stdout until it sees a `\n`,
  so without one nothing appears. Many terminals also need the `\r` to go back
  to the start of the line.
- `HAL_GetTick()` counts milliseconds since reset, using the SysTick interrupt.
  Each printed tick should be about 500 ms after the last one. That is a free
  check that the clock is set up right.
- `%lu` because `HAL_GetTick()` returns `uint32_t`, which is `unsigned long`
  on this toolchain.

## 4. Build, flash, look

1. Build (hammer icon). Expect 0 errors.
2. Run > Debug As > STM32 C/C++ Application, then Resume (F8). Or just Run.
3. Open a serial terminal on the ST-LINK port at **115200 8N1**:
   - CubeIDE: Window > Show View > Console, then open a Command Shell Console
     of type Serial Port.
   - Or PuTTY (Windows, look up the COM number in Device Manager), or
     `screen /dev/ttyACM0 115200` on Linux, or
     `screen /dev/tty.usbmodem* 115200` on macOS.
4. Press the black reset button on the Nucleo so the `hello` line prints
   after the terminal is already open.

Expected:
- LD3 blinks, on for 0.5 s and off for 0.5 s.
- The terminal shows `hello from shottrack`, then a new `tick` line every
  ~500 ms.

## If it doesn't work

Change one thing at a time and confirm each guess with a measurement.

| Symptom | Likely cause | How to check |
|---|---|---|
| Won't flash, "no ST-LINK detected" | charge-only USB cable, or the ST-LINK firmware needs an update | try another cable; CubeIDE offers to update the ST-LINK firmware |
| LED blinks, terminal blank | wrong COM port, wrong baud, or no `\n` | try the other ports; check 115200; check the string ends in `\r\n` |
| Terminal shows garbage | baud mismatch or wrong clock setting | the baud on both ends must match; if it does, the UART clock is wrong |
| Prints `hello` once and stops | stuck in `HAL_Delay`, so SysTick isn't running | pause in the debugger and see where the PC is |
| Nothing at all | stuck in `Error_Handler()` or a HardFault | pause in the debugger; the call stack shows where |

## Results

TODO: fill in after running it on the board (date, what printed, anything
that went wrong).

## F401RE differences

| | L432KC | F401RE |
|---|---|---|
| User LED | LD3 on PB3 | LD2 on PA5 (use `LD2_Pin` / `LD2_GPIO_Port`) |
| VCP UART | USART2, PA2/PA15 | USART2, PA2/PA3 |

## Heads-up for step 2

On the L432KC, solder bridges SB16 and SB18 connect a few pins together
(including PB6/PB7 to PA6/PA5) for Arduino Nano compatibility. PB6/PB7 are
I2C1, which is the obvious choice for the MPU-6050. Check the Nucleo-32 user
manual (UM1956) before wiring the IMU.
