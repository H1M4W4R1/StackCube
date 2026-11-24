# I2C Scanner

1. Build and flash the [current StackCube console firmware](../../StackCubeConsole/README.MD) for address-only probe support.
2. Run `pio run -d Games/StackCubeI2cScanner -e cores3` from the repository root.
3. Copy [the manifest](sd/games/I2cScanner/.gameinfo), [box cover](sd/games/I2cScanner/box.png), and `Games/StackCubeI2cScanner/.pio/build/cores3/game.elf` to `/games/I2cScanner` on the FAT32 microSD card. The [SD preparation script](../README.MD) also packages the app automatically.
4. Launch **I2C Scanner** from the carousel. Use the left and right arrow keys or D-pad to select DIN Base Port B or Port C. Press Esc or the controller B button to exit.

The app starts on Port B and repeatedly probes at 100 kHz. The matrix shows 7-bit addresses `00` through `7F`. Addresses that acknowledge are green. Addresses with no response and reserved addresses are gray. The scan skips the reserved ranges `00`–`07` and `78`–`7F`.

Connect yellow to SDA, white to SCL, and black to ground. Use 3.3 V signal levels and pull-ups to 3.3 V on both signal lines. See the [DIN Base port pinout](../../StackCubeAPI/README.MD#din-base-ports).
