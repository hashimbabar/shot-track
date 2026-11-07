#!/usr/bin/env python3
"""Log DC current from a SCPI bench multimeter, labelled by mode.

Written for a Keysight 34461A (the TMU lab meters), and should work with
other SCPI DMMs that understand CONFigure:CURRent:DC and READ?. Connects over
USB or LAN with PyVISA, runs one or more phases (e.g. "sleep" then "active"),
takes a reading every --interval seconds, saves everything to CSV and prints
the mean and max per phase.

    pip install -r tools/requirements-lab.txt
    python tools/measure_current.py --list                      # find the meter
    python tools/measure_current.py --resource USB0::0x2A8D::0x1301::MY1234::INSTR \\
        --phases sleep:30,active:30 --out data/current/run1.csv

Before each phase it asks you to put the board in that mode and press Enter
(--no-prompt skips that). The first --settle seconds of each phase are
recorded but left out of the summary, so the switch-over doesn't skew it.

--dry-run uses a fake meter so the script can be tried without hardware.
Dry-run files and output are marked as FAKE; never report those numbers.

How to wire the meter in: see docs/bringup.md, "Measuring current".
"""
import argparse
import csv
import os
import random
import statistics
import sys
import time


class FakeMeter:
    """Stands in for the DMM in --dry-run. Values are made up."""

    idn = "FAKE METER (dry run, not a measurement)"
    _levels = {"sleep": 0.0015, "active": 0.012}

    def __init__(self):
        self.mode = None

    def read(self):
        level = self._levels.get(self.mode, 0.005)
        return random.gauss(level, level * 0.05)

    def close(self):
        pass


class ScpiMeter:
    """A SCPI multimeter configured for DC current."""

    def __init__(self, resource, backend, current_range, nplc, terminals, timeout_ms):
        import pyvisa  # only needed for a real meter

        self.rm = pyvisa.ResourceManager(backend) if backend else pyvisa.ResourceManager()
        self.inst = self.rm.open_resource(resource)
        self.inst.timeout = timeout_ms
        self.inst.read_termination = "\n"
        self.inst.write_termination = "\n"
        self.idn = self.inst.query("*IDN?").strip()
        self.inst.write("*RST")
        self.inst.write("*CLS")
        # CONF sets the function and range in one go; AUTO lets the meter
        # pick, which matters when sleep (uA) and active (mA) differ a lot.
        self.inst.write(f"CONF:CURR:DC {current_range}")
        self.inst.write(f"CURR:DC:NPLC {nplc}")
        if terminals:
            # 34461A: 3 A or 10 A input jack. Other meters may not have this.
            self.inst.write(f"CURR:DC:TERM {terminals}")
        self.mode = None
        self._check_errors()

    def _check_errors(self):
        err = self.inst.query("SYST:ERR?").strip()
        if not err.startswith(("+0", "0")):
            raise RuntimeError(f"meter reported an error: {err}")

    def read(self):
        return float(self.inst.query("READ?"))

    def close(self):
        try:
            self._check_errors()
        finally:
            self.inst.close()
            self.rm.close()


def parse_phases(text):
    phases = []
    for part in text.split(","):
        name, _, seconds = part.partition(":")
        if not name or not seconds:
            raise argparse.ArgumentTypeError(f"bad phase '{part}', expected name:seconds")
        phases.append((name.strip(), float(seconds)))
    return phases


def fmt_current(amps):
    if abs(amps) < 1e-3:
        return f"{amps * 1e6:.1f} uA"
    return f"{amps * 1e3:.3f} mA"


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--resource", help="VISA resource string, e.g. USB0::...::INSTR or TCPIP0::192.168.1.20::INSTR")
    ap.add_argument("--list", action="store_true", help="list VISA resources and exit")
    ap.add_argument("--backend", default="", help="PyVISA backend, e.g. '@py' for pyvisa-py (default: system VISA)")
    ap.add_argument("--phases", type=parse_phases, default=parse_phases("sleep:30,active:30"),
                    help="comma-separated name:seconds (default: sleep:30,active:30)")
    ap.add_argument("--interval", type=float, default=0.2, help="seconds between readings (default 0.2)")
    ap.add_argument("--settle", type=float, default=2.0, help="seconds at the start of each phase left out of the summary")
    ap.add_argument("--range", dest="current_range", default="AUTO", help="current range in A, or AUTO (default)")
    ap.add_argument("--nplc", default="1", help="integration time in power-line cycles (default 1)")
    ap.add_argument("--terminals", default="3", help="34461A input jack: 3 or 10 (empty to skip)")
    ap.add_argument("--timeout-ms", type=int, default=5000)
    ap.add_argument("--out", help="CSV path (default data/current/current_log.csv, "
                                  "or data/current/DRYRUN_current_log.csv with --dry-run)")
    ap.add_argument("--no-prompt", action="store_true", help="don't wait for Enter between phases")
    ap.add_argument("--dry-run", action="store_true", help="fake meter, no hardware (output marked FAKE)")
    args = ap.parse_args()

    if args.list:
        import pyvisa
        rm = pyvisa.ResourceManager(args.backend) if args.backend else pyvisa.ResourceManager()
        for r in rm.list_resources():
            print(r)
        return 0

    if args.out is None:
        # Dry-run files get their own name, which .gitignore keeps out of git.
        args.out = "data/current/DRYRUN_current_log.csv" if args.dry_run else "data/current/current_log.csv"

    if args.dry_run:
        meter = FakeMeter()
    elif args.resource:
        meter = ScpiMeter(args.resource, args.backend, args.current_range, args.nplc,
                          args.terminals, args.timeout_ms)
    else:
        ap.error("give --resource (find it with --list), or use --dry-run")

    print(f"meter: {meter.idn}")
    os.makedirs(os.path.dirname(args.out) or ".", exist_ok=True)
    rows = []
    try:
        with open(args.out, "w", newline="") as f:
            if args.dry_run:
                f.write("# DRY RUN - fake readings from tools/measure_current.py, NOT a measurement\n")
            f.write(f"# meter={meter.idn} range={args.current_range} nplc={args.nplc} interval_s={args.interval}\n")
            w = csv.writer(f)
            w.writerow(["t_s", "mode", "current_a", "settling"])
            t0 = time.monotonic()
            for mode, seconds in args.phases:
                if not args.no_prompt:
                    input(f"Put the board in '{mode}' mode, then press Enter... ")
                meter.mode = mode
                start = time.monotonic()
                next_t = start
                while time.monotonic() - start < seconds:
                    amps = meter.read()
                    now = time.monotonic()
                    settling = (now - start) < args.settle
                    w.writerow([f"{now - t0:.3f}", mode, f"{amps:.9e}", int(settling)])
                    rows.append((mode, amps, settling))
                    next_t += args.interval
                    time.sleep(max(0.0, next_t - time.monotonic()))
                f.flush()
                print(f"  {mode}: done")
    finally:
        meter.close()

    tag = "  [FAKE - dry run]" if args.dry_run else ""
    print(f"\nsaved {args.out}{tag}")
    print(f"{'mode':10} {'readings':>8} {'mean':>12} {'max':>12}")
    for mode, _ in args.phases:
        vals = [a for m, a, s in rows if m == mode and not s]
        if not vals:
            print(f"{mode:10} {0:8}  (no readings after the settle time)")
            continue
        print(f"{mode:10} {len(vals):8} {fmt_current(statistics.fmean(vals)):>12} {fmt_current(max(vals)):>12}{tag}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
