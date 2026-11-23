# StackCube Snake

Use the [SD card packager](../README.MD) to build and package the example games. To install Snake individually:

1. Build with `pio run -d Games/StackCubeSnake` from the repository root.
2. Copy the whole [`sd/games/Snake`](sd/games/Snake) folder to `/games` on the card, including the WAV files. Copy `.pio/build/cores3/firmware.bin` from this project into that directory as `game.bin`.
3. Insert the card and select **Snake**. Steer with the arrow keys, WASD, D-pad, or stick. Enter or A restarts after a collision. Esc or B returns to the launcher.

Snake preloads pickup and collision sounds, then streams looping music while you play. Choose the speaker, HDMI, or Off in the launcher's **Audio output** setting.
