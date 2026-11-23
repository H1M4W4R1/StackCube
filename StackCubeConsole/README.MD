# StackCube console

Run these commands from the repository root. Connect the CoreS3 USB-C port before uploading. Use `--upload-port COMx` if PlatformIO detects multiple ports.

```powershell
pio run -d StackCubeConsole
pio run -d StackCubeConsole -t upload
pio device monitor -b 115200
```

## SD card layout

Use a FAT32 card and give each game a directory under `/games`. Put the PlatformIO **application** binary and its manifest in the same directory.

```text
/games/Snake/.gameinfo
/games/Snake/game.bin
/games/Snake/box.png
/settings/settings.json
```

```json
{
  "title": "Snake",
  "binary": "game.bin",
  "cover": "box.png"
}
```

The manifest can be named `.gameinfo` or `name.gameinfo`. Set `title` for the carousel. The default binary and cover filenames are `game.bin` and `box.png`. The launcher supplies a DVD case with the title if the folder has no cover. Use a portrait PNG up to 256 × 384 pixels and 256 KiB. The carousel lists up to 512 games.

The launcher mounts the card when the CoreS3 detects insertion and rebuilds the catalog on a successful mount. If a mount fails, the launcher cycles the SD slot's power and retries once. Reinsert the card to retry after a second failure. The Arduino SD driver can log one `CMD0` failure during cleanup after physical ejection. Keep the card inserted while copying a game or saving settings. If a game copy fails, the launcher keeps its existing boot selection and shows the error.

## Boot and recovery

The ESP32-S3 bootloader starts the factory launcher. When you select a game, the launcher streams its application binary into the 4 MiB game partition, verifies the image, and requests a reboot into that partition. Use a CoreS3 PlatformIO `firmware.bin` built with the [game API](../StackCubeAPI/README.MD).

Games that call `Console::begin()` select the launcher for the next reset before initializing the HDMI or USB hardware. `Console::returnToLauncher()` reboots immediately. If a game fails before reaching the API, upload the console again over USB-C. That upload resets the boot selection to the factory launcher.

## Settings

Select **Settings** after the last game in the carousel. F2 and the controller's Start/Menu button also open it. Use the D-pad or the arrow keys to move between controls. A or Enter activates the focused control. Select a Wi-Fi text field to open the on-screen keyboard. A controller moves the pointer while that keyboard is open.

Set the Wi-Fi SSID and password, select a region and city (for example, **Europe** and **Warsaw**), then select **Save**. Leave the SSID blank to disable Wi-Fi. The time-zone list uses the IANA 2026f database and includes daylight saving rules.

The launcher creates `/settings/settings.json` when it mounts a card without that file. It carries over valid settings from an earlier `/settings/console.json`. An existing custom POSIX rule stays active until you choose a region and city. Wi-Fi credentials are stored in plain text on the card. The console synchronizes the CoreS3 RTC over NTP when connected. An unset clock displays `--:--`.

## Hardware checks

1. Power on with HDMI attached. Confirm the launcher appears and USB-C reports HDMI and MAX3421E initialization.
2. Plug in a keyboard, a wired Xbox controller, and a mouse individually. Repeat through a powered USB 2.0 hub.
3. Insert a card with two games. Browse both directions, launch Snake, and return with Esc or B.
4. Swap cards in the launcher. Confirm the catalog and Wi-Fi settings reflect the new card.
5. Save a time zone and Wi-Fi network in Settings. Reinsert the card and confirm the connection icon and clock update.
