#!/usr/bin/env python3
"""Plot a session's gyro magnitude with detected and labelled shots marked.

Detected shots come from the real detection code (tools/host/detect_cli, built
by replay.py). If the session has labels (shot_id column), the labelled shot
windows are shaded. If the file header says SYNTHETIC, the title says so.

Usage:
    python tools/plot_session.py data/synthetic/session_01.csv --out docs/img/synthetic_session.png
    python tools/plot_session.py data/synthetic/session_01.csv --start 20 --end 32 --out zoom.png
"""
import argparse
import os
import sys

import matplotlib
matplotlib.use("Agg")  # no display needed (CI, SSH)
import matplotlib.pyplot as plt
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import replay  # noqa: E402  (reuses its build and label helpers)

GYRO_LSB_PER_DPS = 16.4
THRESHOLD_DPS = 700  # DETECT_GYRO_THRESHOLD_DPS in lib/detect/detect_config.h

# Colours: one series hue, one accent for detections, neutrals for the rest.
SERIES = "#2a78d6"
ACCENT = "#eb6834"
INK = "#0b0b0b"
INK_2 = "#52514e"
SHADE = "#e4e3df"
SURFACE = "#fcfcfb"


def load(path):
    rows = [l for l in open(path) if not l.startswith("#")]
    header = rows[0].strip().split(",")
    cols = {name: i for i, name in enumerate(header)}
    data = np.array([[int(v) for v in r.strip().split(",")[:8]] for r in rows[1:]])
    t_s = data[:, cols["t_ms"]] / 1000.0
    gyro = data[:, 5:8] / GYRO_LSB_PER_DPS
    return t_s, np.linalg.norm(gyro, axis=1)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("session")
    ap.add_argument("--out", required=True)
    ap.add_argument("--start", type=float, help="start time in seconds")
    ap.add_argument("--end", type=float, help="end time in seconds")
    ap.add_argument("--title", default="")
    args = ap.parse_args()

    t, gyro_mag = load(args.session)
    detected = np.array(replay.detect(replay.build_cli(), args.session)) / 1000.0
    labelled, kind = replay.read_labels(args.session)
    synthetic = kind == "SYNTHETIC"
    labelled = np.array(labelled or []) / 1000.0

    lo = args.start if args.start is not None else t[0]
    hi = args.end if args.end is not None else t[-1]
    keep = (t >= lo) & (t <= hi)

    plt.rcParams.update({"font.size": 10, "axes.edgecolor": INK_2, "axes.labelcolor": INK_2,
                         "xtick.color": INK_2, "ytick.color": INK_2})
    fig, ax = plt.subplots(figsize=(11, 3.6), dpi=130)
    fig.patch.set_facecolor(SURFACE)
    ax.set_facecolor(SURFACE)

    first = True
    for r in labelled[(labelled >= lo) & (labelled <= hi)]:
        ax.axvspan(r - replay.MATCH_WINDOW_MS / 1000, r + replay.MATCH_WINDOW_MS / 1000,
                   color=SHADE, lw=0, label="labelled shot (±300 ms)" if first else None, zorder=0)
        first = False

    ax.plot(t[keep], gyro_mag[keep], color=SERIES, lw=1.2 if hi - lo > 60 else 2, label="gyro magnitude")
    ax.axhline(THRESHOLD_DPS, color=INK_2, lw=1, ls="--", zorder=1)
    ax.text(lo, THRESHOLD_DPS, " detection threshold", color=INK_2, va="bottom", fontsize=9)

    d = detected[(detected >= lo) & (detected <= hi)]
    peak_y = np.interp(d, t, gyro_mag)
    ax.scatter(d, peak_y + 60, marker="v", s=55, color=ACCENT, edgecolor=SURFACE, linewidth=1.5,
               label=f"detected shot ({len(d)})", zorder=3)

    heading = {"SYNTHETIC": "SYNTHETIC DATA", "recorded": "recorded session"}.get(kind, "session")
    title = args.title or os.path.basename(args.session)
    ax.set_title(f"{heading}: {title}", loc="left", color=INK, fontsize=11, fontweight="bold")
    ax.set_xlabel("time (s)")
    ax.set_ylabel("gyro magnitude (deg/s)")
    ax.set_xlim(lo, hi)
    ax.set_ylim(0, max(1800, gyro_mag[keep].max() * 1.3))  # headroom for the legend
    ax.grid(axis="y", color=SHADE, lw=0.8)
    ax.spines[["top", "right"]].set_visible(False)
    ax.legend(loc="upper right", frameon=False, ncol=3, fontsize=9, labelcolor=INK_2)
    if synthetic:
        fig.text(0.995, 0.01, "Simulated by tools/simulate_session.py - not a real recording",
                 ha="right", va="bottom", fontsize=8, color=INK_2)

    fig.tight_layout()
    os.makedirs(os.path.dirname(args.out) or ".", exist_ok=True)
    fig.savefig(args.out, facecolor=SURFACE)
    print(f"wrote {args.out}")


if __name__ == "__main__":
    main()
