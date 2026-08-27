#!/usr/bin/env python3
"""Hantek 6022BE: capture continuously until Ctrl+C (SIGINT).

Writes raw 8-bit ADC samples straight to disk as they arrive (no in-memory
accumulation, so it's safe to run indefinitely) plus a JSON sidecar with
enough info (sample rate, voltage range, channel count) to rescale and
timestamp the raw bytes afterward. See convert_raw.py for that step.
"""
import argparse
import json
import signal
import sys
import time
from pathlib import Path

from PyHT6022.LibUsbScope import Oscilloscope

VOLTAGE_RANGE_HELP = "0x01=+/-5V 0x02=+/-2.5V 0x05=+/-1V 0x0a=+/-500mV"
INTERFACE_LABELS = {0: "bulk", 1: "iso-fast", 2: "iso-med", 3: "iso-slow"}


def auto_int(x):
    return int(x, 0)


def parse_args():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--samplerate-index", type=auto_int, default=0x01,
                    help="Key into Oscilloscope.SAMPLE_RATES, e.g. 0x01=1MS/s, 0x04=4MS/s (default 0x01)")
    p.add_argument("--channels", type=int, choices=(1, 2), default=1)
    p.add_argument("--vrange1", type=auto_int, default=0x01, help=f"CH1 voltage range. {VOLTAGE_RANGE_HELP}")
    p.add_argument("--vrange2", type=auto_int, default=0x01, help=f"CH2 voltage range (if --channels 2). {VOLTAGE_RANGE_HELP}")
    p.add_argument("--interface", type=int, choices=(0, 1, 2, 3), default=1,
                    help="0=bulk 1=iso-fast 2=iso-med 3=iso-slow (default 1, best for gapless capture)")
    p.add_argument("--chunk-points", type=int, default=4096, help="USB transfer size in samples")
    p.add_argument("--outstanding-transfers", type=int, default=10)
    p.add_argument("--outdir", type=Path, default=Path(__file__).parent / "captures")
    p.add_argument("--name", default=None, help="Basename for output files (default: capture_<timestamp>)")
    return p.parse_args()


def main():
    args = parse_args()
    name = args.name or time.strftime("capture_%Y%m%dT%H%M%S")
    args.outdir.mkdir(parents=True, exist_ok=True)

    ch1_path = args.outdir / f"{name}_ch1.raw"
    ch2_path = args.outdir / f"{name}_ch2.raw"
    meta_path = args.outdir / f"{name}.json"

    scope = Oscilloscope()
    if not scope.setup():
        print("No Hantek 6022BE found.", file=sys.stderr)
        sys.exit(1)
    scope.open_handle()
    scope.flash_firmware()  # firmware lives in FX2 RAM only, must reload every power-up
    scope.set_interface(args.interface)
    scope.set_num_channels(args.channels)
    scope.set_sample_rate(args.samplerate_index)
    scope.set_ch1_voltage_range(args.vrange1)
    if args.channels == 2:
        scope.set_ch2_voltage_range(args.vrange2)
    time.sleep(1)  # let the device settle after reconfiguration

    rate_label, rate_hz = scope.SAMPLE_RATES.get(args.samplerate_index, ("?", 0))
    vrange1_label = scope.VOLTAGE_RANGES.get(args.vrange1, ("?",))[0]
    vrange2_label = scope.VOLTAGE_RANGES.get(args.vrange2, ("?",))[0] if args.channels == 2 else None

    meta = {
        "device": "Hantek 6022BE",
        "channels": args.channels,
        "sample_rate_index": args.samplerate_index,
        "sample_rate_label": rate_label,
        "sample_rate_hz": rate_hz,
        "voltage_range_ch1_index": args.vrange1,
        "voltage_range_ch1_label": vrange1_label,
        "voltage_range_ch2_index": args.vrange2 if args.channels == 2 else None,
        "voltage_range_ch2_label": vrange2_label,
        "interface": INTERFACE_LABELS[args.interface],
        "format": "raw_uint8",
        "start_time": time.time(),
    }
    meta_path.write_text(json.dumps(meta, indent=2))

    ch1_file = open(ch1_path, "wb")
    ch2_file = open(ch2_path, "wb") if args.channels == 2 else None

    stop = False

    def on_sigint(signum, frame):
        nonlocal stop
        stop = True

    signal.signal(signal.SIGINT, on_sigint)

    def callback(ch1_data, ch2_data):
        ch1_file.write(ch1_data)
        if ch2_file is not None and ch2_data:
            ch2_file.write(ch2_data)

    print(f"Recording CH1{'+CH2' if args.channels == 2 else ''} at {rate_label} "
          f"({vrange1_label}) to {args.outdir}/{name}_ch*.raw -- Ctrl+C to stop.")

    start = time.time()
    scope.start_capture()
    shutdown_event = scope.read_async(callback, args.chunk_points,
                                       outstanding_transfers=args.outstanding_transfers, raw=True)
    try:
        while not stop:
            scope.context.handleEventsTimeout(tv=0.1)  # bounded wait so SIGINT is checked promptly
    finally:
        scope.stop_capture()
        shutdown_event.set()
        # drain already-in-flight transfers so the last chunk isn't lost
        drain_until = time.time() + 1.0
        while time.time() < drain_until:
            scope.context.handleEventsTimeout(tv=0.1)
        scope.close_handle()
        ch1_file.close()
        if ch2_file is not None:
            ch2_file.close()

    elapsed = time.time() - start
    ch1_bytes = ch1_path.stat().st_size
    meta["elapsed_seconds"] = elapsed
    meta["ch1_samples"] = ch1_bytes
    if args.channels == 2:
        meta["ch2_samples"] = ch2_path.stat().st_size
    meta_path.write_text(json.dumps(meta, indent=2))

    print(f"Stopped after {elapsed:.2f}s. CH1: {ch1_bytes} samples "
          f"({ch1_bytes / rate_hz:.3f}s nominal at {rate_label}).")


if __name__ == "__main__":
    main()
