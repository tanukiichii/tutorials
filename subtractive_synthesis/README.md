# Subtractive Synthesizer 

A real-time polyphonic subtractive synthesizer written in **C** using **SDL2** and **SDL_ttf**.

The project contains **two implementations** of the same synthesizer:

- `subtractive_synth.c` — simplified filter implementation  
- `true_subtractive_synth.c` — more accurate biquad-based filter implementation  

Both versions have **identical UI and functionality**, but differ in the internal filter processing algorithm.


## Features

- Polyphony (up to 8 simultaneous voices)
- Three oscillator waveforms:
  - Saw
  - Square
  - Triangle
- Up to 5 filters in series
- Filter types:
  - LPF (Low-pass)
  - HPF (High-pass)
  - BPF (Band-pass)
  - Notch
- Real-time waveform visualization
- On-screen piano keyboard
- Octave shifting
- Dynamic filter creation and removal


## Two Filter Implementations

### 1️⃣ subtractive_synth.c (Simplified Filter)

Uses a lightweight state-variable style filter:

- Fast and simple
- Good for learning purposes
- Not physically accurate
- Uses internal `z1`, `z2` states

This version is easier to understand and modify.

---

### 2️⃣ true_subtractive_synth.c (Biquad Filter)

Implements a proper **biquad IIR filter** using normalized coefficients:

- Computes `b0, b1, b2, a1, a2`
- Based on digital filter design equations
- More stable and realistic
- More accurate resonance behavior

This version provides a more correct subtractive synthesis approach.


## Controls

### 🎹 Playing Notes

Keyboard mapping: 
``` bash
Z S X C F V G B N J M K , L . /
```

These keys correspond to a piano layout shown on screen.

- Press key → note on  
- Release key → note off  
- `1` → Octave down  
- `2` → Octave up  

---

###  Oscillator

- `TAB` → Change waveform (Saw / Square / Triangle)

---

###  Filters

- `+` button or `=` key → Add filter  
- Click filter → Select filter  
- `Q` → Change filter type  
- `UP / DOWN` → Adjust cutoff frequency  
- `LEFT / RIGHT` → Adjust resonance  
- `BACKSPACE` → Remove selected filter  

---

### General

- `ESC` → Exit


## Requirements

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

The project uses a single Makefile.

Inside the Makefile, the source file is selected via the SRC variable.

Example:

```makefile
# Choose one:
SRC = subtractive_synth.c #true_subtractive_synth.c
```
To build:

```bash
make        
```
This will compile the code and produce the executable.
To run the program:
```bash
./subtractive_synth        
```
(or the name defined in the Makefile)

Or u can build and run the programm with one command:
```bash
make run
```
To remove compiled files and start fresh, you can use:
```bash
make clean
```
### Notes 
- Sample rate: 44100 Hz
- Mono output
- Maximum 8 voices
- Maximum 5 filters in series
- The font path is currently set to a macOS system font (/System/Library/Fonts/Supplemental/Arial.ttf). Update the path if running on another OS.
