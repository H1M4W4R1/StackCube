#pragma once

#include "StackCubeInput.h"
#include <M5GFX.h>
#include <SPI.h>

namespace stackcube::hal {

constexpr int UsbCs = 1;
constexpr int UsbInt = 10;
constexpr int SdCs = 4;
constexpr int HdmiCs = 7;
constexpr int LcdCs = 3;
constexpr int Sck = 36;
constexpr int Miso = 35;
constexpr int Mosi = 37;
constexpr int Width = 640;
constexpr int Height = 360;

bool begin();
void beginBoard();
bool beginPeripherals();
void prepareSdBus();
bool readSdPresence(bool& inserted);
bool restartSdPower();
const Input& poll();
bool setRumble(uint8_t slot, uint8_t left, uint8_t right);
void stopRumble();
M5GFX& display();

}
