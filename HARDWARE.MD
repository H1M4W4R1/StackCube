# StackCube hardware

Stack bottom to top as Base DIN → USB v1.2 (M020-V12) → Display 13.2 (M126) → CoreS3.

Base DIN ports are available with standard M126 firmware. Port B exposes GPIO9 (yellow) and GPIO8 (white). Port C exposes GPIO17 (yellow) and GPIO18 (white). In UART mode, yellow is TX and white is RX. In I²C mode, yellow is SDA and white is SCL. See the [game API port guide](StackCubeAPI/README.MD#din-base-ports) and [M5Stack Base DIN pinout](https://docs.m5stack.com/en/base/Base_DIN).

- Power from Base DIN (9–24 V DC). DIN switch ON, display switch OFF, display DC jack unplugged.
- With DIN OFF and DC unplugged, slide USB SS 2 (`G5`) and INT 1 (`G35`) toward `ON`. Set all other USB DIPs OFF.
- USB CS is GPIO1, USB INT is GPIO10, and display CS is GPIO7. Serialize transfers on shared SPI (MOSI GPIO37, MISO GPIO35, SCK GPIO36).
- Use the CoreS3 microSD slot (CS GPIO4). It shares SPI with the USB and display modules. Use the CoreS3 USB-C port for serial debugging.
- Leave the Gateway H2 out. Its reset uses GPIO7 and its UART output uses GPIO10.
- Leave RCA 13.2 (M125) out. CoreS3 lacks the DAC it needs for composite video, and RCA audio BCK shares GPIO7 with HDMI CS.
