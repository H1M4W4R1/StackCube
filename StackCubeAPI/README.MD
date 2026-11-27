# StackCube game API

Start with [StackCubeBasicExample](../Games/StackCubeBasicExample/README.MD). Its PlatformIO build produces a native CoreS3 `game.elf` module in `.pio/build/cores3`.

```powershell
pio run -d Games/StackCubeBasicExample
```

Define `setup()` and `loop()`. Call `console.begin()` in `setup()`. The launcher calls `setup()` once per launch and supplies a fresh input snapshot before each `loop()`. Use `console.millis()`, `console.random()`, and `console.sleep()` for timing and randomness.

| Accessor | Service |
| --- | --- |
| `console.screens().raw()` | M5GFX drawing, RGB565 sprites, clipping and SD images |
| `console.screens().ui()` | Game-owned LVGL screen, native widgets and USB mouse interaction |
| `console.data()` | File size and offset-based reads from the game folder on SD |
| `console.sound()` | Cached WAV effects and looping WAV music |
| `console.input()` / `console.ports()` | Input bindings, rumble, DIN and USB module GPIO |

## Display and SD assets

The 640 × 360 display starts in M5GFX mode. Use `rgb565(r, g, b)` for primitive colors and `pushImage()` for row-major RGB565 pixels with an optional transparent color. Use `startWrite()` and `endWrite()` to batch nearby drawing operations. Finish the batch before calling a service that accesses the SD card or USB module.

```cpp
auto& raw = console.screens().raw();
raw.fillScreen(Black);
raw.drawImage("art/title.png", 40, 24); // PNG, JPG, JPEG or BMP

uint8_t header[16];
int32_t bytesRead = console.data().read("levels/level1.dat", header, sizeof(header), 0);
```

Paths for images and data are relative to the folder containing the game's `.gameinfo`. Subfolders are supported. `data().size()` and `read()` return byte counts or `-1` on error. At end of file, a read returns zero. Use the offset to load large assets in chunks between calls to `loop()`.

Include `<lvgl.h>` to use the LVGL 8.3.11 headers provided by the game build. Open the screen before creating children. The root and its children live until `ui().close()` or game exit.

```cpp
if (auto* root = console.screens().ui().open()) {
    auto* label = lv_label_create(root);
    lv_label_set_text(label, "Ready");
    lv_obj_center(label);
}
```

The firmware handles display refresh and USB mouse input while the LVGL screen is open. Use `InputService` to map keyboard and gamepad controls in an LVGL game. Both screen backends can be used during a game. Raw drawing is suspended while the LVGL screen is open. `close()` takes effect after the current callback or game frame, so redraw the raw scene in the next `loop()`. Use `removeWidget()` to delete an individual child. The available native LVGL calls are listed in [StackCubeLvglExports.h](src/StackCubeLvglExports.h). The module build reports unsupported imports.

## Input bindings

Bindings run on the game thread before `loop()`. Each callback receives an `InputEvent` and an optional context pointer. For example, use a USB HID usage code such as `0x2c` for Space.

```cpp
auto binding = console.input().bindKeyPressSingle(0x2c, [](const InputEvent&, void* context) {
    *static_cast<bool*>(context) = true;
}, &jumpRequested);
```

- Use `bindKey()`, `bindMouseButton()` or `bindGamepadButton()` with `Gesture::Down` or `Gesture::Up` for immediate edges.
- `Single` fires 250 ms after a short release so a second short release can produce `Double` instead. `Hold` fires once at 500 ms. Disconnecting cancels pending gestures.
- `bindMouseMove()` and `bindMouseWheel()` report relative deltas. `bindGamepadStick()` reports changed X/Y values, including a return to center. Slot numbers are 0 through 3.
- Keep the nonzero binding ID for `unbind()`. A return of zero signals invalid arguments or a full binding table (32 active bindings). Use `clear()` to remove all bindings.

Logical buttons combine input from keyboards and controllers. `Input::held` contains the buttons down on every frame. `Input::pressed` contains the new presses. For an action that should repeat at a steady rate, use `input.pressed | input.repeated`. A held button first repeats after 350 ms and repeats every 90 ms until release. Each button has its own timer. Use `pressed` for one-shot actions such as exiting.

`console.input().snapshot()` exposes four per-device gamepad slots with buttons and normalized axes. Positive X points right and positive Y points down. The snapshot also includes held keyboard HID usages via `keyDown()`, the latest new key and US-layout text character, plus relative mouse motion, wheel delta, and button state.

Call `console.input().setRumble(slot, left, right)` to set the two Xbox controller motors. Use the zero-based `Input::pads` slot and strengths from 0 to 255. Set both strengths to zero to stop rumble. The method returns whether a supported controller was available to receive the request.

Call `Console::returnToLauncher()` and return from `loop()` to exit the game. The console releases both DIN ports, stops game audio and rumble, unloads the module, and redraws the launcher.

## DIN Base ports

Get a port with `console.ports().din(DinPortId::B)` or `console.ports().din(DinPortId::C)`. Each port can run GPIO, UART, or I²C. Call `end()` before changing protocols on a port. The console also releases the port when the game exits.

| Port | Yellow wire | White wire |
| --- | --- | --- |
| B | GPIO9 / UART TX / I²C SDA | GPIO8 / UART RX / I²C SCL |
| C | GPIO17 / UART TX / I²C SDA | GPIO18 / UART RX / I²C SCL |

Use 3.3 V logic and a common ground. For I²C, use pull-ups to 3.3 V on SDA and SCL. The red wire supplies 5 V. [M5Stack Base DIN pinout](https://docs.m5stack.com/en/base/Base_DIN)

```cpp
auto portB = console.ports().din(DinPortId::B);
if (portB.pinMode(DinPin::White, PinMode::InputPullUp)) {
    const bool buttonDown = portB.digitalRead(DinPin::White) == 0;
    // Use buttonDown in gameplay. A button wired to ground reads low while held.
}

auto portC = console.ports().din(DinPortId::C);
if (portC.beginUart(115200)) {
    uint8_t bytes[32];
    const auto count = portC.uartRead(bytes, sizeof(bytes));
    // Process bytes when count is positive.
}
```

- **GPIO** supports input, input pull-up, input pull-down, push-pull output and open-drain output. `digitalRead()` returns `0`, `1`, or `-1` on error. `digitalWrite()` returns whether the write succeeded.
- **UART** opens at 8N1 with `beginUart(baud)`. Pass a `UartSettings` value to `beginUart()` to set 5–8 data bits, none/even/odd parity, and 1, 1.5, or 2 stop bits on current firmware. For example, `portC.beginUart({9600, 7, UartParity::Even, UartStopBits::One})` selects 9600 baud, 7E1. `uartRead()` and `uartWrite()` return the number of bytes read or queued, `0` when none were transferred, or `-1` on error. Retry partial writes in later frames. `uartTxIdle()` returns `true` once the open UART has finished sending. Let pending writes drain before changing settings.
- **UART pin swapping** uses `UartSettings::swapPins`. Set it to `true` to use white as TX and yellow as RX. `beginUart(settings)` returns `false` for a swap request on firmware without V6 services.
- **I²C** uses 7-bit controller mode at 10–400 kHz, with 100 kHz as the default. Call `beginI2c()` before `i2cTransfer(address, tx, txSize, rx, rxSize)`. A zero-length side can use `nullptr`. Supplying both buffers performs a write followed by a repeated start and read. A transfer returns `false` on error or timeout. Port B and Port C share one external controller, which the console routes to the selected port for each transfer.
- **I²C address probes** use `i2cProbe(address)` after `beginI2c()`. A probe sends a START, the address with the write bit, and a STOP. It sends no data bytes. The result is `I2cProbeResult::Acknowledged` for an ACK, `NoResponse` for a NACK or bus error, or `Unavailable` on older firmware. For a general scan, probe `0x08` through `0x77` to skip reserved addresses.

## USB module GPIO

The M5Stack USB v1.2 module has separate `IN` and `OUT` headers with channels `0` through `4` on each. `console.ports().readUsbGpio(channel)` reads the `IN` header and returns `0`, `1`, or `-1` if the channel or module is unavailable. `console.ports().writeUsbGpio(channel, high)` drives the `OUT` header and returns whether the request was accepted. The pin directions are fixed. [Module pinout](https://docs.m5stack.com/en/module/Module_USB_v1.2)

Use 3.3 V logic and a common ground. The inputs have internal pull-ups, so a button wired between `IN 0` and `G` reads low while held. Read it each time through `loop()` for continuous input. Call the GPIO methods outside display write batches. The console drives the outputs low on game exit. [MAX3421E datasheet](https://www.analog.com/media/en/technical-documentation/data-sheets/MAX3421E.pdf)

```cpp
const int32_t level = console.ports().readUsbGpio(0);
if (level >= 0) console.ports().writeUsbGpio(0, level == 0);
```

## Firmware requirements

Use current console firmware for SD files, raw image drawing, LVGL, and held keyboard bindings. These features use the V7 service table and V3 input snapshot.

The SDK links into each game. The console owns the HAL, drivers, and shared SPI bus. Game modules link against the SDK and resolve the supported C runtime and LVGL calls from the firmware.

The V2 services and input in [`StackCubeAbi.h`](src/StackCubeAbi.h) extend the V1 structures at the end. V3 adds USB module GPIO, V4 adds I²C address probing, V5 adds UART framing and TX-idle reporting, and V6 adds UART pin swapping. V2 through V6 use the V2 input layout. The firmware advertises the larger services size through the existing V1 entry point, preserving the layouts and button semantics published games use. On V1 firmware, the newer SDK reports DIN port operations as unavailable and `Input::repeated` is zero. On V2 through V4 firmware, the `UartSettings` overload supports 8N1 and `uartTxIdle()` returns `false`. Verify each release by launching the same previously built `game.elf` on the new firmware and exercising rendering, input, audio, and exit.

## Game audio

Put the `.wav` files next to your game's `.gameinfo`. Pass the filename, including `.wav`. Use signed 16-bit PCM at **48 kHz**, in mono or stereo. Mono uses 96 KB/s of SD bandwidth and works well for short effects and background tracks.

```powershell
ffmpeg -i source.ogg -map_metadata -1 -ar 48000 -ac 1 -c:a pcm_s16le background.wav
```

```cpp
// During setup, after console.begin():
console.sound().preloadEffect("pickup.wav");
console.sound().playMusic("background.wav");

// During gameplay:
console.sound().playEffect("pickup.wav");
```

- `playMusic()` loops one track. Request another file to change tracks, or call `stopMusic()` to stop it. Already queued music drains before a switch or stop is audible.
- `playEffect()` plays an effect once. Up to four effects can overlap. Music plays more quietly while effects are audible, then returns to its usual level. Extra triggers are dropped when all four effect voices are playing. Keep each effect at or below 192,000 PCM bytes (2 seconds mono or 1 second stereo).
- `preloadEffect()` loads synchronously and reports whether the effect is ready. Use it in `setup()` or a loading screen. The cache holds up to eight effects and can evict idle clips. A cache miss during gameplay loads the effect in chunks before playing it.
- `playMusic()` and `playEffect()` report whether the request was accepted. Missing or invalid WAV files are reported over serial during loading. Calls return `false` while the corresponding volume is 0% or output is off or unavailable.

During gameplay, the console services the audio queue between calls to `loop()`, with at most one 4 KiB PCM read each iteration. Four music buffers provide read-ahead across short stalls. Return promptly from `loop()` so the console can service stereo music at least 60 times per second. The Arduino task handles SD access alongside input and display, while the I²S task on the other core mixes and plays from memory.
