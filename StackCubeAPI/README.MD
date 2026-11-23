# StackCube game API

Start with [StackCubeBasicExample](../StackCubeBasicExample/README.MD) and build your game with PlatformIO for CoreS3 using the shared configuration.

```powershell
pio run -d StackCubeBasicExample
```

Call `Console::begin()` from `setup()`, then call `Console::poll()` every loop. Draw through `Console::screen()` on the 640 × 360 HDMI surface. If a game opens a drawing transaction, end it before polling, since the screen and input hub share SPI.

`Input::held` and `Input::pressed` provide logical buttons across keyboards and controllers. `Input::pads` exposes four per-device slots with buttons and normalized axes. Positive X points right and positive Y points down. The input snapshot also includes the latest new keyboard HID key and US-layout text character, plus relative mouse motion, wheel delta, and button state.

Call `Console::returnToLauncher()` when the player exits. The API lives in `StackCubeAPI`. Launcher code and SD management live in `StackCubeConsole`. The HAL and drivers live in `StackCubeHardware`.
