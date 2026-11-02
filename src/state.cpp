#include "firmware.h"

namespace firmware {
uint32_t readCycles = 0;
uint32_t writeCycles = 0;
uint32_t nopResponses = 0;
uint32_t testCycles = 0;
uint32_t resetVectorReads = 0;
uint32_t programReads = 0;
uint32_t irqHandlerReads = 0;
uint32_t irqVectorReads = 0;
uint32_t uartBytesReceived = 0;
uint32_t uartDataReads = 0;
uint32_t uartBytesEchoed = 0;
uint32_t uartBytesDropped = 0;
uint16_t lastAddress = 0;
uint8_t lastWriteData = 0;
uint16_t activeResetAddress = PROGRAM_START;
uint8_t lastReceivedByte = 0;
uint8_t lastEchoedByte = 0;
uint8_t ram[RAM_SIZE] = {};
uint16_t pinTestAddressHigh = 0;
uint16_t pinTestAddressLow = 0;
uint8_t pinTestDataHigh = 0;
uint8_t pinTestDataLow = 0;
size_t pinTestWrites = 0;
bool pinTestMismatch = false;
uint8_t uartQueue[UART_QUEUE_SIZE] = {};
size_t uartHead = 0;
size_t uartTail = 0;
size_t uartCount = 0;
uint8_t irqPulseCycles = 0;
bool testRunning = false;
bool pinTestMode = false;
bool pinTestComplete = false;
bool terminalMode = false;
char command[COMMAND_SIZE] = {};
size_t commandLength = 0;
}