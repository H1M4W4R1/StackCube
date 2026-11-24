# StackCube

Build the [console firmware](StackCubeConsole/README.MD), then [prepare a FAT32 microSD card with the example games](Games/README.MD) to try the full launch flow.

1. Assemble the modules using [HARDWARE.MD](HARDWARE.MD). Connect a monitor over HDMI and a keyboard, mouse, or wired controller to the USB module.
2. Flash the console from the CoreS3 USB-C port. That port also carries the 115200-baud debug console.
3. Insert the prepared card and launch a game. Use the arrow keys or D-pad to browse and Enter or A to play. Esc or B returns from the example games.

| Project | Contents |
| --- | --- |
| [StackCubeConsole](StackCubeConsole/README.MD) | Game bootloader, SD catalog, HDMI launcher, and settings |
| [StackCubeHardware](StackCubeHardware/README.MD) | Shared CoreS3 HAL, MAX3421E USB input drivers, and HDMI output |
| [StackCubeAPI](StackCubeAPI/README.MD) | C++ interface for games |
| [StackCubeSnake](Games/StackCubeSnake/README.MD) | Snake built with the game API |
| [StackCubeBlocks](Games/StackCubeBlocks/README.MD) | Falling-block game with a 10 × 21 board and row-clearing chains |
| [StackCubeBasicExample](Games/StackCubeBasicExample/README.MD) | Small example for keyboard, mouse, and controller input |
| [I2C Scanner](Games/StackCubeI2cScanner/README.MD) | Live I²C address matrix for DIN Base Port B or Port C |
| [UART Console](Games/StackCubeUartConsole/README.MD) | Keyboard-driven DINBase UART monitor and sender |
