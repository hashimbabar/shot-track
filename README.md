# ShotTrack

A wrist-worn sensor that counts my basketball shots during practice. Think of a
step counter, but for jump shots.

The plan has three versions:

1. **v1:** a breadboard logger. STM32 Nucleo, MPU-6050 IMU and a microSD card,
   running FreeRTOS. Sessions are logged to CSV and checked in Python.
2. **v2:** a Bluetooth prototype on an nRF52840 Feather, showing a live count on
   my phone.
3. **v3:** my own PCB in a 3D-printed watch case.

The shot detection code (`detect/`) is plain C with no hardware calls, so the
same code runs on the microcontroller and on my laptop for testing.

The full plan is in [docs/handoff.md](docs/handoff.md).

## Status

Phase 1, step 1 (bring-up): LED blink and serial output with FreeRTOS. The
firmware builds in CI but hasn't been run on the board yet. Steps for running
it are in [docs/bringup.md](docs/bringup.md).

## Building

The firmware is a PlatformIO project in `firmware/stm32/`, using ST's HAL
drivers and FreeRTOS. From that folder:

```
pio run              # build
pio run -t upload    # flash the Nucleo over its built-in ST-LINK
pio device monitor   # serial output at 115200 baud
```

GitHub Actions builds it on every push.

## Repo layout

```
firmware/stm32/   PlatformIO project for the NUCLEO-L432KC (HAL + FreeRTOS)
detect/           shot detection, pure C (not started)
host/             PC build of detect/ for tests (not started)
tools/            Python scripts for parsing and plotting logs (not started)
tests/            pytest tests and recorded sessions (not started)
docs/             plan, bring-up notes, wiring, captures, plots
.github/          CI
```

## Results

TODO: no measurements yet. This table gets filled in with real numbers from
court sessions in step 10.

| Session | Hand count | Detected | False positives | Dropped samples |
|---|---|---|---|---|
| TODO | | | | |

## Bug log

Nothing yet. Real bugs get written up here as they happen.
