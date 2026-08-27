# Third-Party Licenses

## DaisySP

Used for: `daisysp::Svf` (state-variable filter), as the HPF stage inside
`src/TransientDetector.h`.

Source: https://github.com/electro-smith/DaisySP

Fetched at CMake configure time via `FetchContent` (see `CMakeLists.txt` /
`cmake/DaisySPArduinoShim.cmake`) into `.deps/daisysp-src/` (gitignored) — DaisySP's own source
is never committed into this repository. `cmake/DaisySPArduinoShim.cmake` symlinks the two files
actually used (`Source/Filters/svf.{h,cpp}`, `Source/Utility/dsp.h`) into `src/Filters/` so
arduino-cli's library resolver can find the path-qualified `#include "Filters/svf.h"`. Those
symlinks are regenerated on every configure and are not meant to be committed.

`Source/Filters/svf.{h,cpp}` and `Source/Utility/dsp.h` live under DaisySP's top-level `Source/`
tree, not under `DaisySP-LGPL/` (a separate subtree for a handful of LGPL-licensed modules) — so
the applicable license is the MIT license below, reproduced from DaisySP's `LICENSE` file as
fetched at commit-time from the URL above. Because the compiled Teensy firmware links Svf's
object code directly into the flashed binary, this notice travels with the repo per the MIT
license's "included in all copies or substantial portions" requirement.

```
DaisySP, copyright (c) 2020 Electrosmith, Corp.

Published under the MIT license:

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

## PyHT6022 (from Hantek6022API)

Used for: `tools/hantek6022/` — scripted continuous acquisition from a Hantek 6022BE USB
oscilloscope, for capturing real analog signals to verify against expected DSP output.
Host-side tooling only; not linked into any firmware built from this repository.

Source: https://github.com/jhoenicke/Hantek6022API

A trimmed copy (the Linux/Python `PyHT6022` package only — no Windows SDK, no firmware
build toolchain) is committed at `tools/hantek6022/PyHT6022/`, with two local patches:
`array.tostring()` → `array.tobytes()` (removed in Python 3.9+), and the vendored
`60-hantek-6022-usb.rules` corrected from the upstream copy's invalid `TAGS+=` udev
operator to the correct singular `TAG+=`. Full license text at
`tools/hantek6022/PyHT6022-LICENSE` (GPLv2).
