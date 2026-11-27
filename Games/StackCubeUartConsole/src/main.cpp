#include <StackCube.h>

#include <cstdio>

#include "ReceiveLog.h"
#include "UartFormat.h"

using namespace stackcube;
using namespace uartconsole;

namespace {
constexpr uint32_t PulseMs = 180;
constexpr uint32_t RedrawMs = 90;
constexpr uint32_t BaudRates[] = {1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600};
constexpr int CharacterWidth = 6;
constexpr int Margin = 16;
constexpr uint16_t Panel = 0x1944;
constexpr uint16_t Line = 0x3aeb;
constexpr uint16_t Muted = 0xa534;
constexpr uint16_t Mint = 0x87b3;
constexpr uint16_t Amber = 0xfda5;

// USB HID keyboard usages supplied by the StackCube input snapshot.
constexpr uint8_t Enter = 0x28, Backspace = 0x2a, KeypadEnter = 0x58;
constexpr uint8_t PortKey = 0x3a, SendModeKey = 0x3c, DisplayModeKey = 0x3d, ClearKey = 0x3e;
constexpr uint8_t BaudKey = 0x3f, DataBitsKey = 0x40, ParityKey = 0x41, StopBitsKey = 0x42;
constexpr uint8_t SwapPinsKey = 0x43; // F10

struct Activity {
    uint32_t last;
    bool active;
    bool drawn;

    void flash(uint32_t now) {
        last = now;
        active = true;
    }

    bool update(uint32_t now) {
        active = active && now - last < PulseMs;
        if (active == drawn) return false;
        drawn = active;
        return true;
    }
};

Console console;
ReceiveLog receiveLog;
DinPortId portId = DinPortId::B;
UartSettings uartSettings;
Format sendFormat = Format::Ascii;
Format displayFormat = Format::Ascii;
Activity rx{}, tx{};
char draft[DraftCapacity + 1];
uint8_t pending[SendCapacity];
size_t draftLength, sendOffset, sendLength;
uint32_t lastLogDraw;
const char* message;
bool portOpen, txDraining, settingsDirty, editorDirty, logDirty, exiting;

void label(const char* text, int x, int y, uint16_t color, uint8_t scale = 1) {
    auto& screen = console.screen();
    screen.setTextColor(color);
    screen.setTextSize(scale);
    screen.drawString(text, x, y);
}

void setMessage(const char* text) {
    if (message == text) return;
    message = text;
    editorDirty = true;
}

void refreshLog(uint32_t now) {
    lastLogDraw = now - RedrawMs;
    logDirty = true;
}

void selectPort(DinPortId requested, uint32_t now) {
    if (!console.port(requested).beginUart(uartSettings)) {
        if (!portOpen) portId = requested;
        settingsDirty = true;
        setMessage(requested == DinPortId::B ? "Port B unavailable" : "Port C unavailable");
        return;
    }
    if (portOpen) console.port(portId).end();
    portId = requested;
    portOpen = true;
    rx.active = tx.active = false;
    settingsDirty = true;
    receiveLog.clear();
    refreshLog(now);
    setMessage(nullptr);
}

bool txBusy(uint32_t now) {
    if (sendLength) return true;
    if (!txDraining) return false;
    // Older firmware cannot report TX idle. Allow 12 bits per byte for the FIFO and shift register.
    constexpr uint32_t QueuedBitMs = 129 * 12 * 1000;
    const uint32_t drainMs = (QueuedBitMs + uartSettings.baud - 1) / uartSettings.baud + 1;
    txDraining = !console.port(portId).uartTxIdle() && now - tx.last < drainMs;
    return txDraining;
}

uint32_t nextBaud(uint32_t baud) {
    for (const uint32_t rate : BaudRates) {
        if (rate > baud) return rate;
    }
    return BaudRates[0];
}

void changeUartSetting(uint8_t key, uint32_t now) {
    if (txBusy(now)) {
        setMessage("TX busy");
        return;
    }

    auto next = uartSettings;
    switch (key) {
        case BaudKey:
            next.baud = nextBaud(next.baud);
            break;
        case DataBitsKey:
            next.dataBits = next.dataBits == 8 ? 5 : next.dataBits + 1;
            break;
        case ParityKey:
            next.parity = next.parity == UartParity::Odd ? UartParity::None
                : static_cast<UartParity>(static_cast<uint8_t>(next.parity) + 1);
            break;
        case StopBitsKey:
            next.stopBits = next.stopBits == UartStopBits::Two ? UartStopBits::One
                : static_cast<UartStopBits>(static_cast<uint8_t>(next.stopBits) + 1);
            break;
        case SwapPinsKey:
            next.swapPins = !next.swapPins;
            break;
    }
    if (!console.port(portId).beginUart(next)) {
        setMessage(key == SwapPinsKey ? "RX/TX swap unavailable" : "UART setting unavailable");
        return;
    }
    uartSettings = next;
    portOpen = true;
    settingsDirty = true;
    setMessage(nullptr);
}

void queueDraft() {
    if (!draftLength) return;
    if (!portOpen) {
        setMessage("Select an available port");
        return;
    }
    if (sendLength) {
        setMessage("TX busy");
        return;
    }
    sendLength = encode(draft, draftLength, sendFormat, pending);
    if (!sendLength) {
        setMessage(sendFormat == Format::Hex ? "Enter pairs of hex digits" : "Enter unsigned 32-bit numbers");
        return;
    }
    draftLength = 0;
    draft[0] = '\0';
    editorDirty = true;
    setMessage(nullptr);
}

char keyboardCharacter(const Input& input) {
    constexpr uint8_t KeypadDivide = 0x54, KeypadDecimal = 0x63;
    if (input.text) return input.text;
    if (input.key >= KeypadDivide && input.key <= KeypadDecimal) {
        return "/*-+\0" "1234567890."[input.key - KeypadDivide];
    }
    return 0;
}

void handleKey(const Input& input, uint32_t now) {
    if (input.key >= BaudKey && input.key <= SwapPinsKey) {
        changeUartSetting(input.key, now);
        return;
    }
    switch (input.key) {
        case PortKey:
            if (txBusy(now)) setMessage("TX busy");
            else selectPort(portId == DinPortId::B ? DinPortId::C : DinPortId::B, now);
            return;
        case SendModeKey:
            sendFormat = nextFormat(sendFormat);
            editorDirty = true;
            setMessage(nullptr);
            return;
        case DisplayModeKey:
            displayFormat = nextFormat(displayFormat);
            settingsDirty = true;
            refreshLog(now);
            return;
        case ClearKey:
            receiveLog.clear();
            refreshLog(now);
            return;
        case Enter:
        case KeypadEnter:
            queueDraft();
            return;
        case Backspace:
            if (draftLength) draft[--draftLength] = '\0';
            break;
        default: {
            const char character = keyboardCharacter(input);
            if (character < ' ' || character > '~') return;
            if (draftLength == DraftCapacity) {
                setMessage("Input full");
                return;
            }
            draft[draftLength++] = character;
            draft[draftLength] = '\0';
            break;
        }
    }
    editorDirty = true;
    setMessage(nullptr);
}

void serviceUart(uint32_t now) {
    if (!portOpen) return;
    const auto port = console.port(portId);
    uint8_t received[256];
    const int32_t count = port.uartRead(received, sizeof(received));
    if (count > 0) {
        receiveLog.append(received, count);
        rx.flash(now);
        logDirty = true;
    } else if (count < 0) {
        setMessage("UART read failed");
    }

    if (!sendLength) return;
    const int32_t written = port.uartWrite(pending + sendOffset, sendLength - sendOffset);
    if (written < 0) {
        setMessage("UART write failed");
        sendLength = sendOffset = 0;
    } else if (written > 0) {
        tx.flash(console.millis());
        txDraining = true;
        sendOffset += written;
        if (sendOffset == sendLength) sendOffset = sendLength = 0;
    }
}

void drawSettings() {
    auto& screen = console.screen();
    screen.fillRect(0, 43, screen.width(), 24, ReceiveLog::Background);
    char settings[40];
    constexpr char ParityNames[] = "NEO";
    constexpr char StopBitNames[][4] = {"1", "1.5", "2"};
    std::snprintf(settings, sizeof(settings), "Port %c  /  %lu %u%c%s%s",
                  portId == DinPortId::B ? 'B' : 'C', static_cast<unsigned long>(uartSettings.baud),
                  static_cast<unsigned>(uartSettings.dataBits),
                  ParityNames[static_cast<uint8_t>(uartSettings.parity)],
                  StopBitNames[static_cast<uint8_t>(uartSettings.stopBits)], portOpen ? "" : "  /  CLOSED");
    label(settings, Margin, 52, portOpen ? Muted : Amber);
    const int width = std::snprintf(settings, sizeof(settings), "Display  %s", formatName(displayFormat)) * CharacterWidth;
    label(settings, screen.width() - Margin - width, 52, Mint);
}

void drawEditor() {
    auto& screen = console.screen();
    screen.fillRect(0, 281, screen.width(), 48, Panel);
    char title[16];
    std::snprintf(title, sizeof(title), "Send  %s", formatName(sendFormat));
    label(title, 16, 290, Mint);
    if (message) label(message, 172, 290, Amber);
    label(draft, 16, 309, White);
    screen.fillRect(Margin + CharacterWidth * draftLength, 317, 5, 1, Mint);
}

void drawWireDot(int x, uint16_t color, bool active) {
    auto& screen = console.screen();
    screen.fillCircle(x, 21, 6, active ? color : Panel);
    if (!active) screen.fillCircle(x, 21, 4, color);
}

void draw(uint32_t now) {
    const bool rxChanged = rx.update(now);
    const bool txChanged = tx.update(now);
    const bool drawLog = logDirty && now - lastLogDraw >= RedrawMs;
    if (!settingsDirty && !editorDirty && !rxChanged && !txChanged && !drawLog) return;

    auto& screen = console.screen();
    screen.startWrite();
    if (settingsDirty) drawSettings();
    const uint16_t rxWire = uartSettings.swapPins ? Yellow : White;
    const uint16_t txWire = uartSettings.swapPins ? White : Yellow;
    if (settingsDirty || rxChanged) drawWireDot(548, portOpen ? rxWire : Line, rx.active);
    if (settingsDirty || txChanged) drawWireDot(612, portOpen ? txWire : Line, tx.active);
    if (drawLog) {
        receiveLog.draw(screen, displayFormat);
        lastLogDraw = now;
        logDirty = false;
    }
    if (editorDirty) drawEditor();
    screen.endWrite();
    settingsDirty = editorDirty = false;
}
}

void setup() {
    if (!console.begin()) return;
    const uint32_t now = console.millis();
    selectPort(DinPortId::B, now);
    auto& screen = console.screen();
    screen.startWrite();
    screen.fillScreen(ReceiveLog::Background);
    screen.fillRect(0, 0, screen.width(), 42, Panel);
    constexpr int Separators[] = {42, 67, ReceiveLog::Bottom, 329};
    for (const int y : Separators) screen.fillRect(0, y, screen.width(), 1, Line);
    label("UART Console", 16, 14, White, 2);
    label("RX", 524, 17, Muted);
    label("TX", 588, 17, Muted);
    label("F1 Port   F3 Send mode   F4 Display mode   F5 Clear RX   Enter Send   Esc Exit", Margin, 334, Muted);
    label("F6 Baud   F7 Data bits   F8 Parity         F9 Stop bits  F10 Swap RX/TX", Margin, 347, Muted);
    screen.endWrite();
    editorDirty = true;
    draw(now);
}

void loop() {
    const uint32_t now = console.millis();
    const auto& input = console.poll();
    if (input.pressed & (Back | Menu)) exiting = true;
    if (exiting && !txBusy(now)) {
        console.returnToLauncher();
        return;
    }
    if (!exiting) handleKey(input, now);
    serviceUart(now);
    draw(console.millis());
}
