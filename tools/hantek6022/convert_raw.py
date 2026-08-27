#!/usr/bin/env python3
"""Convert a capture_until_stop.py raw capture (+ its .json sidecar) to CSV or .npy."""
import argparse
import json
from pathlib import Path

import numpy as np


def scale(raw_u8, voltage_range_index):
    # Same formula as PyHT6022.LibUsbScope.Oscilloscope.scale_read_data
    scale_factor = 5.0 / (voltage_range_index << 7)
    return (raw_u8.astype(np.float32) - 128) * scale_factor


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("name", help="Capture basename, e.g. captures/smoketest2 (expects <name>.json, <name>_ch1.raw[, _ch2.raw])")
    p.add_argument("--format", choices=("csv", "npy"), default="csv")
    args = p.parse_args()

    base = Path(args.name)
    meta = json.loads(base.with_suffix(".json").read_text())

    ch1 = np.fromfile(base.parent / f"{base.name}_ch1.raw", dtype=np.uint8)
    v1 = scale(ch1, meta["voltage_range_ch1_index"])
    t = np.arange(len(v1)) / meta["sample_rate_hz"]

    if meta["channels"] == 2:
        ch2 = np.fromfile(base.parent / f"{base.name}_ch2.raw", dtype=np.uint8)
        n = min(len(v1), len(ch2))
        v2 = scale(ch2[:n], meta["voltage_range_ch2_index"])
        v1, t = v1[:n], t[:n]

    if args.format == "npy":
        out = base.with_suffix(".npy")
        arr = np.column_stack([t, v1, v2]) if meta["channels"] == 2 else np.column_stack([t, v1])
        np.save(out, arr)
    else:
        out = base.with_suffix(".csv")
        with open(out, "w") as f:
            if meta["channels"] == 2:
                f.write("time_s,ch1_v,ch2_v\n")
                for row in zip(t, v1, v2):
                    f.write("%.9f,%.4f,%.4f\n" % row)
            else:
                f.write("time_s,ch1_v\n")
                for row in zip(t, v1):
                    f.write("%.9f,%.4f\n" % row)

    print(f"Wrote {out} ({len(v1)} samples, {meta['sample_rate_label']}, {meta['voltage_range_ch1_label']})")


if __name__ == "__main__":
    main()
