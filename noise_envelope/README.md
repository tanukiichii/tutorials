# Drum Sample Synth

This is a small **drum sound synthesizer** written in C.

Instead of playing prerecorded samples, this program **generates drum sounds mathematically** using oscillators, envelopes, noise, and pitch modulation.

The generated waveform is also visualized in real time so you can see how the sound looks.

The goal of this project is to explore **how electronic drum sounds are constructed from simple DSP building blocks**.

---

# How it works

Each drum sound is generated from a few basic components:

### Oscillators
Two sine oscillators can be mixed together.

```
OSC1 frequency
OSC2 frequency
OSC2 mix
```

This allows simple tonal shaping of the drum body.

---

### Pitch Envelope (FM)

A fast pitch drop is what gives kick drums and toms their punch.

This is implemented as a **decaying frequency modulation**:
```
freq = base_freq + fm_amount * exp(-fm_k * t)
```

---

### Noise

Noise is used to simulate the **snare wires** or **hi-hat metallic texture**.
```
noise = random() * noise_amount
```

---

### Amplitude Envelope

Controls how quickly the sound fades out.
```
amp = exp(-env_k * t)
```

This is what creates the typical **percussive decay**.

---

# Requirements

- SDL2
- SDL2_ttf
- C compiler (GCC / Clang)

### macOS

```bash
brew install sdl2 sdl2_ttf
```
### On Linux:

```bash
sudo apt install libsdl2-dev libsdl2-ttf-dev
```
### Build instructions

This project contains a `Makefile`. To build the program, simply run:

```bash
make        
```
This will compile the code and produce the executable.
To run the program:
```bash
./drum_synth       
```

Or u can build and run the programm with one command:
```bash
make run
```
### Notes 
- Sample rate: 44100 Hz
- Mono output
- The font path is currently set to a macOS system font (/System/Library/Fonts/Supplemental/Arial.ttf). Update the path if running on another OS.
