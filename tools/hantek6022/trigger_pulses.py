#!/usr/bin/env python3
"""Find TransientDetector trigger pulses on CH2 of a 2-channel capture and flag
clusters (2+ pulses closer together than --cluster-ms) — the retrigger defect
signature. Each cluster should be exactly one real strum/pluck firing the
detector once; more than one pulse in a cluster means it fired multiple times
on the same event."""
import argparse
import json
from pathlib import Path

import numpy as np


def load_capture(base: Path):
    meta = json.loads(base.with_suffix(".json").read_text())
    sr = meta["sample_rate_hz"]

    def load(suffix, vrange_index):
        raw = np.fromfile(base.parent / f"{base.name}_{suffix}.raw", dtype=np.uint8)
        return (raw.astype(np.float32) - 128) * (5.0 / (vrange_index << 7))

    ch1 = load("ch1", meta["voltage_range_ch1_index"])
    ch2 = load("ch2", meta["voltage_range_ch2_index"]) if meta["channels"] == 2 else None
    if ch2 is not None:
        n = min(len(ch1), len(ch2))
        ch1, ch2 = ch1[:n], ch2[:n]
    return ch1, ch2, sr, meta


def find_pulses(ch2, sr, thresh_frac=0.5, skip_start_s=0.05):
    skip = int(skip_start_s * sr)
    v = ch2.copy()
    v[:skip] = v[skip] if len(v) > skip else 0  # ignore startup transient
    thresh = v.max() * thresh_frac
    above = v > thresh
    rising = np.flatnonzero(np.diff(above.astype(np.int8)) == 1) + 1
    return rising / sr, thresh


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("name", help="Capture basename, e.g. captures/verify1 (must be a 2-channel capture)")
    p.add_argument("--cluster-ms", type=float, default=100.0,
                    help="Pulses closer together than this (ms) are considered one cluster / same real event")
    p.add_argument("--thresh-frac", type=float, default=0.5, help="Fraction of CH2 peak used as the pulse threshold")
    args = p.parse_args()

    base = Path(args.name)
    ch1, ch2, sr, meta = load_capture(base)
    if ch2 is None:
        print("This capture has no CH2 — recapture with --channels 2.")
        return

    times, thresh = find_pulses(ch2, sr, args.thresh_frac)
    print(f"{base.name}: {len(ch1)/sr:.2f}s analyzed, {len(times)} trigger pulses found (CH2 threshold={thresh:.3f}V)\n")

    if len(times) == 0:
        print("No pulses found — check wiring/threshold.")
        return

    cluster_gap = args.cluster_ms / 1000.0
    clusters = [[times[0]]]
    for t in times[1:]:
        if t - clusters[-1][-1] < cluster_gap:
            clusters[-1].append(t)
        else:
            clusters.append([t])

    defective = [c for c in clusters if len(c) > 1]
    print(f"{len(clusters)} distinct events (clusters), {len(defective)} of them multi-pulse (defect)\n")

    if defective:
        print(f"Multi-pulse clusters (retrigger defect), gap threshold {args.cluster_ms}ms:")
        for c in defective:
            gaps = [f"{(b - a)*1000:.1f}ms" for a, b in zip(c, c[1:])]
            print(f"  t={c[0]:.3f}s  {len(c)} pulses  gaps: {', '.join(gaps)}")
    else:
        print("No multi-pulse clusters found — every event fired exactly once.")


if __name__ == "__main__":
    main()
