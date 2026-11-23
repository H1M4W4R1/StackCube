# StackCube game API

Start with [StackCubeBasicExample](../Games/StackCubeBasicExample/README.MD). Its PlatformIO build produces a native CoreS3 `game.elf` module in `.pio/build/cores3`.

```powershell
pio run -d Games/StackCubeBasicExample
```

Define `setup()` and `loop()`. The launcher calls `setup()` once per launch and calls `loop()` with a fresh input snapshot. Call `Console::begin()` during setup, then read the current snapshot with `Console::poll()`. Draw through `Console::screen()` on the 640 × 360 RGB565 surface. Pair every `startWrite()` with `endWrite()` before calling services that access USB or SD. Use `Console::millis()`, `Console::random()`, and `Console::sleep()` for timing and randomness.

`Input::held` and `Input::pressed` provide logical buttons across keyboards and controllers. `Input::pads` exposes four per-device slots with buttons and normalized axes. Positive X points right and positive Y points down. The input snapshot also includes the latest new keyboard HID key and US-layout text character, plus relative mouse motion, wheel delta, and button state.

Call `Console::setRumble(slot, left, right)` after reading input to set the two Xbox controller motors. Use the zero-based `Input::pads` slot and strengths from 0 to 255. Set both strengths to zero to stop rumble. The method returns whether a supported controller was available to receive the request.

Call `Console::returnToLauncher()` and return from `loop()` to exit the game. The console stops game audio and rumble, unloads the module, and redraws the launcher.

## Binary compatibility

The SDK links into each game. It binds to the running console through the V1 structures in [`StackCubeAbi.h`](src/StackCubeAbi.h). The console owns the HAL, drivers, and shared SPI bus. Game modules link against the SDK and the listed C runtime imports in `Games/build_game.py`.

Keep V1 field layouts, function signatures, and behavior available in future firmware. Add new features in a separate ABI version when the interface needs to change, with a V1 adapter for published games. Verify each release by launching the same previously built `game.elf` on the new firmware and exercising rendering, input, audio, and exit.

## Game audio

Put the `.wav` files next to your game's `.gameinfo`. Pass the filename, including `.wav`. Use signed 16-bit PCM at **48 kHz**, in mono or stereo. Mono uses 96 KB/s of SD bandwidth and works well for short effects and background tracks.

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
- `play_sfx()` plays an effect once. Up to four effects can overlap. Music plays more quietly while effects are audible, then returns to its usual level. Extra triggers are dropped when all four effect voices are playing. Keep each effect at or below 192,000 PCM bytes (2 seconds mono or 1 second stereo).
- `preload_sfx()` loads synchronously and reports whether the effect is ready. Use it in `setup()` or a loading screen. The cache holds up to eight effects and can evict idle clips. A cache miss during gameplay loads the effect in chunks before playing it.
- `play_bg_track()` and `play_sfx()` report whether the request was accepted. Missing or invalid WAV files are reported over serial during loading. Calls return `false` while the corresponding volume is 0% or output is off or unavailable.

During gameplay, the console services the audio queue between calls to `loop()`, with at most one 4 KiB PCM read each iteration. Four music buffers provide read-ahead across short stalls. Return promptly from `loop()` so the console can service stereo music at least 60 times per second. The Arduino task handles SD access alongside input and display, while the I²S task on the other core mixes and plays from memory.
