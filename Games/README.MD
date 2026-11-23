# Prepare a StackCube microSD card

From the repository root, run the command for your OS. Both commands require Python 3 and PlatformIO Core (`pio` on your `PATH`).

| OS | Command |
| --- | --- |
| Windows | `py -3 Games/prepare_sd.py` |
| Linux | `python3 Games/prepare_sd.py` |

The script builds Snake and Input Example and regenerates `Games/dist`. Copy the `games` folder **inside** `Games/dist` to the root of a FAT32 microSD card. The launcher should find `/games/Snake/.gameinfo` and `/games/InputExample/.gameinfo`.

To include another game, add its PlatformIO project directly under `Games/` with a `cores3` environment and a single `sd/games/<GameName>/` folder containing its `.gameinfo` manifest and any assets. The script copies the application binary there as `game.bin` by default or uses the name in the manifest's `binary` field.
