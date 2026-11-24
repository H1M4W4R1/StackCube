# Prepare a StackCube microSD card

From the repository root, run the command for your OS. Both commands require Python 3 and PlatformIO Core (`pio` on your `PATH`).

| OS | Command |
| --- | --- |
| Windows | `py -3 Games/prepare_sd.py` |
| Linux | `python3 Games/prepare_sd.py` |

The script builds Snake, StackCube Blocks, Basic Example, I2C Scanner, and UART Console and regenerates `Games/dist`. Copy the `games` folder **inside** `Games/dist` to the root of a FAT32 microSD card. The launcher should list all five entries.

To include another game, add its PlatformIO project directly under `Games/` using `platformio-game.ini` and a single `sd/games/<GameName>/` folder containing its `.gameinfo` manifest and any assets. Set `"binary": "game.elf"` in the manifest. The script copies the built module there using the name in that field.
