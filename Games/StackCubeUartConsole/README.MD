# UART Console

Run `pio run -d Games/StackCubeUartConsole` from the repository root to build the CoreS3 module. To install it alongside the other SD apps, follow the [microSD preparation guide](../README.MD). The launcher lists it as **UART Console**.

Connect a USB keyboard and wire the selected DINBase port for 3.3 V UART with a common ground. Yellow is StackCube TX, and white is StackCube RX. The console starts on Port B at **115200 baud, 8N1**.

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

The receive view follows the latest data. The header dots flash on received bytes (RX) and bytes accepted for transmission (TX).
