# StackCube hardware library

Build this library with any of the three PlatformIO projects. The launcher and games share its CoreS3 HAL and USB input drivers.

```powershell
pio run -d StackCubeConsole
```

The HAL configures M126 HDMI at 1280 × 720 with a 640 × 360 drawing surface. It serializes HDMI, SD, and MAX3421E SPI transfers by keeping them on the Arduino loop task. Keep game drawing and `Console::poll()` on that task. End open drawing transactions before polling input.

The USB module uses USB Host Shield Library 2.0. The pinned library includes wired Xbox 360, Xbox One, and Microsoft Xbox Core Controller (USB PID `0x0B12`) support. StackCube also handles boot-protocol keyboards and mice, and standard joystick or gamepad HID collections. Compatibility depends on the device's USB descriptors and protocol. The host registers two hub drivers and provides four gamepad slots. A powered full-speed-compatible USB 2.0 hub is suitable for multi-device testing. The upstream USB library is GPL-licensed.
