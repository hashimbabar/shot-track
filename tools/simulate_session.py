#!/usr/bin/env python3
"""Generate a labelled SYNTHETIC IMU session for testing shot detection.

This is NOT real data. Nothing here was recorded on a wrist. The shapes and
sizes of each movement are my guesses at what a wrist-worn IMU would see,
picked before having any hardware. They exist so the detection code and the
tools can be exercised end to end in CI. Once real sessions are recorded,
compare them to these and fix the guesses.

Output is a CSV in the same format the firmware logs (raw MPU-6050 counts at
200 Hz, +-16 g and +-2000 dps), plus two label columns the firmware does not
have:

    seq,t_ms,ax,ay,az,gx,gy,gz,activity,shot_id

activity is idle / walk / dribble / fidget / shot. shot_id is 0 except in a
600 ms window centred on each shot's release, where it is the shot number.

Usage:
    python tools/simulate_session.py --seed 1 --shots 25 --out data/sim/session_01.csv
"""
import argparse
import csv
import os

import numpy as np

FS = 200                    # Hz, same as the firmware
DT = 1.0 / FS
ACCEL_LSB_PER_G = 2048.0    # +-16 g
GYRO_LSB_PER_DPS = 16.4     # +-2000 dps
SHOT_LABEL_HALF_WIDTH_S = 0.3


def bump(t, centre, sigma):
    """Gaussian bump of height 1 centred on `centre` (seconds)."""
    return np.exp(-0.5 * ((t - centre) / sigma) ** 2)


class Session:
    def __init__(self, rng):
        self.rng = rng
        self.chunks = []  # (accel[n,3], gyro[n,3], activity, shot_times)

    def _blank(self, seconds):
        n = int(round(seconds * FS))
        t = np.arange(n) * DT
        accel = np.zeros((n, 3))
        accel[:, 2] = 1.0  # gravity on z with the hand roughly level
        return t, accel, np.zeros((n, 3))

    def idle(self, seconds):
        t, a, g = self._blank(seconds)
        # Slow drift, like standing still but not frozen.
        g[:, 1] += 5 * np.sin(2 * np.pi * 0.3 * t + self.rng.uniform(0, 6))
        self.chunks.append((a, g, "idle", []))

    def walk(self, seconds):
        t, a, g = self._blank(seconds)
        f = self.rng.uniform(1.7, 2.0)               # steps per second
        swing = self.rng.uniform(80, 150)            # arm swing, dps
        a[:, 2] += 0.3 * np.sin(2 * np.pi * f * t)   # bounce per step
        a[:, 0] += 0.15 * np.sin(np.pi * f * t)
        g[:, 1] += swing * np.sin(np.pi * f * t)     # one swing per two steps
        self.chunks.append((a, g, "walk", []))

    def dribble(self, seconds):
        t, a, g = self._blank(seconds)
        f = self.rng.uniform(1.8, 2.6)               # bounces per second
        for k in np.arange(0.2, seconds - 0.2, 1.0 / f):
            push = self.rng.uniform(250, 500)        # wrist push, dps
            g[:, 0] += push * bump(t, k, 0.04)
            g[:, 0] -= 0.5 * push * bump(t, k + 0.18, 0.05)  # catch
            a[:, 1] += self.rng.uniform(0.6, 1.3) * bump(t, k, 0.04)
        self.chunks.append((a, g, "dribble", []))

    def fidget(self, seconds):
        """Random hand movements: passes, adjusting a sleeve, waving."""
        t, a, g = self._blank(seconds)
        for _ in range(int(seconds)):
            c = self.rng.uniform(0.2, seconds - 0.2)
            axis = self.rng.integers(0, 3)
            g[:, axis] += self.rng.uniform(-450, 450) * bump(t, c, self.rng.uniform(0.05, 0.15))
            a[:, self.rng.integers(0, 3)] += self.rng.uniform(-0.8, 0.8) * bump(t, c, 0.08)
        self.chunks.append((a, g, "fidget", []))

    def shot(self):
        """One jump shot, 2.5 s long, release 1.0 s in."""
        t, a, g = self._blank(2.5)
        r = 1.0
        peak = self.rng.uniform(900, 1500)           # wrist snap, dps
        width = self.rng.uniform(0.025, 0.035)
        # Load: the wrist cocks back before the release.
        g[:, 0] -= self.rng.uniform(150, 250) * bump(t, r - 0.25, 0.08)
        # Leg drive / jump: upward acceleration.
        a[:, 2] += self.rng.uniform(0.8, 1.5) * bump(t, r - 0.15, 0.1)
        # Release: the wrist flick.
        g[:, 0] += peak * bump(t, r, width)
        g[:, 1] += 0.2 * peak * bump(t, r, width)
        a[:, 1] += self.rng.uniform(2.2, 4.0) * bump(t, r, 0.03)
        # Sometimes a second, smaller peak in the follow-through. This is
        # the case the refractory window is there for.
        if self.rng.random() < 0.35:
            d = self.rng.uniform(0.15, 0.26)
            g[:, 0] += self.rng.uniform(0.55, 0.8) * peak * bump(t, r + d, width)
            a[:, 1] += self.rng.uniform(1.0, 2.0) * bump(t, r + d, 0.03)
        # Landing.
        a[:, 2] += self.rng.uniform(1.5, 3.0) * bump(t, r + 0.5, 0.04)
        self.chunks.append((a, g, "shot", [r]))

    def render(self):
        accel = np.vstack([c[0] for c in self.chunks])
        gyro = np.vstack([c[1] for c in self.chunks])
        n = len(accel)
        activity = []
        shot_id = np.zeros(n, dtype=int)
        offset, shots = 0, 0
        for a, _, name, releases in self.chunks:
            activity += [name] * len(a)
            for r in releases:
                shots += 1
                lo = offset + int((r - SHOT_LABEL_HALF_WIDTH_S) * FS)
                hi = offset + int((r + SHOT_LABEL_HALF_WIDTH_S) * FS)
                shot_id[lo:hi + 1] = shots
            offset += len(a)
        # Sensor noise (rough guesses at MPU-6050 noise at 44 Hz bandwidth).
        accel += self.rng.normal(0, 0.01, accel.shape)
        gyro += self.rng.normal(0, 1.5, gyro.shape)
        return accel, gyro, activity, shot_id, shots


def build_session(seed, n_shots):
    rng = np.random.default_rng(seed)
    s = Session(rng)
    s.idle(rng.uniform(2, 4))
    others = [s.walk, s.dribble, s.fidget, s.idle]
    for _ in range(n_shots):
        # Between shots: walk to get the rebound, dribble, pass, stand around.
        for _ in range(rng.integers(1, 3)):
            others[rng.integers(0, len(others))](rng.uniform(2, 6))
        s.shot()
    s.walk(rng.uniform(3, 6))
    s.dribble(rng.uniform(5, 10))  # a dribble-only stretch at the end
    s.idle(2)
    return s.render()


def to_counts(x, lsb):
    return np.clip(np.round(x * lsb), -32768, 32767).astype(int)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--shots", type=int, default=25)
    ap.add_argument("--out", required=True)
    args = ap.parse_args()

    accel, gyro, activity, shot_id, shots = build_session(args.seed, args.shots)
    ax = to_counts(accel, ACCEL_LSB_PER_G)
    gx = to_counts(gyro, GYRO_LSB_PER_DPS)

    os.makedirs(os.path.dirname(args.out) or ".", exist_ok=True)
    with open(args.out, "w", newline="") as f:
        f.write("# SYNTHETIC DATA - generated by tools/simulate_session.py, NOT a real recording\n")
        f.write(f"# seed={args.seed} shots={shots} samples={len(ax)} rate={FS}Hz "
                "accel=+-16g(2048 LSB/g) gyro=+-2000dps(16.4 LSB/dps)\n")
        w = csv.writer(f)
        w.writerow(["seq", "t_ms", "ax", "ay", "az", "gx", "gy", "gz", "activity", "shot_id"])
        for i in range(len(ax)):
            w.writerow([i + 1, (i + 1) * 5, *ax[i], *gx[i], activity[i], shot_id[i]])

    print(f"wrote {args.out}: {len(ax) / FS:.0f} s, {shots} labelled shots (synthetic)")


if __name__ == "__main__":
    main()
