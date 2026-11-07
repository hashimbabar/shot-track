# Shot-to-GPIO latency

How long after the wrist snap does the firmware know it was a shot? This
page splits that delay into parts, says how each part is measured, and has
a blank table for the real numbers.

**No results yet.** Everything below is method. Fill the table in from
real measurements only.

## Why it's split into parts

A scope can't see the moment of a shot. It only sees electrical signals, and
the only signal that reacts to the flick is the IMU itself. So the scope
measures what happens inside the board, and the other parts come from the
sensor datasheet and from timestamps the firmware already logs:

| Part | What it is | How it's measured |
|---|---|---|
| 1. Sensor filter | The MPU-6050's own low-pass filter (DLPF 44 Hz) delays its output | Datasheet: RM-MPU-6000A-00, section 4.3 table, row DLPF_CFG = 3 (gyro delay about 4.8 ms; check the exact figure in your review notes) |
| 2. Sampling | The sensor updates its registers on its own 200 Hz clock; TIM6 reads them on another. A new value waits 0-5 ms to be read | Bound: 0 to 5 ms (one sample period) |
| 3. Detection | From the gyro peak to the sample where the filtered gyro drops below the release level (350 °/s) and the shot is confirmed | Firmware: every shot logs `delay_ms` (peak to confirm). It's printed in the stats line as `shots N (last +D ms)` and written in the SD log as `# shot,t_ms,peak_dps,peak_mg,delay_ms` |
| 4. Processing | From the end of the I2C read of the confirming sample to the PA12 pulse: queue, task switch, `detect_update()` | Scope, below |

**Total ≈ part 1 + part 2 + part 3 + part 4.** Part 3 will be by far the
biggest, because the detector waits for the peak to end before deciding.
On the synthetic CI sessions it comes out at 45-65 ms (synthetic data, set
by the simulator's flick width; real flicks will differ).

## Scope measurement (part 4, and checking part 3)

The firmware has three debug pins (wiring in [wiring.md](wiring.md)):

| Pin | Header | Meaning |
|---|---|---|
| PA8 | D9 | High while the sampling task reads the IMU, once every 5 ms |
| PA12 | D2 | Short pulse (a few µs) when the detect task confirms a shot |
| PA11 | D10 | High while the logger writes to the SD card |

Use the normal build (`pio run -e nucleo_l432kc -t upload`), not the debug
build: the debug build is slower and would make the numbers worse than
reality.

1. **Probes:** CH1 on D9 (PA8), CH2 on D2 (PA12), both grounds to the
   Nucleo's GND. 10x probes are fine; these are 3.3 V logic signals.
2. **Trigger:** CH2, rising edge, **single** (one-shot) mode, level
   about 1.6 V. Put the trigger point near the right edge of the screen so
   most of the screen shows what happened *before* the shot.
3. **Timebase:** start at 2 ms/div to see several PA8 pulses, then zoom
   to about 20 µs/div around the trigger.
4. Arm the trigger, then flick the board (or your wrist with it strapped
   on) like a shot. The serial console should show `shots` go up by one.
5. **Measure part 4:** cursor from the falling edge of the last PA8 pulse
   before the trigger (the I2C read of the confirming sample has just
   finished) to the rising edge of PA12. Use the scope's Δt readout.
6. **Check part 3:** zoom out to see the PA8 pulses. Write down the
   `last +D ms` value from the stats line for the same shot. The PA12
   pulse should land on a PA8 pulse boundary (detection happens right after
   a read).
7. Repeat for at least 10 shots and record min / mean / max.

Also worth a look while the scope is set up: the PA8 period should read
5.000 ms with very little jitter. Write down the jitter (the scope's
"period" measurement, min/max) too.

## Making it faster

Each knob trades something:

| Knob | Where | Effect on latency | Cost |
|---|---|---|---|
| `DETECT_RELEASE_RATIO` (0.5) | `lib/detect/detect_config.h` | Higher = confirm sooner after the peak | Noise near the threshold can split a peak in two (the refractory window still stops double counts) |
| `DETECT_LPF_CUTOFF_HZ` (15) | `detect_config.h` | Higher = less filter delay (~1/(2π·fc)) | More noise in the gyro magnitude |
| MPU-6050 DLPF (44 Hz) | `imu_cfg` in `firmware/src/task_sampling.c` | Wider bandwidth = less sensor delay (see the RM 4.3 table) | More noise, and above ~100 Hz it aliases at 200 Hz sampling |
| Sample rate (200 Hz) | TIM6 setup in `board.c`, `SAMPLE_RATE_HZ` | Faster = smaller part 2 and finer steps in part 3 | More I2C traffic, more CPU, bigger logs |
| Detect task priority | `app.h` | Part 4 only; already high (3) | Little to gain |

Change one knob at a time, re-run the unit tests and the synthetic replay
(`pio test -e native`, `python tools/replay.py --check data/sim/*.csv`),
then re-measure on the board. Record the before and after numbers below.

## Results

TODO: fill in from real measurements. Leave a row out rather than guess.

| Date | Build / settings | Shots measured | Part 3: peak → confirm (min / mean / max, ms) | Part 4: PA8 fall → PA12 (min / mean / max, µs) | PA8 period jitter | Notes |
|---|---|---|---|---|---|---|
| | | | | | | |
| | | | | | | |
