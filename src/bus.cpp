#include "firmware.h"

#include <hardware/gpio.h>

namespace firmware {
void initializeBus() {
  gpio_init_mask(ADDRESS_MASK | DATA_MASK | RW_MASK);
  gpio_set_dir_masked(ADDRESS_MASK | RW_MASK, 0);
  gpio_set_dir_masked(DATA_MASK, 0);

  _gpio_init(CLK_PIN);
  _gpio_init(RDY_PIN);
  _gpio_init(IRQ_PIN);
  _gpio_init(NMI_PIN);
  _gpio_init(RESET_PIN);
  gpio_set_dir(CLK_PIN, GPIO_OUT);
  gpio_set_dir(RDY_PIN, GPIO_OUT);
  gpio_set_dir(IRQ_PIN, GPIO_OUT);
  gpio_set_dir(NMI_PIN, GPIO_OUT);
  gpio_set_dir(RESET_PIN, GPIO_OUT);

  gpio_put(CLK_PIN, false);
  gpio_put(RDY_PIN, true);
  gpio_put(IRQ_PIN, true);
  gpio_put(NMI_PIN, true);
  gpio_put(RESET_PIN, false);
}

void serviceBus() {
  if (!testRunning) {
    return;
  }

  gpio_set_dir_masked(DATA_MASK, 0);
  gpio_put(CLK_PIN, false);
  delayMicroseconds(PHASE_US);

  const uint32_t bus = gpio_get_all();
  lastAddress = static_cast<uint16_t>(bus & ADDRESS_MASK);
  const bool isRead = (bus & RW_MASK) != 0;

  if (isRead) {
    const uint8_t data = readMemory(lastAddress);
    gpio_put_masked(DATA_MASK, static_cast<uint32_t>(data) << 16);
    gpio_set_dir_masked(DATA_MASK, DATA_MASK);
    ++readCycles;
  } else {
    gpio_set_dir_masked(DATA_MASK, 0);
  }

  gpio_put(CLK_PIN, true);
  delayMicroseconds(PHASE_US);

  if (!isRead) {
    const uint32_t writeBus = gpio_get_all();
    lastWriteData = static_cast<uint8_t>((writeBus & DATA_MASK) >> 16);
    if (pinTestMode) {
      pinTestAddressHigh |= lastAddress;
      pinTestAddressLow |= static_cast<uint16_t>(~lastAddress);
      pinTestDataHigh |= lastWriteData;
      pinTestDataLow |= static_cast<uint8_t>(~lastWriteData);
      if (pinTestWrites < PIN_TEST_WRITE_COUNT) {
        const uint8_t bit = static_cast<uint8_t>(pinTestWrites / 2);
        const bool inverse = (pinTestWrites & 1u) != 0;
        const uint16_t expectedAddress = inverse
          ? static_cast<uint16_t>(PIN_TEST_BASE_ADDRESS ^ (1u << bit))
          : PIN_TEST_BASE_ADDRESS;
        const uint8_t expectedData = static_cast<uint8_t>(1u << bit);
        if (lastAddress != expectedAddress ||
            lastWriteData != (inverse ? static_cast<uint8_t>(~expectedData)
                                      : expectedData)) {
          pinTestMismatch = true;
        }
      } else {
        pinTestMismatch = true;
      }
      ++pinTestWrites;
      pinTestComplete = pinTestWrites >= PIN_TEST_WRITE_COUNT;
    }
    if (lastAddress < RAM_SIZE) {
      ram[lastAddress] = lastWriteData;
    } else if (lastAddress == UART_DATA_ADDRESS) {
      lastEchoedByte = lastWriteData;
      ++uartBytesEchoed;
      Serial.write(lastWriteData);
    }
    ++writeCycles;
  }

  ++testCycles;
  if (irqPulseCycles > 0) {
    --irqPulseCycles;
    if (irqPulseCycles == 0) {
      gpio_put(IRQ_PIN, true);
    }
  }
}
}