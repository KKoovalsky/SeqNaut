# Seqnaut — Project Context for Claude

## What this is

Seqnaut is the working name for an embedded-first, platform-portable audio and sequencer library.
The goal is a library that makes it trivial to build:

- Synthesizers with sequencers
- Drum machines
- Guitar pedals and multi-effects
- Vocal effects processors
- OSC-controlled live performance instruments
- ...basically anything that involves DSP Audio and/or live audio processing.

...on any platform: Teensy 4.x, Electrosmith Daisy, ESP32, RP2040, and desktop (Linux/macOS via
PortAudio) for testing without hardware.

The design philosophy is: **clean abstractions at every boundary, borrow and adapt proven code
wherever it exists, reinvent nothing that does not need reinventing.**

---

## Vision — Modular Embedded Synth / Groovebox with OSC Control

The concrete application driving this library is a **fully headless, OSC-controlled groovebox**:
an embedded DSP system controllable in real-time via OSC over UART, enabling external control
from scripts, CLI tools, or AI-generated patterns.

### Main components

#### Sequencer Engine
- **Piano Roll sequencer** — note-based (pitch + duration), driven by OSC, supports real-time
  pattern generation (e.g. AI-generated sequences sent over UART)
- **Step Grid sequencer** — classic 16/32-step grid, triggers instruments per step, optimised
  for rhythm and loop-based workflows
- Both sequencers already implemented in a platform-agnostic form (see below)

#### Sampler Engine
- Sample-based playback, tightly integrated with the step grid
- Pitch shifting via playback rate control
- Runtime sample upload via OSC
- Dynamic sample bank management

#### Synth Engine (Polysynth)
- Polyphonic synthesizer: basic waveforms (sine, saw, square, etc.), multiple voices
- Voice allocation and stealing
- All parameters OSC-addressable
- Driven by the piano roll sequencer

#### OSC over UART Interface
- OSC as the primary control protocol; UART as the transport (embedded-friendly, no WiFi needed)
- Full control surface: sequencer, sampler, synth parameters
- Data transfer: samples, patterns
- Designed for real-time interaction with external systems (AI, scripts, live coding environments)

### Workflow

```
External system (AI / CLI / script)
  → generates sequences or control data
  → sends OSC messages over UART
       ↓
Device
  → receives and parses OSC
  → updates sequencer / engines
  → renders audio in real time
```

### MVP scope

- Piano Roll sequencer
- Polysynth (8 voices)
- OSC over UART control
- No UI — OSC-only control

---

## What has been implemented so far

The current codebase is a working proof-of-concept on Teensy 4.1 + PJRC Audio Shield (SGTL5000).
It plays a two-bar A minor pentatonic melody via a sawtooth oscillator at 120 BPM.

### Sequencer engine (platform-agnostic, all in `.h` files)

| File | Purpose |
|---|---|
| `SeqConfig.h` | Central compile-time constants: `PPQN=48`, pool sizes |
| `ITimeProvider.h` | Abstract wall-clock time source; `ArduinoTimeProvider` concrete impl |
| `IClockable.h` | Interface for anything driven by the master clock: `tick(absoluteTick)` |
| `MusicalTime.h` | Value type for musical time in ticks; named factories `beats(n)`, `ticks(n)` |
| `MasterClock.h` | BPM → microsecond tick engine; RAII `ClockConnection` handle |
| `IInstrument.h` | Contract between sequencer and audio generator: `noteOn`, `noteOff` |
| `PianoRoll.h` | Event-list sequencer; stateless `tick()`; fixed pool; stable `NoteId`s |
| `StepGrid.h` | Grid-based sequencer with per-track lengths for polyrhythm |

### Audio layer (Teensy-specific, temporary)

| File | Purpose |
|---|---|
| `SawtoothVoice.h` | Adapter: `AudioSynthWaveform` × `IInstrument` via multiple inheritance |
| `SeqnautDemo.ino` | Wiring: audio graph + sequencer; two-bar demo melody |

### Key design decisions already made

- **Dynamic allocation is the goal.** The library is intended to behave like a DAW engine —
  instruments, piano rolls, and sequences should be spawnable at runtime, growable, and
  destroyable. Some current modules use fixed pools (e.g. `MAX_PR_NOTES`, `MAX_CLOCKABLES`) as a
  temporary simplification; these are expected to migrate to heap-backed containers (`std::vector`,
  `std::list`, or custom allocators) as the design matures. Do not cement static pool assumptions
  deeper into the codebase.
- **`MusicalTime` as a named value type.** Eliminates unit ambiguity (ticks vs beats vs samples).
- **`MasterClock` is time-source-injected.** Takes `ITimeProvider&` — swap in a mock for tests.
- **`PianoRoll::tick()` is stateless.** No internal playhead; position = `absoluteTick % length`.
  Safe to call from any context; trivial to test without running hardware.
- **`ClockConnection` is RAII, move-only, `[[nodiscard]]`.** Deregisters automatically on
  destruction. Compiler warns if the caller discards the handle accidentally.
- **`SawtoothVoice` uses multiple inheritance as an adapter.** No wrapper object, no internal
  `AudioConnection`. The voice *is* an `AudioStream` and *is* an `IInstrument` simultaneously.
- **`noteOff()` sets amplitude to 0.** Rectangular envelope by design — envelope shaping is a
  separate DSP concern, not baked into the voice primitive.

---

## Intended target architecture

```
┌──────────────────────────────────────────────────────────┐
│                    Application Layer                      │
│   Synth  DrumMachine  GuitarPedal  VocalFX  OSCServer   │
├──────────────────────────────────────────────────────────┤
│               Control / Sequencer Layer                   │
│   MasterClock  PianoRoll  StepGrid  MIDI  OSC            │
│   (already built, already platform-agnostic)             │
├──────────────────────────────────────────────────────────┤
│                  DSP / Node Graph Layer                   │
│   AudioNode  Oscillator  Filter  Envelope  Mixer         │
│   Delay  Reverb  Compressor  Chorus  ...                 │
│   (DaisySP-backed; blocks at boundary, per-sample inside)│
├──────────────────────────────────────────────────────────┤
│                    Platform HAL                           │
│   IAudioDriver   ITimeProvider   IMidiIO   IGpio         │
│   [Teensy]  [Daisy]  [ESP32]  [PortAudio]  [WASM]       │
└──────────────────────────────────────────────────────────┘
```

### Audio processing model

- **Block-based at all platform boundaries.** Every real audio API (I2S DMA, PortAudio, JACK,
  WASM AudioWorklet) delivers and expects blocks. Fighting this means reinventing buffer
  management. Standard block size: 128 samples (configurable per platform).
- **Per-sample DSP inside the block loop.** DaisySP objects (`Oscillator`, `Svf`, `Adsr`, etc.)
  call `Process()` once per sample inside `AudioNode::process()`. These two models nest cleanly —
  they are not in conflict.
- **Float internally; convert at the HAL boundary only.** `int16_t` / `int32_t` only where the
  hardware demands it.

---

## Code to borrow and adapt — do not reinvent

The policy is: **identify the best existing implementation, adapt it to our interfaces, credit the
source.** We are not building a research project; we are building a usable library.

### From DaisySP (`electro-smith/DaisySP`)

Primary DSP backend. Every algorithm here is already real-time safe and embedded-tested.

- `Oscillator` — wavetable oscillator with bandlimited waveforms
- `Svf` — state-variable filter (LP/HP/BP/Notch in one object)
- `Adsr` — attack/decay/sustain/release envelope
- `Oscillator` in LFO mode — modulation sources
- `Decimator`, `Bitcrush` — lo-fi effects
- `Overdrive`, `Wavefolder` — distortion/waveshaping
- `Delay<N>` — templated delay line
- `ReverbSc` — Schroeder reverb
- `Chorus`, `Flanger`, `Phaser` — modulation effects
- `Compressor` — dynamics

Adapt: wrap each in an `AudioNode` subclass that calls `Process()` in a sample loop inside
`process(block)`.

### From Teensy Audio (`PaulStoffregen/Audio`)

The audio graph infrastructure is worth studying closely; some pieces are worth adapting directly.

- **Block memory pool** — reference-counted `audio_block_t` pool; the allocation/release
  discipline is well-thought-out. Evaluate whether a heap-backed pool (growing on demand) suits
  us better than a fixed-size one given the DAW-like extensibility goal.
- **`AudioStream` update graph** — the construction-order traversal model; adapt for our
  `AudioNode` graph if we need dynamic routing.
- **Waveform generators** (`synth_waveform`) — bandlimited sawtooth/square/triangle
  implementation details, especially the BLEP (band-limited step) correction math.
- **`effect_envelope`** — the 8-sample-chunk envelope state machine is a good embedded pattern.
- **`AudioMixer4`** — simple gain-summing mixer; trivial but the implementation is clean.

Do NOT borrow: anything tied to `AudioStream` base class, `AudioConnection`, or `DMAMEM` — these
are Teensy/IMXRT-specific.

### From Amy (`shorepine/amy`)

Amy is a complete synthesizer engine, not a composable library, so borrowing is selective.

- **Voice allocation / stealing policy** — Amy's approach to polyphonic voice management (oldest
  note stealing, priority queues) is well-considered and worth adapting for our voice pool.
- **Partial/oscillator abstraction** — the concept of a `partial` as a sub-voice building block
  maps well onto our node model.
- **MIDI note → frequency table** — Amy's precomputed lookup is faster than `powf()` per note.
- **Additive synthesis engine** — if we want additive synth support, Amy's partials approach is
  a good starting point.
- **Envelope curves** — Amy supports multiple curve shapes (linear, exponential, log); the math
  is worth lifting.

Do NOT borrow: the event scheduler, the audio output loop, the WiFi/serial message parser — these
assume Amy owns the process.

### From per-platform vendor libraries

#### Teensy / IMXRT (NXP i.MX RT1062)

- **I2S driver** — `AudioOutputI2S` / `AudioInputI2S` DMA setup; adapt for `TeensyAudioDriver`.
  The double-buffered DMA + ISR pattern is correct and battle-tested.
- **SGTL5000 codec driver** — `AudioControlSGTL5000`; lift directly, it is just I2C register
  writes. Wrap behind an `ICodecDriver` interface.
- **ADC input** — `AudioInputAnalog`; adapt for CV/expression pedal input.
- **`elapsedMicros`** — already abstracted behind `ITimeProvider`; keep as the Teensy impl.
- **USB MIDI** — `usb_midi` from Teensyduino cores; wrap behind `IMidiIO`.

#### Electrosmith Daisy (STM32H750)

- **`DaisyHardware`** audio callback — maps directly to `IAudioDriver::Callback`.
- **SAI (I2S equivalent)** DMA driver — already abstracted in `libdaisy`.
- **WM8731 / PCM3060 codec drivers** — lift from `libdaisy`, wrap behind `ICodecDriver`.
- **`System::GetNow()`** — maps to `ITimeProvider`.

#### ESP32

- **I2S driver** (`driver/i2s.h`) — DMA-based; wrap for `IAudioDriver`.
- **`esp_timer_get_time()`** — microsecond timer; maps to `ITimeProvider`.
- **WiFi stack** — for OSC over UDP; wrap behind `INetworkTransport`.

---

## Agreed next step

1. Define `IAudioDriver` interface.
2. Implement `TeensyAudioDriver` (wraps current I2S + ISR setup).
3. Implement `PortAudioDriver` (desktop, for testing without hardware).
4. Define `AudioNode` base class with `process(float**, float**, size_t)`.
5. Port `SawtoothVoice` to an `AudioNode` backed by `daisysp::Oscillator`.

**Success criterion:** the two-bar demo melody plays identically on Teensy and on the desktop
via PortAudio, driven by the same sequencer engine code.

---

## Things to keep in mind

- `update()` / `process()` always runs in interrupt context on embedded targets. No blocking,
  no heap allocation, no `Serial.print` inside the audio path.
- `uint32_t` subtraction for microsecond timing is intentional — handles `micros()` wrap-around
  (~71 min) correctly without branching.
- `ClockConnection` must be stored (e.g., as a `static` or member). Discarding the return value
  of `masterClock.connect()` immediately deregisters — `[[nodiscard]]` makes the compiler warn.
- Fixed pool constants in `SeqConfig.h` (`MAX_PR_NOTES`, `MAX_CLOCKABLES`, etc.) are temporary
  scaffolding. Do not add new fixed-size pools — prefer `std::vector` or similar for new code,
  and flag existing pools for migration when touching those modules.
- PPQN=48 is the master resolution. All musical time arithmetic flows from this. Change it in
  `SeqConfig.h` only — every tick interval recalculates automatically.
- `prompts/` holds local planning/working notes (live TODO checklists, session summaries) —
  gitignored on purpose, not part of the shipped project. Check there for more context on
  current work before assuming a fresh clone has the full picture.
