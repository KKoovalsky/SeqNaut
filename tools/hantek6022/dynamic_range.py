#!/usr/bin/env python3
"""Analyze dynamic range of a capture_until_stop.py raw capture.

Segments the capture into discrete "events" (note/strum bursts separated by
quiet gaps) and reports each event's peak level, plus the overall played
dynamic range (loudest event vs. quietest event) and the floor noise level.
Used to compare a live-guitar reference capture against a PC-playback
verification capture of the same corpus.
"""
import argparse
import json
from pathlib import Path

import numpy as np


def load_ch1(base: Path):
    meta = json.loads(base.with_suffix(".json").read_text())
    raw = np.fromfile(base.parent / f"{base.name}_ch1.raw", dtype=np.uint8)
    sr = meta["sample_rate_hz"]
    v = (raw.astype(np.float32) - 128) * (5.0 / (meta["voltage_range_ch1_index"] << 7))
    return v, sr, meta


def find_events(v, sr, block_ms=10.0, gap_merge_ms=100.0, min_event_ms=15.0, thresh_mult=4.0):
    block = max(1, int(sr * block_ms / 1000))
    n_blocks = len(v) // block
    vb = v[:n_blocks * block].reshape(n_blocks, block)
    bias = np.median(v)
    ac = vb - bias
    rms_env = np.sqrt(np.mean(ac ** 2, axis=1))
    peak_env = np.abs(ac).max(axis=1)

    floor = np.percentile(rms_env, 10)
    thresh = max(floor * thresh_mult, 1e-4)
    above = rms_env > thresh

    gap_merge_blocks = max(1, int(gap_merge_ms / block_ms))
    min_event_blocks = max(1, int(min_event_ms / block_ms))

    events = []
    i = 0
    n = len(above)
    while i < n:
        if not above[i]:
            i += 1
            continue
        start = i
        j = i
        while j < n:
            if above[j]:
                j += 1
                continue
            gap_end = j
            while gap_end < n and not above[gap_end] and (gap_end - j) < gap_merge_blocks:
                gap_end += 1
            if gap_end < n and above[gap_end]:
                j = gap_end
                continue
            break
        end = j
        if end - start >= min_event_blocks:
            events.append((start, end))
        i = end + 1

    results = []
    for start, end in events:
        results.append({
            "t_start_s": start * block_ms / 1000,
            "t_end_s": end * block_ms / 1000,
            "peak_v": float(peak_env[start:end].max()),
            "rms_v": float(np.sqrt(np.mean(rms_env[start:end] ** 2))),
        })
    return results, float(floor), float(bias)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("name", help="Capture basename, e.g. captures/teststrum")
    p.add_argument("--block-ms", type=float, default=10.0)
    p.add_argument("--gap-merge-ms", type=float, default=100.0, help="Merge events separated by gaps shorter than this")
    p.add_argument("--min-event-ms", type=float, default=15.0, help="Discard events shorter than this")
    p.add_argument("--thresh-mult", type=float, default=4.0, help="Event threshold = floor RMS * this")
    p.add_argument("--skip-start-s", type=float, default=0.05, help="Ignore this much at the very start (startup transient)")
    args = p.parse_args()

    base = Path(args.name)
    v, sr, meta = load_ch1(base)
    skip = int(args.skip_start_s * sr)
    v = v[skip:]

    events, floor, bias = find_events(v, sr, args.block_ms, args.gap_merge_ms, args.min_event_ms, args.thresh_mult)
    if not events:
        print("No events found — check thresholds.")
        return

    peaks = np.array([e["peak_v"] for e in events])
    order = np.argsort(peaks)

    print(f"{base.name}: {len(v)/sr:.2f}s analyzed, bias={bias:.4f}V, floor RMS={floor:.5f}V")
    print(f"{len(events)} events detected (block={args.block_ms}ms, gap-merge={args.gap_merge_ms}ms, "
          f"min-event={args.min_event_ms}ms, thresh={args.thresh_mult}x floor)\n")

    quietest = events[order[0]]
    loudest = events[order[-1]]
    print(f"Quietest event: t={quietest['t_start_s']:.2f}-{quietest['t_end_s']:.2f}s  "
          f"peak={quietest['peak_v']:.4f}V  rms={quietest['rms_v']:.4f}V")
    print(f"Loudest event:  t={loudest['t_start_s']:.2f}-{loudest['t_end_s']:.2f}s  "
          f"peak={loudest['peak_v']:.4f}V  rms={loudest['rms_v']:.4f}V")

    played_range_db = 20 * np.log10(loudest["peak_v"] / quietest["peak_v"])
    floor_range_db = 20 * np.log10(loudest["peak_v"] / floor)
    print(f"\nPlayed dynamic range (loudest/quietest event peak): {played_range_db:.1f} dB")
    print(f"Peak-to-floor range (loudest event / noise floor):   {floor_range_db:.1f} dB")

    print("\nAll events, quietest to loudest:")
    for idx in order:
        e = events[idx]
        db = 20 * np.log10(e["peak_v"] / quietest["peak_v"])
        print(f"  t={e['t_start_s']:7.2f}-{e['t_end_s']:7.2f}s  peak={e['peak_v']:.4f}V  "
              f"rms={e['rms_v']:.4f}V  ({db:+5.1f} dB rel. quietest)")


if __name__ == "__main__":
    main()
