# ShotTrack

[![ci](https://github.com/hashimbabar/shot-track/actions/workflows/ci.yml/badge.svg)](https://github.com/hashimbabar/shot-track/actions/workflows/ci.yml)

A wrist-worn sensor that counts my basketball shots during practice, like a
step counter for shots. I play intramural ball at TMU and wanted real numbers
on how many shots I actually put up, instead of guessing.

**Status:** Firmware, driver and host tests complete and passing in CI;
hardware bring-up in progress. The parts haven't arrived yet, so **nothing
here has run on a real board or been worn on court**. All data in this repo
is synthetic and labelled as such.

## What it does (v1)

- **Samples an MPU-6050 IMU at 200 Hz** from a hardware timer interrupt on an
  STM32L432 (NUCLEO-L432KC), using FreeRTOS.
- **Detects each shot** from the wrist-flick peak: filtered gyro magnitude,
  threshold with hysteresis, an accel check, and a refractory window so one
  shot isn't counted twice.
- **Logs sessions to a microSD card** as CSV from a low-priority task fed by
  a queue, so slow SD writes can't hold up sampling.
- **Counts dropped samples** instead of hiding them (sequence numbers, queue
  overflow counters, debug pins for a logic analyzer).
- **Button** to start/stop a session, **LED** flash per detected shot, stats
  on the serial port once a second.

Built from scratch where it matters for learning: the MPU-6050 driver is
written from the register map, and the SD card SPI driver from the SD spec.
FreeRTOS, FatFs and ST's HAL are used as-is. The MPU-6050 driver is also
published on its own as
[hashimbabar/mpu6050-driver](https://github.com/hashimbabar/mpu6050-driver).

## Architecture

```mermaid
flowchart LR
    TIM6["TIM6 ISR 200 Hz"] -- notify --> S["sampling<br/>prio 4"]
    S -- queue --> D["detect<br/>prio 3"]
    S -- queue --> L["logger<br/>prio 1"]
    D -- shots --> L
    D -- notify --> U["ui<br/>prio 2"]
    BTN["button ISR"] -- notify --> U
    U -- start/stop --> L
    L --> SD[("microSD")]
```

Sampling has the only hard deadline (every 5 ms), so it gets the highest
priority. The SD card can stall for hundreds of milliseconds, so the logger
gets the lowest priority and a 640 ms queue in front of it. Details, interrupt
priorities and the detection pipeline are in
[docs/architecture.md](docs/architecture.md).

The detection code in `lib/detect` is plain C with no hardware calls, so the
same file runs on the STM32, in the unit tests, and in the replay tool on a
laptop.

## Repo layout

```
firmware/        STM32 application: board setup, FreeRTOS tasks, SD driver
lib/mpu6050/     MPU-6050 driver (register map, I2C via function pointers)
lib/detect/      shot detection, pure C; thresholds in detect_config.h
lib/FreeRTOS/    FreeRTOS kernel V11.1.0 (vendored, unmodified)
lib/FatFs/       FatFs R0.16 (vendored, unmodified)
test/            Unity tests run on the PC (mocked I2C, synthetic signals)
tools/           simulator, replay and plotting scripts (Python)
docs/            wiring, BOM, architecture, debugging, bring-up, roadmap
```

## Build and test

Needs [PlatformIO](https://platformio.org/) (`pip install platformio`, or the
VS Code extension) and Python 3.

```
pio run -e nucleo_l432kc              # build the firmware
pio run -e nucleo_l432kc -t upload    # flash the Nucleo over its ST-LINK
pio device monitor                    # serial console, 115200 baud
pio test -e native                    # unit tests on the PC (25 tests)
```

The unit tests cover the MPU-6050 driver against a mocked I2C bus (init
sequence, burst-read parsing, NACK and wrong-WHO_AM_I errors) and the detector
against synthetic signals (single shot, double peak inside the refractory
window, dribbling, flat signal, timer wraparound).

## Simulator and replay (synthetic data)

Until there is real data, `tools/simulate_session.py` generates labelled
**synthetic** sessions: shots, dribbling, walking, random hand movement and
sensor noise. The movement shapes are my own guesses, not measurements.

```
pip install -r tools/requirements.txt
python tools/simulate_session.py --seed 1 --shots 25 --out data/synthetic/session_01.csv
python tools/replay.py data/synthetic/*.csv      # builds detect.c for the PC and compares to labels
python tools/plot_session.py data/synthetic/session_01.csv --out plot.png
```

![Synthetic session: gyro magnitude with detected shots](docs/img/synthetic_session.png)

![Synthetic zoom: follow-through peaks counted once](docs/img/synthetic_refractory.png)

Both plots are **synthetic data**. CI regenerates three sessions (seeds 1-3)
and fails if the replay doesn't match the labels exactly. That only proves
the code does what it was designed to do on signals made with the same
assumptions. It says nothing about accuracy on a real wrist.

## Results

Real on-court results come after hardware testing. The plan: 3-5 shootaround
sessions with a hand count, including passing and dribbling to check for false
positives. That table will go here, with the real numbers whatever they turn
out to be.

The only numbers so far are from synthetic data: on the three CI sessions,
turning the refractory window off makes the detector count 29, 25 and 29 shots
instead of 25 each, because of simulated follow-through peaks.

## Bug log

Real bugs found during bring-up will be written up here.

## Roadmap

1. **v1, breadboard logger** (this repo, in progress): STM32 + FreeRTOS + SD card.
2. **v2, Bluetooth**: nRF52840 Feather on Zephyr, live shot count on my phone over BLE.
3. **v3, custom hardware**: my own KiCad PCB (nRF52840 module, IMU, LiPo
   charger, USB-C) in a 3D-printed watch case.

Full plan with the v1 step checklist: [docs/roadmap.md](docs/roadmap.md).
Wiring: [docs/wiring.md](docs/wiring.md). Parts: [docs/bom.md](docs/bom.md).

## License

MIT for my code ([LICENSE](LICENSE)). FreeRTOS (MIT) and FatFs (BSD-style)
keep their own licenses in `lib/`.
