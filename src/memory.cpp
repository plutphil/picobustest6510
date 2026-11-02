#include "firmware.h"

#include <hardware/gpio.h>
#include <string.h>

namespace firmware {
namespace {
constexpr uint8_t program[] = {
  0x78,
  0xa9, 0x01,
  0x8d, 0x00, 0x00,
  0xa9, 0x00,
  0x8d, 0x01, 0x00,
  0x20, 0x40, 0x80,
  0xa9, 0x01,
  0x8d, 0x01, 0x00,
  0x20, 0x40, 0x80,
  0x4c, 0x06, 0x80,
  0xea, 0xea, 0xea, 0xea, 0xea, 0xea, 0xea, 0xea, 0xea, 0xea,
  0xea, 0xea, 0xea, 0xea, 0xea, 0xea, 0xea, 0xea, 0xea, 0xea,
  0xea, 0xea, 0xea, 0xea, 0xea, 0xea, 0xea, 0xea, 0xea, 0xea,
  0xea, 0xea, 0xea, 0xea, 0xea, 0xea, 0xea, 0xea, 0xea,
  0xa9, 0x01,
  0x85, 0x02,
  0xa2, 0x01,
  0xa0, 0x01,
  0xea,
  0x88,
  0xd0, 0xfc,
  0xca,
  0xd0, 0xf7,
  0xc6, 0x02,
  0xd0, 0xf1,
  0x60
};
uint8_t pinTestProgram[PIN_TEST_PROGRAM_SIZE] = {};

constexpr uint8_t irqHandler[] = {
    0x78,
    0xad, 0x01, 0xd0,
    0x8d, 0x00, 0x02,
    0xee, 0x01, 0x02,
    0xad, 0x00, 0x02,
    0x8d, 0x01, 0xd0,
    0x40
};
}

uint8_t readMemory(uint16_t address) {
  if (address < RAM_SIZE) {
    return ram[address];
  }
  if (address == UART_STATUS_ADDRESS) {
    return uartCount > 0 ? 1 : 0;
  }
  if (address == UART_DATA_ADDRESS) {
    ++uartDataReads;
    if (uartCount == 0) {
      return 0;
    }
    const uint8_t value = uartQueue[uartHead];
    uartHead = (uartHead + 1) % UART_QUEUE_SIZE;
    --uartCount;
    lastReceivedByte = value;
    ++uartBytesReceived;
    gpio_put(IRQ_PIN, uartCount > 0 ? false : true);
    return value;
  }
  const uint8_t *activeProgram = pinTestMode ? pinTestProgram : program;
  const size_t activeProgramSize = pinTestMode ? sizeof(pinTestProgram)
                                                : sizeof(program);
  if (address >= PROGRAM_START &&
      address < PROGRAM_START + activeProgramSize) {
    ++programReads;
    return activeProgram[address - PROGRAM_START];
  }
  if (address >= IRQ_HANDLER_START &&
      address < IRQ_HANDLER_START + sizeof(irqHandler)) {
    ++irqHandlerReads;
    return irqHandler[address - IRQ_HANDLER_START];
  }
  if (address == RESET_VECTOR_LOW) {
    ++resetVectorReads;
    return static_cast<uint8_t>(activeResetAddress & 0xff);
  }
  if (address == RESET_VECTOR_HIGH) {
    ++resetVectorReads;
    return static_cast<uint8_t>(activeResetAddress >> 8);
  }
  if (address == IRQ_VECTOR_LOW) {
    ++irqVectorReads;
    irqPulseCycles = 0;
    gpio_put(IRQ_PIN, true);
    return static_cast<uint8_t>(IRQ_HANDLER_START & 0xff);
  }
  if (address == IRQ_VECTOR_HIGH) {
    return static_cast<uint8_t>(IRQ_HANDLER_START >> 8);
  }
  ++nopResponses;
  return 0xea;
}

uint8_t peekMemory(uint16_t address) {
  if (address < RAM_SIZE) return ram[address];
  if (address == UART_STATUS_ADDRESS) return uartCount > 0 ? 1 : 0;
  if (address == UART_DATA_ADDRESS) {
    return uartCount > 0 ? uartQueue[uartHead] : 0;
  }
  const uint8_t *activeProgram = pinTestMode ? pinTestProgram : program;
  const size_t activeProgramSize = pinTestMode ? sizeof(pinTestProgram)
                                                : sizeof(program);
  if (address >= PROGRAM_START &&
      address < PROGRAM_START + activeProgramSize) {
    return activeProgram[address - PROGRAM_START];
  }
  if (address >= IRQ_HANDLER_START &&
      address < IRQ_HANDLER_START + sizeof(irqHandler)) {
    return irqHandler[address - IRQ_HANDLER_START];
  }
  if (address == RESET_VECTOR_LOW) {
    return static_cast<uint8_t>(activeResetAddress & 0xff);
  }
  if (address == IRQ_VECTOR_LOW) {
    return static_cast<uint8_t>(IRQ_HANDLER_START & 0xff);
  }
  if (address == RESET_VECTOR_HIGH) {
    return static_cast<uint8_t>(activeResetAddress >> 8);
  }
  if (address == IRQ_VECTOR_HIGH) {
    return static_cast<uint8_t>(IRQ_HANDLER_START >> 8);
  }
  return 0xea;
}

bool enqueueUartByte(uint8_t value) {
  if (uartCount == UART_QUEUE_SIZE) {
    ++uartBytesDropped;
    return false;
  }
  uartQueue[uartTail] = value;
  uartTail = (uartTail + 1) % UART_QUEUE_SIZE;
  ++uartCount;
  irqPulseCycles = IRQ_PULSE_CYCLES;
  gpio_put(IRQ_PIN, false);
  return true;
}

void buildPinTestProgram() {
  size_t offset = 0;
  pinTestProgram[offset++] = 0x78;
  for (uint8_t bit = 0; bit < 16; ++bit) {
    const uint16_t address = PIN_TEST_BASE_ADDRESS;
    const uint16_t toggledAddress = static_cast<uint16_t>(
        PIN_TEST_BASE_ADDRESS ^ (1u << bit));
    pinTestProgram[offset++] = 0xa9;
    pinTestProgram[offset++] = static_cast<uint8_t>(1u << bit);
    pinTestProgram[offset++] = 0x8d;
    pinTestProgram[offset++] = static_cast<uint8_t>(address & 0xff);
    pinTestProgram[offset++] = static_cast<uint8_t>(address >> 8);
    pinTestProgram[offset++] = 0xa9;
    pinTestProgram[offset++] = static_cast<uint8_t>(~(1u << bit));
    pinTestProgram[offset++] = 0x8d;
    pinTestProgram[offset++] = static_cast<uint8_t>(toggledAddress & 0xff);
    pinTestProgram[offset++] = static_cast<uint8_t>(toggledAddress >> 8);
  }
  const uint16_t idleAddress = PROGRAM_START + offset;
  pinTestProgram[offset++] = 0x4c;
  pinTestProgram[offset++] = static_cast<uint8_t>(idleAddress & 0xff);
  pinTestProgram[offset] = static_cast<uint8_t>(idleAddress >> 8);
}

void startTest(bool runPinTest, uint16_t ramAddress, bool reportStart) {
  if (testRunning) {
    Serial.println("Test is already running.");
    return;
  }

  gpio_put(RESET_PIN, false);
  gpio_set_dir_masked(DATA_MASK, 0);
  const bool runRamProgram = !runPinTest && ramAddress < RAM_SIZE;
  activeResetAddress = runRamProgram ? ramAddress : PROGRAM_START;
  if (!runRamProgram) {
    memset(ram, 0, sizeof(ram));
  }
  pinTestMode = runPinTest;
  pinTestComplete = false;
  pinTestWrites = 0;
  pinTestAddressHigh = 0;
  pinTestAddressLow = 0;
  pinTestDataHigh = 0;
  pinTestDataLow = 0;
  pinTestMismatch = false;
  if (pinTestMode) {
    buildPinTestProgram();
  }

  for (uint8_t pulse = 0; pulse < 8; ++pulse) {
    gpio_put(CLK_PIN, false);
    delayMicroseconds(PHASE_US);
    gpio_put(CLK_PIN, true);
    delayMicroseconds(PHASE_US);
  }

  readCycles = 0;
  writeCycles = 0;
  nopResponses = 0;
  testCycles = 0;
  resetVectorReads = 0;
  programReads = 0;
  irqHandlerReads = 0;
  irqVectorReads = 0;
  uartBytesReceived = 0;
  uartDataReads = 0;
  uartBytesEchoed = 0;
  uartBytesDropped = 0;
  lastReceivedByte = 0;
  lastEchoedByte = 0;
  uartHead = 0;
  uartTail = 0;
  uartCount = 0;
  irqPulseCycles = 0;
  gpio_put(IRQ_PIN, true);
  lastAddress = 0;
  lastWriteData = 0;
  gpio_put(CLK_PIN, false);
  gpio_put(RESET_PIN, true);
  testRunning = true;
  if (reportStart) {
    if (runRamProgram) {
      Serial.print("Running RAM program at $");
      Serial.println(activeResetAddress, HEX);
    } else {
      Serial.println("Memory monitor and IRQ test running; use terminal or irq <hex-byte>.");
    }
  }
}

void stopTest() {
  if (!testRunning) {
    Serial.println("Test is already stopped.");
    return;
  }

  gpio_put(RESET_PIN, false);
  gpio_put(CLK_PIN, false);
  gpio_set_dir_masked(DATA_MASK, 0);
  testRunning = false;
  Serial.println("6510 stopped; RESET held low.");
}
}