# UART Console

Run `pio run -d Games/StackCubeUartConsole` from the repository root to build the CoreS3 module. To install it alongside the other SD apps, follow the [microSD preparation guide](../README.MD). The launcher lists it as **UART Console**.

Connect a USB keyboard and wire the selected DINBase port for 3.3 V UART with a common ground. The console starts on Port B at **115200 baud, 8N1**, with yellow as StackCube TX and white as RX.

### UART settings

Press each key to cycle its setting. The current settings follow you when switching ports. F10 requires the [console firmware with V6 game services](../../StackCubeConsole/README.MD).

| Key | Setting | Values |
| --- | --- | --- |
| F6 | Baud rate | 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600 |
| F7 | Data bits | 5, 6, 7, 8 |
| F8 | Parity | None, even, odd |
| F9 | Stop bits | 1, 1.5, 2 |
| F10 | Swap RX/TX pins | TX yellow / RX white ↔ TX white / RX yellow |

If your target needs the swapped mapping, select **TX White / RX Yellow** before connecting it. If F10 displays **TX busy**, wait for the current transmission to finish and press it again.

### Console controls

| Key | Action |
| --- | --- |
| F1 | Switch between Port B and Port C. A successful switch clears the receive view. |
| F3 | Cycle send format through ASCII, HEX, DEC LE, and DEC BE. |
| F4 | Cycle the display format independently and reformat the retained bytes. |
| F5 | Clear the receive view and reset decimal word alignment. |
| Enter / Backspace | Send the draft or delete its last character. |
| Esc | Return to the launcher. |

| Format | Send with Enter | Receive view |
| --- | --- | --- |
| ASCII | Type the exact characters to send. Enter adds no line ending. | Spaces stay blank and LF starts a new row. Control bytes appear in named frames such as `SOH` and `CR`. Bytes above `7F` appear in hex frames. |
| HEX | Type byte pairs with optional spaces between pairs, such as `48 69 0D 0A`. | Two uppercase hex digits per byte. |
| DEC LE / DEC BE | Type unsigned integers from `0` to `4294967295`, separated by spaces. Each sends four bytes in the selected byte order. | Unsigned 32-bit words starting at the first byte after opening or clearing the port view. Any trailing bytes appear in hex frames until a full word arrives. |

The receive view follows the latest data. The RX and TX dots show their assigned wire colors and grow briefly as bytes arrive or are accepted for transmission.
