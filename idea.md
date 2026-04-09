# Project: Modular Embedded Synth / Groovebox with OSC Control (UART)

## Core Idea

An embedded DSP-based music system that can be **fully controlled in real-time via OSC over UART**, enabling:

* external control (CLI / scripts / AI)
* real-time sequence generation (e.g. AI-generated patterns)
* hybrid workflow (automated + manual sequencing)

---

## Main Components

### 1. Sequencer Engine (Core System)

#### Piano Roll Sequencer

* note-based sequencing (pitch + duration)
* controlled via OSC messages
* supports real-time pattern generation (e.g. from AI)

#### Grid Sequencer (Step Sequencer)

* classic step grid (e.g. 16/32 steps)
* triggers instruments/samplers per step
* optimized for rhythm and loop-based workflows

---

### 2. Sampler Engine

* sample-based playback engine
* tightly integrated with grid sequencer (step-triggered samples)

**Features:**

* pitch shifting (playback rate / pitch control)
* runtime sample upload via OSC
* dynamic sample bank management

---

### 3. Synth Engine (Polysynth)

* simple polyphonic synthesizer based on oscillators

**Assumptions:**

* basic waveforms (sine, saw, square, etc.)
* multiple voices (polyphony)
* minimal complexity (no advanced synthesis initially)

**Core features:**

* voice allocation (basic voice management)
* OSC-controlled parameters
* driven by piano roll sequencer

---

### 4. OSC over UART Interface

* OSC as the primary control protocol
* UART as lightweight transport layer (embedded-friendly)

**Capabilities:**

* full control over:
  * sequencer
  * sampler
  * synth
* data transfer:
  * samples
  * patterns
* real-time interaction with external systems (e.g. AI, scripts)

---

## Workflow / Use Case

1. External system (AI / CLI / script):
   * generates sequences or control data
   * sends OSC messages over UART

2. Device:
   * receives and parses OSC
   * updates sequencer / engines
   * renders audio in real time

3. Optional:
   * manual interaction (grid/piano roll editing)

---

## High-Level Architecture

* Audio Engine (DSP layer)
* Sequencer Engine
* Sampler Engine
* Synth Engine
* OSC Server (UART transport)
* Memory Manager (sample storage)

---

## Project Goals

* fully headless control via OSC
* seamless integration with AI-generated content
* modular architecture (extensible instruments)
* embedded-first design (MCU/DSP friendly)

---

## MVP Scope (Minimal Version)

* Piano Roll sequencer
* Polysynth (8 voices)
* OSC over UART control
* No UI — OSC-only control
