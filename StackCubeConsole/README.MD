# StackCube console

Run these commands from the repository root. Connect the CoreS3 USB-C port before uploading. Use `--upload-port COMx` if PlatformIO detects multiple ports.

```powershell
pio run -d StackCubeConsole
pio run -d StackCubeConsole -t upload
pio device monitor -b 115200
```

The first build downloads the pinned [Espressif ELF loader](https://components.espressif.com/components/espressif/elf_loader/versions/2.1.0/readme) into the project build cache.

## SD card layout

Use a FAT32 card and give each game a directory under `/games`. Put the game module and its manifest in the same directory.

```text
/games/Snake/.gameinfo
/games/Snake/game.elf
/games/Snake/box.png
/settings/settings.json
```

```json
{
  "title": "Snake",
  "binary": "game.elf",
  "cover": "box.png"
}
```

The manifest can be named `.gameinfo` or `name.gameinfo`. Set `title` for the carousel. The default binary is `game.bin`, and the default art filenames are `box.cover` and `box.png`. For a custom name, set `cover` to either filename extension. The launcher tries the `.cover` file first, then the PNG with the same base name, then a DVD case with the game title. For direct PNG loading, use images up to 256 × 384 pixels and 256 KiB. The carousel lists up to 512 games.

Convert your PNG on your PC to skip decoding on the console. Run these commands from the repository root with the path to your PNG.

```powershell
python -m pip install Pillow
python StackCubeConsole/convert_cover.py path/to/box.png
```

Copy the generated `box.cover` to the game folder. The script accepts multiple PNG paths and puts each output beside its source. It preserves the aspect ratio within 99 × 150 pixels and composites transparency against the DVD case background. Each `.cover` starts with `SCV1`, then little-endian 16-bit width and height, then row-major little-endian RGB565 pixels. A full-size file is 29,708 bytes.

The launcher mounts the card when the CoreS3 detects insertion and rebuilds the catalog on a successful mount. If a mount fails, the launcher cycles the SD slot's power and retries once. Reinsert the card to retry after a second failure. The Arduino SD driver can log one `CMD0` failure during cleanup after physical ejection. Keep the card inserted while copying a game or saving settings. If a game copy fails, the launcher keeps its existing boot selection and shows the error.

## Boot and recovery

The ESP32-S3 bootloader starts the factory launcher. For `game.elf`, the launcher loads the native module from SD into PSRAM and calls through the [versioned game API](../StackCubeAPI/README.MD). The running firmware supplies display, input, and audio. Use modules from sources you trust. They run with the same access as the console firmware. Resetting the console returns to the launcher.

Legacy `game.bin` images use the 4 MiB game partition and reboot into their bundled drivers. Older games that reach `Console::begin()` select the launcher for the next reset. If an older game fails earlier, upload the console again over USB-C to reset the boot selection. Rebuild an older game once as an ELF module to use the console's current drivers.

## Settings

Select **Settings** after the last game in the carousel. F2 and the controller's Start/Menu button also open it. Tap a D-pad direction or arrow key to move one step in the carousel, between settings controls, or through an open drop-down. Hold the direction to repeat. A or Enter activates the focused control. Select a Wi-Fi text field to open the on-screen keyboard. A controller moves the pointer while that keyboard is open.

Set the Wi-Fi SSID and password, select a region and city (for example, **Europe** and **Warsaw**), then select **Save**. Leave the SSID blank to disable Wi-Fi. The time-zone list uses the IANA 2026f database and includes daylight saving rules.

Set **Audio output** to **Built-in speaker**, **HDMI**, or **Off**. Set **Master volume**, **Music volume**, and **Effects volume** with the mouse, or focus a slider and press Left or Right. Master controls the overall level. Set a slider to **0%** to mute the corresponding audio. Select **Save** to apply your choices the next time you launch a game. Missing settings default to **Master 100%, Music 10%, and Effects 25%**. Games read audio assets from the same SD folder as their binary.

The launcher creates `/settings/settings.json` when it mounts a card without that file. It carries over valid settings from an earlier `/settings/console.json`. An existing custom POSIX rule stays active until you choose a region and city. Wi-Fi credentials are stored in plain text on the card. The console synchronizes the CoreS3 RTC over NTP when connected. An unset clock displays `--:--`.

## Hardware checks

1. Power on with HDMI attached. Confirm the launcher appears and USB-C reports HDMI and MAX3421E initialization.
2. Plug in a keyboard, a wired Xbox controller, and a mouse individually. Repeat through a powered USB 2.0 hub.
3. Insert a card with two games. Browse both directions, launch Snake, and return with Esc or B. Leave the controller plugged in and confirm the D-pad moves the carousel after returning. Launch Snake again to check that its state resets.
4. Swap cards in the launcher. Confirm the catalog and Wi-Fi settings reflect the new card.
5. Save a time zone and Wi-Fi network in Settings. Reinsert the card and confirm the connection icon and clock update.

### Binary compatibility check

Update the console firmware with the same SD card and launch the same `game.elf` again. Verify rendering, input, audio, and return to the launcher.

### Audio check

1. Save **Built-in speaker**, launch Snake, and collect food. Listen for the pickup sound over the music on the CoreS3.
2. Repeat with **HDMI** on a display or receiver that supports HDMI audio.
3. Use the default **Music volume** of 10% and **Effects volume** of 25%. Let Snake hit a wall and listen for the collision over the music.
4. Set **Master volume** to 0%, save, and relaunch Snake. Confirm that the game is silent and Settings still shows the selected output.
