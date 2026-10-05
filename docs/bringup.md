# Hardware bring-up plan

Written before the parts arrived, so each step is something I haven't done
yet. Go in order: each step proves one more piece, so when something fails
I know which piece it is. Record what actually happens in the Results
column, including the failures.

Wiring for every step is in [wiring.md](wiring.md). Flash with
`pio run -e nucleo_l432kc -t upload` (or the → button in VS Code) and open
the serial monitor at 115200 baud (`pio device monitor`, or the plug button).

| # | Step | Wire up | Expect | Results |
|---|---|---|---|---|
| 1 | Board alive | Nucleo only, USB | Banner: `ShotTrack firmware`, `system clock: 80000000 Hz`, `32 kHz crystal: ok`. LED blinks fast (no IMU yet). `imu: init failed: i2c error (no ack?)` once a second | TODO |
| 2 | IMU found | + MPU-6050 | `imu: ok (+-16 g, +-2000 dps, 200 Hz)`. LED changes to a 1 Hz blink. Stats line shows `samples` going up by ~200 per second | TODO |
| 3 | IMU values sane | same | Lying flat: about 2048 counts (1 g) on one accel axis. Gyro near 0 when still, big numbers when the board is twisted. (Use the debugger, `p g_stats` / a breakpoint in the sampling task) | TODO |
| 4 | Sample timing | + logic analyzer on D9 (PA8) | Pulses every 5.000 ms, each ~0.45 ms wide. Record min/max period | TODO |
| 5 | Missed samples at rest | same | `missed 0` in the stats line after a few minutes | TODO |
| 6 | SD card | + SD breakout, FAT32 card | Press the button: `log: started LOG00001.CSV`, LED solid. Press again: `log: stopped`. File opens on the laptop | TODO |
| 7 | SD timing | + analyzer on D10 (PA11) | Record the longest PA11 pulse. Check `q_full 0` | TODO |
| 8 | Replay a real log | laptop | `python tools/replay.py LOG00001.CSV` runs (no labels yet, so only the detected count) | TODO |
| 9 | Detection live | on the wrist | A flick of the wrist like a shot flashes the LED; `shots` goes up by 1 per shot | TODO |

## If something doesn't work

- Paste the full serial output and the stats line.
- `imu: init failed: i2c error`: check SDA/SCL aren't swapped, and VCC/GND.
  Put the analyzer on SDA/SCL and decode I2C: is there an ACK after address
  0x68?
- `imu: init failed: wrong WHO_AM_I`: the board may be a clone (MPU-6500
  reads 0x70, MPU-9250 reads 0x71). Note the value and decide whether to
  accept it.
- `sd: mount failed`: card not FAT32, CS or MISO wiring, or the breakout
  needs 5 V. Try another card.
- LED blinking very fast and `FATAL:` on the console: the message says
  which check failed. See [debugging.md](debugging.md).

Write the real bug stories into the README's bug log as they happen.
