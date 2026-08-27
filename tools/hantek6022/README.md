# Hantek 6022BE tooling

Scripted acquisition from a Hantek 6022BE USB oscilloscope, for capturing and verifying
real analog output (e.g. audio-out from a Teensy running this project) against what the
DSP code is expected to produce.

Two paths, depending on what you need:

## Quick, fixed-duration capture — `sigrok-cli`

For a one-off bounded capture (known start/stop time), system `sigrok-cli` is enough and
needs no venv:

```
sudo apt install -y sigrok-cli libsigrok4t64 sigrok-firmware-fx2lafw
sigrok-cli -d hantek-6xxx --config samplerate=1m --time 5000 -O csv -o out.csv
```

`sigrok-cli`'s `hantek-6xxx` driver does one fixed-size acquisition per invocation — it
cannot run indefinitely and be stopped early (interrupting it produces zero data; see the
skill for why). For that, use the script below.

## Continuous capture, stop whenever — `capture_until_stop.py`

```
venv/bin/python capture_until_stop.py --name mytest --samplerate-index 0x01 --channels 1
# ... Ctrl+C whenever ...
venv/bin/python convert_raw.py captures/mytest --format csv
```

Writes raw 8-bit ADC samples straight to disk as they arrive (safe to run indefinitely,
no in-memory accumulation) plus a `<name>.json` sidecar with sample rate / voltage range /
channel count. `convert_raw.py` turns a raw capture + its sidecar into CSV or `.npy`
(time + scaled voltage columns).

Run `capture_until_stop.py --help` / `convert_raw.py --help` for all options (voltage
range per channel, 2-channel capture, USB transfer tuning).

### One-time host setup (already done on this machine)

```
sudo cp 60-hantek-6022-usb.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules && sudo udevadm trigger
python3 -m venv venv && venv/bin/pip install -r requirements.txt
```

The udev rule grants USB access to the local session (via `uaccess`) for both the
scope's pre-firmware (`04b4:6022`) and post-firmware (`04b5:6022`) USB IDs. **After
installing/changing the rule, fully power-cycle the scope** (unplug both USB-A plugs,
not just one) — see the `hantek6022` skill for why a partial replug isn't enough.

## Attribution

`PyHT6022/` is a trimmed, patched vendor copy of the Linux/Python parts of
[jhoenicke/Hantek6022API](https://github.com/jhoenicke/Hantek6022API) (GPLv2, see
`PyHT6022-LICENSE`). Local patches: `array.tostring()` → `array.tobytes()` (removed in
Python 3.9+), and `60-hantek-6022-usb.rules` fixed from the upstream copy's invalid
`TAGS+=` operator to the correct singular `TAG+=`.
