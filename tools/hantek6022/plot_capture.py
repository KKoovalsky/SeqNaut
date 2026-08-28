#!/usr/bin/env python3
"""Quick-look plot of a capture_until_stop.py raw capture (full view + a zoom window)."""
import argparse
import json
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np


def load_channel(base: Path, suffix: str, vrange_index: int):
    raw = np.fromfile(base.parent / f"{base.name}_{suffix}.raw", dtype=np.uint8)
    return (raw.astype(np.float32) - 128) * (5.0 / (vrange_index << 7))


def minmax_envelope(v, t, n_bins):
    """Bin into n_bins and keep each bin's min+max, so every sample's extremes
    are represented (unlike naive striding, which just skips samples)."""
    n = len(v)
    bin_size = max(1, n // n_bins)
    n_full = n // bin_size
    vb = v[:n_full * bin_size].reshape(n_full, bin_size)
    vmin = vb.min(axis=1)
    vmax = vb.max(axis=1)
    t_bins = t[:n_full * bin_size:bin_size]
    x = np.repeat(t_bins, 2)
    y = np.empty(2 * n_full, dtype=v.dtype)
    y[0::2] = vmin
    y[1::2] = vmax
    return x, y


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("name", help="Capture basename, e.g. captures/teststrum")
    p.add_argument("--zoom", nargs=2, type=float, metavar=("START_S", "END_S"),
                    help="Zoom window in seconds for the second subplot (default: last 2s)")
    p.add_argument("--out", type=Path, default=None, help="Output PNG path (default: <name>_plot.png)")
    args = p.parse_args()

    base = Path(args.name)
    meta = json.loads(base.with_suffix(".json").read_text())
    sr = meta["sample_rate_hz"]

    v1 = load_channel(base, "ch1", meta["voltage_range_ch1_index"])
    t = np.arange(len(v1)) / sr
    two_ch = meta["channels"] == 2
    if two_ch:
        v2 = load_channel(base, "ch2", meta["voltage_range_ch2_index"])
        n = min(len(v1), len(v2))
        v1, v2, t = v1[:n], v2[:n], t[:n]

    zoom_lo, zoom_hi = args.zoom if args.zoom else (max(0.0, t[-1] - 2.0), t[-1])
    mask = (t >= zoom_lo) & (t <= zoom_hi)

    fig, axes = plt.subplots(2, 1, figsize=(14, 7))
    n_bins = 4000
    x1, y1 = minmax_envelope(v1, t, n_bins)
    axes[0].plot(x1, y1, linewidth=0.4, label="ch1")
    if two_ch:
        x2, y2 = minmax_envelope(v2, t, n_bins)
        axes[0].plot(x2, y2, linewidth=0.4, label="ch2")
        axes[0].legend()
    axes[0].set_title(f"Full capture: {base.name} (min/max envelope, {n_bins} bins — every sample's extremes represented)")
    axes[0].set_xlabel("time (s)"); axes[0].set_ylabel("V")

    axes[1].plot(t[mask], v1[mask], linewidth=0.6, label="ch1")
    if two_ch:
        axes[1].plot(t[mask], v2[mask], linewidth=0.6, label="ch2")
        axes[1].legend()
    axes[1].set_title(f"Zoom {zoom_lo:.2f}-{zoom_hi:.2f}s (full resolution)")
    axes[1].set_xlabel("time (s)"); axes[1].set_ylabel("V")

    plt.tight_layout()
    out = args.out or base.with_name(base.name + "_plot.png")
    plt.savefig(out, dpi=130)
    print(f"wrote {out}")


if __name__ == "__main__":
    main()
