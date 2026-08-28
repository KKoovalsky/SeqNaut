#!/usr/bin/env python3
"""Play an audio file with every other PipeWire playback stream muted for the
duration, so a background notification/app sound can't leak into a test
signal that's feeding the AFE during an automated test loop.

Tags our own playback with a distinctive PIPEWIRE_PROPS node.description (set
via env var, which pipewire-alsa respects) so it's never mistaken for "someone
else" and muted — no PID/name matching needed, which would be ambiguous if the
competing sound also happens to be `aplay`. Mutes everything else up front,
then keeps watching for newly-appearing streams (e.g. a notification that
starts mid-playback) for the rest of the run. Restores every mute state it
touched when done, even on Ctrl+C.
"""
import argparse
import json
import os
import signal
import subprocess
import sys
import threading
import time

OWN_TAG = "claude-exclusive-play"


def pw_streams():
    """Return {node_id: (mute_bool, node_description)} for all current
    Stream/Output/Audio nodes. Tolerates pw-dump occasionally emitting extra
    trailing data under heavy node-churn by decoding only the first JSON value."""
    out = subprocess.run(["pw-dump"], capture_output=True, text=True, check=True).stdout
    try:
        data, _ = json.JSONDecoder().raw_decode(out)
    except json.JSONDecodeError:
        return {}
    streams = {}
    for obj in data:
        info = obj.get("info") or {}
        props = info.get("props") or {}
        if "Stream/Output/Audio" not in props.get("media.class", ""):
            continue
        params = info.get("params") or {}
        prop_param = (params.get("Props") or [{}])[0]
        streams[obj["id"]] = (prop_param.get("mute", False), props.get("node.description"))
    return streams


def set_mute(node_id, mute):
    subprocess.run(["wpctl", "set-mute", str(node_id), "1" if mute else "0"],
                   capture_output=True)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("file", help="Audio file to play (via aplay)")
    p.add_argument("--poll-ms", type=int, default=200, help="Watcher poll interval while playing")
    args = p.parse_args()

    touched = {}  # node_id -> original mute state, for everything we've muted

    def mute_new():
        try:
            current = pw_streams()
        except (subprocess.SubprocessError, OSError):
            return
        # Reconcile first: PipeWire can recycle node ids, so something we
        # muted earlier (as "someone else") may since have been torn down and
        # its id reassigned to our own tagged stream. Release it if so.
        for node_id in list(touched):
            if node_id in current and current[node_id][1] == OWN_TAG:
                set_mute(node_id, touched.pop(node_id))
        for node_id, (was_muted, desc) in current.items():
            if desc == OWN_TAG:
                # WirePlumber's per-application stream-restore remembers
                # mute/volume by app identity (e.g. the "aplay" binary) —
                # muting an unrelated stream from the same app earlier in
                # this run can get silently re-applied to our own tagged
                # stream when it starts. Force it back every time we see it.
                if was_muted:
                    set_mute(node_id, False)
                continue
            if node_id in touched:
                continue
            touched[node_id] = was_muted
            if not was_muted:
                set_mute(node_id, True)

    print("Muting other playback streams...")
    mute_new()
    print(f"  {len(touched)} stream(s) muted.")

    env = dict(os.environ, PIPEWIRE_PROPS=f'{{ node.description = "{OWN_TAG}" }}')
    proc = subprocess.Popen(["aplay", args.file], env=env)
    stop_watch = threading.Event()

    def watcher():
        while not stop_watch.is_set():
            mute_new()
            time.sleep(args.poll_ms / 1000)

    t = threading.Thread(target=watcher, daemon=True)
    t.start()

    def restore(*_):
        stop_watch.set()
        for node_id, was_muted in touched.items():
            if not was_muted:
                set_mute(node_id, False)
        print("Restored prior mute states.")

    def on_signal(signum, _frame):
        proc.terminate()
        restore()
        sys.exit(128 + signum)

    signal.signal(signal.SIGINT, on_signal)
    signal.signal(signal.SIGTERM, on_signal)
    try:
        proc.wait()
    finally:
        restore()


if __name__ == "__main__":
    main()
