
# Multi-Oscillator Synthesizer

A simple real-time additive synthesizer written in C using **SDL2** and **SDL_ttf**.  
It allows you to create and control multiple oscillators, mix them in real-time, and visualize the waveform.

## Features

- Up to **8 oscillators** simultaneously
- Supports four wave types:
  - Sine
  - Square
  - Triangle
  - Saw
- Real-time audio playback via SDL
- Visual waveform display
- Adjustable **frequency**, **amplitude**, and **wave type** for each oscillator
- Click to select oscillators, add or remove them dynamically

## Controls

| Action                         | Key / Interaction                          |
|--------------------------------|--------------------------------------------|
| Play / Pause                    | `SPACE`                                    |
| Change selected oscillator type | `TAB`                                      |
| Increase frequency              | `UP` arrow                                 |
| Decrease frequency              | `DOWN` arrow                               |
| Increase amplitude              | `RIGHT` arrow                               |
| Decrease amplitude              | `LEFT` arrow                                |
| Select oscillator               | Click on the oscillator in the list       |
| Add new oscillator              | Click `+` button                           |
| Remove selected oscillator      | `BACKSPACE`                                |
| Exit program                    | `ESC`                                      |

## Requirements

- SDL2
- SDL2_ttf
- C compiler (GCC/Clang)

### On macOS:

You can install SDL2 via [Homebrew](https://brew.sh/):

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
This will compile the code and produce the executable (e.g., wave_generator).
To run the program:
```bash
./add_synth       
```
Or u can build and run the programm with one command:
```bash
make run
```
To remove compiled files and start fresh, you can use:
```bash
make clean
```
### Notes 

- The program uses a fixed sample rate of 44100 Hz.

- The font path is currently set to a macOS system font (/System/Library/Fonts/Supplemental/Arial.ttf). Update the path if running on another OS.

- Maximum 8 oscillators allowed at a time.