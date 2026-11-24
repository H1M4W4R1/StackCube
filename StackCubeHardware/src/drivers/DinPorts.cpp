#include "../StackCubeDinPorts.h"

#include <driver/gpio.h>
#include <driver/i2c.h>
#include <driver/uart.h>

namespace stackcube::hal::din {
namespace {
enum class Protocol { Closed, Gpio, Uart, I2c };

// These indices match DinPin and PinMode in the game SDK.
constexpr uint8_t Yellow = 0, White = 1;
constexpr uint8_t Output = 3, OpenDrain = 4, Unconfigured = 0xff;
constexpr gpio_mode_t PinDirections[] = {
    GPIO_MODE_INPUT, GPIO_MODE_INPUT, GPIO_MODE_INPUT, GPIO_MODE_INPUT_OUTPUT, GPIO_MODE_INPUT_OUTPUT_OD,
};
constexpr gpio_pull_mode_t PinPulls[] = {
    GPIO_FLOATING, GPIO_PULLUP_ONLY, GPIO_PULLDOWN_ONLY, GPIO_FLOATING, GPIO_FLOATING,
};
constexpr i2c_port_t ExternalI2c = I2C_NUM_0; // CoreS3 internal I2C is controller 1.
constexpr TickType_t I2cTimeout = pdMS_TO_TICKS(25);

struct Port {
    gpio_num_t pins[2];
    uart_port_t uart;
    Protocol protocol = Protocol::Closed;
    uint8_t modes[2] = {Unconfigured, Unconfigured};
    uint32_t clockHz = 0;
};

Port ports[] = {
    {{GPIO_NUM_9, GPIO_NUM_8}, UART_NUM_1},    // B, yellow then white
    {{GPIO_NUM_17, GPIO_NUM_18}, UART_NUM_2}, // C, yellow then white
};
Port* activeI2c;

Port* getPort(uint8_t id) {
    return id < sizeof(ports) / sizeof(ports[0]) ? &ports[id] : nullptr;
}

bool canUse(const Port* port, Protocol protocol) {
    return port && (port->protocol == Protocol::Closed || port->protocol == protocol);
}

void resetPins(const Port& port) {
    for (const auto pin : port.pins) {
        gpio_reset_pin(pin);
        gpio_set_pull_mode(pin, GPIO_FLOATING);
    }
}

void releaseI2c() {
    if (!activeI2c) return;
    i2c_driver_delete(ExternalI2c);
    resetPins(*activeI2c);
    activeI2c = nullptr;
}

bool routeI2c(Port& port, uint32_t clockHz) {
    if (activeI2c == &port && port.clockHz == clockHz) return true;
    releaseI2c();
    if (i2c_driver_install(ExternalI2c, I2C_MODE_MASTER, 0, 0, 0) != ESP_OK) return false;

    i2c_config_t config{};
    config.mode = I2C_MODE_MASTER;
    config.sda_io_num = port.pins[Yellow];
    config.scl_io_num = port.pins[White];
    config.sda_pullup_en = true;
    config.scl_pullup_en = true;
    config.master.clk_speed = clockHz;
    activeI2c = &port;
    if (i2c_param_config(ExternalI2c, &config) == ESP_OK) return true;
    releaseI2c();
    return false;
}

bool isOutput(uint8_t mode) { return mode == Output || mode == OpenDrain; }
}

uint8_t gpioMode(uint8_t id, uint8_t pin, uint8_t mode) {
    auto* port = getPort(id);
    if (!canUse(port, Protocol::Gpio) || pin > White || mode > OpenDrain) return false;
    const auto gpio = port->pins[pin];
    if (isOutput(mode) && !isOutput(port->modes[pin])) gpio_set_level(gpio, mode == OpenDrain);
    if (gpio_set_pull_mode(gpio, PinPulls[mode]) != ESP_OK ||
        gpio_set_direction(gpio, PinDirections[mode]) != ESP_OK) return false;
    port->modes[pin] = mode;
    port->protocol = Protocol::Gpio;
    return true;
}

int32_t gpioRead(uint8_t id, uint8_t pin) {
    const auto* port = getPort(id);
    if (!port || port->protocol != Protocol::Gpio || pin > White || port->modes[pin] == Unconfigured) return -1;
    return gpio_get_level(port->pins[pin]);
}

uint8_t gpioWrite(uint8_t id, uint8_t pin, uint8_t high) {
    const auto* port = getPort(id);
    if (!port || port->protocol != Protocol::Gpio || pin > White || !isOutput(port->modes[pin])) return false;
    return gpio_set_level(port->pins[pin], high != 0) == ESP_OK;
}

uint8_t uartBegin(uint8_t id, uint32_t baud) {
    auto* port = getPort(id);
    if (!canUse(port, Protocol::Uart) || !baud || baud > UART_BITRATE_MAX) return false;
    const auto uart = port->uart;
    if (port->protocol == Protocol::Uart) return uart_set_baudrate(uart, baud) == ESP_OK;
    if (uart_is_driver_installed(uart) || uart_driver_install(uart, 512, 0, 0, nullptr, 0) != ESP_OK) return false;

    uart_config_t config{};
    config.baud_rate = baud;
    config.data_bits = UART_DATA_8_BITS;
    config.parity = UART_PARITY_DISABLE;
    config.stop_bits = UART_STOP_BITS_1;
    config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    config.source_clk = UART_SCLK_APB;
    if (uart_param_config(uart, &config) != ESP_OK ||
        uart_set_pin(uart, port->pins[Yellow], port->pins[White], UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) != ESP_OK) {
        uart_driver_delete(uart);
        resetPins(*port);
        return false;
    }
    port->protocol = Protocol::Uart;
    return true;
}

int32_t uartRead(uint8_t id, uint8_t* data, uint32_t size) {
    const auto* port = getPort(id);
    if (!port || port->protocol != Protocol::Uart || (size && !data) || size > INT32_MAX) return -1;
    return size ? uart_read_bytes(port->uart, data, size, 0) : 0;
}

int32_t uartWrite(uint8_t id, const uint8_t* data, uint32_t size) {
    const auto* port = getPort(id);
    if (!port || port->protocol != Protocol::Uart || (size && !data) || size > INT32_MAX) return -1;
    return size ? uart_tx_chars(port->uart, reinterpret_cast<const char*>(data), size) : 0;
}

uint8_t i2cBegin(uint8_t id, uint32_t clockHz) {
    auto* port = getPort(id);
    if (!canUse(port, Protocol::I2c) || clockHz < 10000 || clockHz > 400000 || !routeI2c(*port, clockHz)) return false;
    port->clockHz = clockHz;
    port->protocol = Protocol::I2c;
    return true;
}

uint8_t i2cProbe(uint8_t id, uint8_t address) {
    auto* port = getPort(id);
    if (!port || port->protocol != Protocol::I2c || address > 0x7f || !routeI2c(*port, port->clockHz)) return false;

    alignas(void*) uint8_t buffer[I2C_LINK_RECOMMENDED_SIZE(1)];
    const auto command = i2c_cmd_link_create_static(buffer, sizeof(buffer));
    if (!command) return false;
    const bool acknowledged = i2c_master_start(command) == ESP_OK &&
        i2c_master_write_byte(command, (address << 1) | I2C_MASTER_WRITE, true) == ESP_OK &&
        i2c_master_stop(command) == ESP_OK && i2c_master_cmd_begin(ExternalI2c, command, I2cTimeout) == ESP_OK;
    i2c_cmd_link_delete_static(command);
    return acknowledged;
}

uint8_t i2cTransfer(uint8_t id, uint8_t address, const uint8_t* tx, uint32_t txSize,
                    uint8_t* rx, uint32_t rxSize) {
    auto* port = getPort(id);
    if (!port || port->protocol != Protocol::I2c || address > 0x7f || (!txSize && !rxSize) ||
        (txSize && !tx) || (rxSize && !rx) || !routeI2c(*port, port->clockHz)) return false;
    if (!txSize) return i2c_master_read_from_device(ExternalI2c, address, rx, rxSize, I2cTimeout) == ESP_OK;
    if (!rxSize) return i2c_master_write_to_device(ExternalI2c, address, tx, txSize, I2cTimeout) == ESP_OK;
    return i2c_master_write_read_device(ExternalI2c, address, tx, txSize, rx, rxSize, I2cTimeout) == ESP_OK;
}

void end(uint8_t id) {
    auto* port = getPort(id);
    if (!port || port->protocol == Protocol::Closed) return;
    if (port->protocol == Protocol::Uart) uart_driver_delete(port->uart);
    if (activeI2c == port) releaseI2c();
    else resetPins(*port);
    port->protocol = Protocol::Closed;
    port->modes[Yellow] = port->modes[White] = Unconfigured;
    port->clockHz = 0;
}

void endAll() {
    for (uint8_t id = 0; id < sizeof(ports) / sizeof(ports[0]); ++id) end(id);
}

}
