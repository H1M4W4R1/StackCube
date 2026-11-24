# Basic Example

Use the [SD card packager](../README.MD) to build and package the example games. To install Basic Example individually:

1. Build with `pio run -d Games/StackCubeBasicExample` from the repository root.
2. Create `/games/InputExample` on the SD card. Copy [the manifest](sd/games/InputExample/.gameinfo) and [box cover](sd/games/InputExample/box.png) there. Copy `.pio/build/cores3/game.elf` from this project into that directory.
3. Select **Basic Example**. Move the dot with a mouse, arrow keys, D-pad, or stick. Esc or B returns to the launcher.
