# StackCube game API

Start with [StackCubeBasicExample](../Games/StackCubeBasicExample/README.MD) and build your game with PlatformIO for CoreS3 using the shared configuration.

```powershell
pio run -d Games/StackCubeBasicExample
```

Call `Console::begin()` from `setup()`, then call `Console::poll()` every loop. Draw through `Console::screen()` on the 640 × 360 HDMI surface. If a game opens a drawing transaction, end it before polling, since the screen and input hub share SPI.

`Input::held` and `Input::pressed` provide logical buttons across keyboards and controllers. `Input::pads` exposes four per-device slots with buttons and normalized axes. Positive X points right and positive Y points down. The input snapshot also includes the latest new keyboard HID key and US-layout text character, plus relative mouse motion, wheel delta, and button state.

Call `Console::setRumble(slot, left, right)` after polling input to set the two Xbox controller motors. Use the zero-based `Input::pads` slot and strengths from 0 to 255. Set both strengths to zero to stop rumble. The method returns whether a supported controller was available to receive the request. Controller enumeration starts with the motors idle. Rebuild and replace older SD game binaries to update the USB driver bundled in each game.

Call `Console::returnToLauncher()` when the player exits. The API lives in `StackCubeAPI`. Launcher code and SD management live in `StackCubeConsole`. The HAL and drivers live in `StackCubeHardware`.

## Game audio

Put the `.wav` files next to your game's `.gameinfo` and launch the rebuilt game from the updated launcher. Pass the filename, including `.wav`. Use signed 16-bit PCM at **48 kHz**, in mono or stereo. Mono uses 96 KB/s of SD bandwidth and works well for short effects and background tracks.

```powershell
ffmpeg -i source.ogg -map_metadata -1 -ar 48000 -ac 1 -c:a pcm_s16le background.wav
```

```cpp
// During setup, after console.begin():
console.preload_sfx("pickup.wav");
console.play_bg_track("background.wav");

// During gameplay:
console.play_sfx("pickup.wav");
```

- `play_bg_track()` loops one track. Request another file to change tracks, or call `stop_bg_track()` to stop it. Already queued music drains before a switch or stop is audible.
- `play_sfx()` plays an effect once. Up to four effects can overlap with the music. Extra triggers are dropped when all four effect voices are playing. Keep each effect at or below 192,000 PCM bytes (2 seconds mono or 1 second stereo).
- `preload_sfx()` loads synchronously and reports whether the effect is ready. Use it in `setup()` or a loading screen. The cache holds up to eight effects and can evict idle clips. A cache miss during gameplay loads the effect in chunks before playing it.
- `play_bg_track()` and `play_sfx()` report whether the request was accepted. Missing or invalid WAV files are reported over serial during loading. Calls return `false` while sound is off or unavailable.

During gameplay, `Console::poll()` services the audio queue with at most one 4 KiB PCM read per call. Continue polling throughout the game and end display transactions first. Poll at least once per 60 Hz frame to supply stereo music. The game loop handles SD access alongside input and display, while the I²S task mixes and plays from memory independently.
