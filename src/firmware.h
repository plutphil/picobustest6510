#pragma once

#include <Arduino.h>

namespace firmware {
constexpr uint32_t ADDRESS_MASK = 0x0000ffffu;
constexpr uint32_t DATA_MASK = 0x00ff0000u;
constexpr uint32_t RW_MASK = 1u << 24;
constexpr uint8_t CLK_PIN = 25;
constexpr uint8_t RDY_PIN = 26;
constexpr uint8_t IRQ_PIN = 27;
constexpr uint8_t NMI_PIN = 28;
constexpr uint8_t RESET_PIN = 29;
constexpr uint32_t PHASE_US = 2;
constexpr size_t COMMAND_SIZE = 64;
constexpr size_t NOP_LOG_SIZE = 32;
constexpr uint16_t PROGRAM_START = 0x8000;
constexpr uint16_t PIN_TEST_BASE_ADDRESS = 0x5555;
constexpr size_t PIN_TEST_WRITE_COUNT = 32;
constexpr size_t PIN_TEST_PROGRAM_SIZE = 164;
constexpr uint16_t RESET_VECTOR_LOW = 0xfffc;
constexpr uint16_t RESET_VECTOR_HIGH = 0xfffd;
constexpr uint16_t IRQ_VECTOR_LOW = 0xfffe;
constexpr uint16_t IRQ_VECTOR_HIGH = 0xffff;
constexpr uint16_t RAM_SIZE = 0x0800;
constexpr uint16_t UART_STATUS_ADDRESS = 0xd000;
constexpr uint16_t UART_DATA_ADDRESS = 0xd001;
constexpr uint16_t IRQ_HANDLER_START = 0x8100;
constexpr size_t UART_QUEUE_SIZE = 16;
constexpr uint8_t IRQ_PULSE_CYCLES = 64;

extern uint32_t readCycles;
extern uint32_t writeCycles;
extern uint32_t nopResponses;
extern uint32_t testCycles;
extern uint32_t resetVectorReads;
extern uint32_t programReads;
extern uint32_t irqHandlerReads;
extern uint32_t irqVectorReads;
extern uint32_t uartBytesReceived;
extern uint32_t uartDataReads;
extern uint32_t uartBytesEchoed;
extern uint32_t uartBytesDropped;
extern uint16_t lastAddress;
extern uint8_t lastWriteData;
extern uint16_t activeResetAddress;
extern uint8_t lastReceivedByte;
extern uint8_t lastEchoedByte;
extern uint8_t ram[RAM_SIZE];
extern uint16_t pinTestAddressHigh;
extern uint16_t pinTestAddressLow;
extern uint8_t pinTestDataHigh;
extern uint8_t pinTestDataLow;
extern size_t pinTestWrites;
extern bool pinTestMismatch;
extern uint8_t uartQueue[UART_QUEUE_SIZE];
extern size_t uartHead;
extern size_t uartTail;
extern size_t uartCount;
extern uint8_t irqPulseCycles;
extern bool testRunning;
extern bool pinTestMode;
extern bool pinTestComplete;
extern bool terminalMode;
extern char command[COMMAND_SIZE];
extern size_t commandLength;

uint8_t readMemory(uint16_t address);
uint8_t peekMemory(uint16_t address);
bool enqueueUartByte(uint8_t value);
void buildPinTestProgram();
void startTest(bool runPinTest = false, uint16_t ramAddress = 0xffff,
			   bool reportStart = true);
bool saveRamProgram(uint16_t address, uint16_t length);
bool clearSavedProgram();
bool restoreSavedProgram(uint16_t &entryAddress);
void stopTest();
void printHelp();
void printStatus();
void runCommand();
void pollSerial();
void initializeBus();
void serviceBus();
}