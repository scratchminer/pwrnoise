# pwrnoise
An emulator for the Power Noise fantasy sound chip, part of the [Hexheld](https://github.com/Hexheld/) fantasy console.

Design by [jvsTSX](https://github.com/jvsTSX/), code by scratchminer.

## Building
Requires SDL2 for audio playback.

- `git clone https://github.com/scratchminer/pwrnoise`
- `mkdir build && cd build`
- `cmake ..`
- `make`

## File format
Power Noise register dumps are quite simple.

- The start of the file must contain the 8-byte signature "PWRNOISE", followed by the clock rate of the chip as an unsigned 32-bit integer.
- After this comes the register data. One byte has the register number, and the next has the value to write to that register.
- If the register is `0xFF`, the next three bytes taken together contain the number of cycles to wait before continuing to the next register.

Two testing examples are in the `examples/` directory:
- `test_noise.bin` contains a snippet of one noise channel, with the taps on bits 15 and 8 of the noise LFSR.
- `test_slope.bin` contains a snippet of the slope channel, configured to produce a square wave.