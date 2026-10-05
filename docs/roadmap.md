# Roadmap

Three versions, each one a working device on its own. The detection code
(`lib/detect`) is written once in plain C and carries through all three; each
version only swaps the hardware layer underneath it.

## v1: breadboard logger (STM32 + FreeRTOS), in progress

NUCLEO-L432KC + MPU-6050 + microSD, strapped to the wrist with a USB battery.

| Step | Status |
|---|---|
| PlatformIO project, clock setup, FreeRTOS, serial console | Done, builds in CI |
| MPU-6050 driver from the register map, mocked-I2C unit tests | Done, tests pass in CI |
| 200 Hz TIM6 interrupt → sampling task, missed-sample counting | Done, builds in CI |
| Shot detection (filter, threshold, accel check, refractory) + unit tests | Done, tests pass in CI |
| SD logging task: FatFs over SPI, 512-byte blocks, button start/stop | Done, builds in CI |
| Simulator, replay and plotting tools on synthetic data | Done, replay check in CI |
| Hardware bring-up: blink, IMU WHO_AM_I, timer period, SD write | **Waiting on parts**, see [bringup.md](bringup.md) |
| Measure timing with the logic analyzer (sample period, SD stalls, drops) | Not started |
| Record real sessions: 20+ shots, dribbling-only, walking | Not started |
| Tune thresholds on real data | Not started |
| Court validation: 3-5 sessions with hand counts, results table | Not started |

**Done when:** I can wear it for a full shootaround and the count is close to
my hand count, with real results and plots in the README.

## v2: Bluetooth to my phone (nRF52840)

Adafruit Feather nRF52840 Sense (IMU on board) + small LiPo.

- Port the hardware layer to nRF Connect SDK (Zephyr). Learn devicetree,
  Kconfig, and Zephyr threads/message queues vs FreeRTOS.
- Use the IMU's FIFO, so the sensor buffers samples itself and drops stop
  depending on task timing.
- Reuse `lib/detect` unchanged and prove it with the same host tests.
- BLE: a custom GATT service with the live shot count, the last shot's
  release time and session start/stop. Notifications instead of polling.
- Phone side: a Web Bluetooth page (Chrome on Android/desktop) with the
  live count and a per-session chart.
- New metrics: release time (load to snap) and a consistency score (spread
  of peak wrist speed and release time across a session).
- Power: measure the current draw, estimate battery life, sleep when idle,
  wake on the IMU's motion interrupt.

**Done when:** I can glance at my phone mid-practice and see shots counting
live, and the battery lasts a session.

Rough parts cost: $50-70 CAD.

## v3: custom PCB + 3D-printed case

- KiCad board, about 30 × 30 mm: an nRF52840 module with integrated antenna
  (e.g. Raytac MDBT50Q), an IMU stocked at the assembler (LSM6DSOX /
  LSM6DS3TR-C / BMI270), LiPo charger (MCP73831 or similar), USB-C with
  5.1 kΩ CC resistors, low-Iq 3.3 V LDO, SPI NOR flash for offline sessions,
  button, LED, SWD pads and test points.
- Keep the module's antenna keep-out exactly as its datasheet says.
- ERC/DRC, then a review pass (power, decoupling, pull-ups, reset, footprint
  orientation) before ordering ~5 assembled boards from JLCPCB.
- Bring-up plan written before the boards arrive: rails on USB only →
  current draw → SWD → blink → IMU WHO_AM_I → flash → BLE → charging.
- Case designed around the board's STEP export: 20 mm spring-bar strap lugs,
  button cutout, LED window, USB-C opening. Printed at TMU or JLC3DP.
- LiPo safety: protected cells only, charge current set for the cell's
  capacity, never charged unattended at first, the case must not press on
  the cell.

**Done when:** it's on my wrist, looks like a device instead of a
breadboard, charges over USB-C, and counts shots live on my phone.

Rough parts cost: $80-150 CAD including fab and shipping.

## Later

- Shot type (jump shot / layup / free throw) from my own labelled sessions.
- Dribble counting as a second mode.
- Fatigue: does my release drift late in a session?
- Makes vs misses would need a second sensor on the rim or phone video. A
  wrist sensor can't see the ball go in.
