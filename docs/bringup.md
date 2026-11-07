# Hardware bring-up plan

Written before the parts arrived, so each step is something I haven't done
yet. Go in order: each step proves one more piece, so when something fails
I know which piece it is. Record what actually happens in the Results
column, including the failures.

## Checklist

Tick each box only after it has actually worked on the hardware, and put
the evidence (a number, a capture, a log file) next to it.

- [ ] Board flashes (`pio run -e nucleo_l432kc -t upload` ends in `[SUCCESS]`)
- [ ] UART console shows the banner and the once-a-second stats line
- [ ] IMU WHO_AM_I reads correctly (`imu: ok`, or 0x68 seen in a capture)
- [ ] SD logging works (a `LOGnnnnn.CSV` from a button-started session opens on the laptop)
- [ ] Logic analyzer capture of the I2C init sequence and sample reads ([debugging.md](debugging.md#checking-i2c-transactions-with-a-logic-analyzer-pulseview))
- [ ] Scope latency measured ([latency.md](latency.md), results table filled in)
- [ ] Current measured with the bench meter (section below, numbers recorded)
- [ ] First real 25-shot session, hand-counted, replayed with `tools/replay.py`

## Step by step

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

## Measuring current

`tools/measure_current.py` logs DC current from a SCPI bench meter (the
Keysight 34461A in the TMU labs) over USB or LAN.

**Safety first:** in current mode the meter is close to a short circuit.
It always goes *in series* (break the wire, meter in the gap), never
across a supply.

**Where to put the meter:**

- **MCU only:** the Nucleo-32 has a jumper for measuring the STM32's own
  supply current (JP1, labelled IDD; check its position in UM1956, the
  Nucleo-32 user manual). Remove it and connect the meter's leads to the
  two pins.
- **Sensors:** break the 3V3 wire that feeds the GY-521 and the SD
  breakout, and put the meter in that gap.

The USB cable also powers the ST-LINK chip, so measuring the USB line
measures the debugger too. Use one of the two points above.

**Meter set-up:** leads in the **3 A** and **COM** jacks (not 10 A; that
range is too coarse for mA and µA). The script sets DC current, auto range,
1 PLC.

```
pip install -r tools/requirements-lab.txt
python tools/measure_current.py --list           # shows e.g. USB0::0x2A8D::0x1301::MY...::INSTR
python tools/measure_current.py --resource <that string> \
    --phases idle:30,logging:30 --out data/current/mcu_idle_vs_logging.csv
```

The script asks you to put the board in each mode before it starts that
phase. It prints the mean and max per phase and saves every reading.

**Modes in v1:** the v1 firmware has no sleep mode yet, so the meaningful
v1 phases are *idle* (powered, sampling, not logging) and *logging* (a
session running, SD writes happening). The default phase names are
`sleep` and `active`, ready for when sleep is added in v2. Use `--phases`
to name what you're actually measuring.

`--dry-run` uses a fake meter to try the script at home. Its files are
named `DRYRUN_...`, marked as fake inside, and ignored by git. Never report
those numbers.

## Bug log

Write the real bug stories into [bug-log.md](bug-log.md) as they happen.
