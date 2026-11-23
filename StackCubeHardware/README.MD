# StackCube hardware library

Build this library with any of the three PlatformIO projects. The launcher and games share its CoreS3 HAL and USB input drivers.

```powershell
pio run -d StackCubeConsole
```

The HAL configures M126 HDMI at 1280 × 720 with a 640 × 360 drawing surface. It serializes HDMI, SD, and MAX3421E SPI transfers by keeping them on the Arduino loop task. Keep game drawing and `Console::poll()` on that task. End open drawing transactions before polling input.

Game audio uses the CoreS3 speaker or M126 HDMI I²S. HDMI uses GPIO6 for BCLK, GPIO0 for LRCLK, and GPIO13 for data, at a fixed 48 kHz. The built-in amplifier uses its own clocks and shares GPIO13 for data. The HAL selects one route per launch and keeps the amplifier disabled on HDMI. The separate M5Unified playback task mixes buffered PCM and drives I²S DMA without accessing SPI.

The USB module uses USB Host Shield Library 2.0. The pinned library includes wired Xbox 360, Xbox One, and Microsoft Xbox Core Controller (USB PID `0x0B12`) support. StackCube also handles boot-protocol keyboards and mice, and standard joystick or gamepad HID collections. Compatibility depends on the device's USB descriptors and protocol. The host registers two hub drivers and provides four gamepad slots. A powered full-speed-compatible USB 2.0 hub is suitable for multi-device testing. The upstream USB library is GPL-licensed.

For the Xbox One S (`045e:02ea`), StackCube waits for a connected status or an input report before exposing the gamepad slot. When the pad announces a new session on an existing USB connection, StackCube clears its old input state and starts the wired session again.

During hub startup, StackCube resets occupied ports one at a time. This lets the launcher or game enumerate USB devices again when a hub keeps its downstream ports enabled across a CoreS3 reboot.
