#include <StackCube.h>

using namespace stackcube;

namespace {
constexpr uint8_t FirstAddress = 0x08;
constexpr uint8_t LastAddress = 0x77;
constexpr int AddressCount = 128;
constexpr uint32_t ScanPauseMs = 250;
constexpr uint16_t Gray = 0xa534;
constexpr uint16_t DarkGray = 0x2945;
constexpr char OutdatedFirmware[] = "Update firmware";

Console console;
DinPortId selectedPort = DinPortId::B;
bool found[AddressCount];
bool portOpen;
bool probeAvailable = true;
uint8_t nextAddress = FirstAddress;
uint32_t lastScanFinished;

void drawAddress(uint8_t address) {
    static constexpr char Hex[] = "0123456789ABCDEF";
    const char label[] = {Hex[address >> 4], Hex[address & 0x0f], '\0'};
    const int x = 20 + (address & 0x0f) * 38;
    const int y = 66 + (address >> 4) * 31;
    auto& screen = console.screen();
    screen.fillRect(x, y, 30, 25, found[address] ? Green : DarkGray);
    screen.setTextColor(found[address] ? Black : Gray);
    screen.setTextSize(2);
    screen.setTextAlign(TextAlign::Center);
    screen.drawString(label, x + 15, y + 5);
}

void drawPort(DinPortId port, int x, const char* label) {
    auto& screen = console.screen();
    const bool selected = selectedPort == port;
    screen.fillRect(x, 12, 82, 30, selected ? White : DarkGray);
    screen.setTextColor(selected ? Black : Gray);
    screen.setTextSize(2);
    screen.setTextAlign(TextAlign::Center);
    screen.drawString(label, x + 41, 19);
}

void drawStatus(const char* status) {
    auto& screen = console.screen();
    screen.fillRect(20, 323, 228, 21, Black);
    screen.setTextColor(Gray);
    screen.setTextSize(2);
    screen.setTextAlign(TextAlign::Left);
    screen.drawString(status, 20, 326);
}

void selectPort(DinPortId port) {
    if (portOpen) console.port(selectedPort).end();
    selectedPort = port;
    portOpen = probeAvailable && console.port(port).beginI2c();
    nextAddress = FirstAddress;
    lastScanFinished = console.millis();

    auto& screen = console.screen();
    screen.startWrite();
    drawPort(DinPortId::B, 448, "Port B");
    drawPort(DinPortId::C, 538, "Port C");
    for (int address = FirstAddress; address <= LastAddress; ++address) {
        if (!found[address]) continue;
        found[address] = false;
        drawAddress(address);
    }
    const char* status = portOpen ? "100 kHz" : "Port unavailable";
    drawStatus(probeAvailable ? status : OutdatedFirmware);
    screen.endWrite();
}
}

void setup() {
    if (!console.begin()) return;
    auto& screen = console.screen();
    screen.startWrite();
    screen.fillScreen(Black);
    screen.setTextColor(White);
    screen.setTextSize(3);
    screen.drawString("I2C Scanner", 20, 15);
    screen.fillRect(20, 53, 600, 1, DarkGray);
    for (int address = 0; address < AddressCount; ++address) drawAddress(address);

    screen.setTextAlign(TextAlign::Left);
    screen.setTextColor(Gray);
    screen.drawString("Left/Right: Port   Esc/B: Exit", 260, 326);
    screen.endWrite();
    selectPort(DinPortId::B);
}

void loop() {
    const auto pressed = console.poll().pressed;
    if (pressed & (Back | Menu)) {
        console.returnToLauncher();
        return;
    }
    if (pressed & (Left | Right)) {
        const auto port = (pressed & Right) ? DinPortId::C : DinPortId::B;
        if (port != selectedPort) selectPort(port);
    }
    if (!probeAvailable) return;

    const auto now = console.millis();
    if (!portOpen) {
        if (now - lastScanFinished >= ScanPauseMs) selectPort(selectedPort);
        return;
    }
    if (nextAddress > LastAddress) {
        if (now - lastScanFinished < ScanPauseMs) return;
        nextAddress = FirstAddress;
    }

    // One probe per tick keeps input responsive even when the bus times out.
    const auto result = console.port(selectedPort).i2cProbe(nextAddress);
    if (result == I2cProbeResult::Unavailable) {
        console.port(selectedPort).end();
        portOpen = false;
        probeAvailable = false;
        drawStatus(OutdatedFirmware);
        return;
    }
    const bool present = result == I2cProbeResult::Acknowledged;
    if (found[nextAddress] != present) {
        found[nextAddress] = present;
        drawAddress(nextAddress);
    }
    if (++nextAddress > LastAddress) lastScanFinished = console.millis();
}
