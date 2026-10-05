#!/usr/bin/env python3
"""Replay session CSVs through the real detection code and compare to labels.

Builds tools/host/detect_cli (lib/detect/detect.c compiled for the PC), runs
it on each session, and matches detected shots to labelled ones. A
detection counts as correct if it lands within 300 ms of a labelled shot's
release. Unmatched detections are false positives; unmatched labels are
misses.

Labels come from the shot_id column that tools/simulate_session.py writes.
Real recordings will need hand-counted labels added the same way.

Usage:
    python tools/replay.py data/synthetic/*.csv
    python tools/replay.py --check data/synthetic/*.csv   # exit 1 on any miss or false positive
"""
import argparse
import csv
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CLI_SRC = [os.path.join(ROOT, "lib", "detect", "detect.c"),
           os.path.join(ROOT, "tools", "host", "detect_cli.c")]
CLI_DEPS = CLI_SRC + [os.path.join(ROOT, "lib", "detect", "detect.h"),
                      os.path.join(ROOT, "lib", "detect", "detect_config.h")]
CLI_BIN = os.path.join(ROOT, "build", "detect_cli")
MATCH_WINDOW_MS = 300


def build_cli():
    """Compile detect_cli if it's missing or older than its sources."""
    if os.path.exists(CLI_BIN):
        built = os.path.getmtime(CLI_BIN)
        if all(os.path.getmtime(p) <= built for p in CLI_DEPS):
            return CLI_BIN
    os.makedirs(os.path.dirname(CLI_BIN), exist_ok=True)
    cmd = ["cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
           "-I" + os.path.join(ROOT, "lib", "detect"), *CLI_SRC, "-lm", "-o", CLI_BIN]
    subprocess.run(cmd, check=True)
    return CLI_BIN


def detect(cli, path):
    """Return the list of detected shot times (ms) for one session."""
    out = subprocess.run([cli, path], check=True, capture_output=True, text=True).stdout
    return [int(line.split(",")[1]) for line in out.splitlines() if line.startswith("shot,")]


def read_labels(path):
    """Return (labelled release times in ms, kind) for one session.

    kind is "SYNTHETIC" (simulator header), "recorded" (the firmware's log
    header) or "unknown". The times are None if the file has no shot_id
    column, e.g. a recording straight off the SD card before it's labelled."""
    windows = {}
    kind = "unknown"
    with open(path) as f:
        rows = []
        for line in f:
            if line.startswith("#"):
                if "SYNTHETIC" in line:
                    kind = "SYNTHETIC"
                elif "shottrack log" in line and kind == "unknown":
                    kind = "recorded"
            else:
                rows.append(line)
    reader = csv.DictReader(rows)
    if "shot_id" not in (reader.fieldnames or []):
        return None, kind
    for row in reader:
        sid = int(row.get("shot_id") or 0)
        if sid:
            windows.setdefault(sid, []).append(int(row["t_ms"]))
    # The label window is centred on the release.
    releases = [(min(ts) + max(ts)) // 2 for _, ts in sorted(windows.items())]
    return releases, kind


def match(detected, labelled):
    """Greedy one-to-one matching within MATCH_WINDOW_MS."""
    unused = list(labelled)
    tp = 0
    for d in detected:
        best = min(unused, key=lambda l: abs(l - d), default=None)
        if best is not None and abs(best - d) <= MATCH_WINDOW_MS:
            unused.remove(best)
            tp += 1
    return tp, len(detected) - tp, len(unused)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("sessions", nargs="+")
    ap.add_argument("--check", action="store_true",
                    help="exit with an error if any session has a miss or a false positive")
    args = ap.parse_args()

    cli = build_cli()
    print(f"{'session':32} {'data':9} {'labelled':>8} {'detected':>8} {'correct':>7} {'false+':>6} {'missed':>6}")
    bad = 0
    for path in args.sessions:
        labelled, kind = read_labels(path)
        detected = detect(cli, path)
        if labelled is None:
            print(f"{os.path.basename(path):32} {kind:9} {'n/a':>8} {len(detected):8} {'n/a':>7} {'n/a':>6} {'n/a':>6}")
            continue
        tp, fp, fn = match(detected, labelled)
        bad += (fp + fn) > 0
        print(f"{os.path.basename(path):32} {kind:9} {len(labelled):8} {len(detected):8} {tp:7} {fp:6} {fn:6}")

    if args.check and bad:
        print(f"FAIL: {bad} session(s) with misses or false positives")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
