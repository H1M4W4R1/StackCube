# StackCube Snake

1. Build with `pio run -d StackCubeSnake` from the repository root.
2. Create `/games/Snake` on the SD card. Copy [the manifest](sd/games/Snake/.gameinfo) and [box cover](sd/games/Snake/box.png) there. Copy `.pio/build/cores3/firmware.bin` from this project into that directory as `game.bin`.
3. Insert the card and select **Snake**. Steer with the arrow keys, WASD, D-pad, or stick. Enter or A restarts after a collision. Esc or B returns to the launcher.
