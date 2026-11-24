#pragma once

#include <stdint.h>

namespace stackcube::hal::din {

uint8_t gpioMode(uint8_t port, uint8_t pin, uint8_t mode);
int32_t gpioRead(uint8_t port, uint8_t pin);
uint8_t gpioWrite(uint8_t port, uint8_t pin, uint8_t high);
uint8_t uartBegin(uint8_t port, uint32_t baud);
int32_t uartRead(uint8_t port, uint8_t* data, uint32_t size);
int32_t uartWrite(uint8_t port, const uint8_t* data, uint32_t size);
uint8_t i2cBegin(uint8_t port, uint32_t clockHz);
uint8_t i2cTransfer(uint8_t port, uint8_t address, const uint8_t* tx, uint32_t txSize,
                    uint8_t* rx, uint32_t rxSize);
void end(uint8_t port);
void endAll();

}
