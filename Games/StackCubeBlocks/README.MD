# StackCubeBlocks

Fit pieces of two to five blocks into a **10-column × 21-row** board. Fill a row to clear it. Each consecutive piece that clears at least one row increases your score multiplier. A placement that clears no rows resets the chain. Pieces fall faster every six cleared rows.

## Build and play

1. From the repository root, run `pio run -d Games/StackCubeBlocks -e cores3` to build for CoreS3.
2. Create `/games/StackCubeBlocks` on your FAT32 microSD card. Copy [`.gameinfo`](sd/games/StackCubeBlocks/.gameinfo) and [the box cover](sd/games/StackCubeBlocks/box.png) there. Copy `Games/StackCubeBlocks/.pio/build/cores3/firmware.bin` into the same folder as `game.bin`.
3. Insert the card and select **StackCubeBlocks** in the launcher.

The [SD card packager](../README.MD) can build and stage all bundled games in one command.

## Controls

| Action | Keyboard | Controller |
| --- | --- | --- |
| Move | Left/Right or A/D | D-pad or left stick |
| Rotate clockwise | Up or W | Up |
| Soft drop | Hold Down or S | Hold Down |
| Hard drop or restart after game over | Space or Enter | A |
| Return to launcher | Esc or F2 | B or Menu |

The outline shows where the falling piece will land. A hard drop locks the piece immediately. The round ends when the next piece has no room to enter the board.
