# Wave Generator

A simple real-time wave generator written in C using **SDL2** and **SDL_ttf**. It allows you to play and visualize basic waveforms (sine, square, triangle, and sawtooth) with adjustable frequency and amplitude.

## Features

- Generates the following waveforms:
  - Sine wave
  - Square wave
  - Triangle wave
  - Saw wave
- Real-time audio playback via SDL
- Adjustable **frequency** and **amplitude** using keyboard
- Displays current waveform, frequency, and amplitude on the screen

## Controls

| Key        | Action                    |
|------------|---------------------------|
| `1`        | Select Sine wave          |
| `2`        | Select Square wave        |
| `3`        | Select Triangle wave      |
| `4`        | Select Saw wave           |
| `Up`       | Increase frequency by 50 Hz |
| `Down`     | Decrease frequency by 50 Hz |
| `Right`    | Increase amplitude by 0.05 |
| `Left`     | Decrease amplitude by 0.05 |
| `Escape`   | Exit program              |

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
./wave_generator       
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

- The font path is currently set to a macOS system font (/System/Library/Fonts/SFNS.ttf). Update the path if you run on another OS.