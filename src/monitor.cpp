#include "firmware.h"

#include <hardware/gpio.h>
#include <stdlib.h>
#include <string.h>

namespace firmware {
namespace {
bool parseHex(const char *text, unsigned long maxValue, unsigned long &value) {
  if (text == nullptr || *text == '\0') return false;
  char *end = nullptr;
  value = strtoul(text, &end, 16);
  return end != text && *end == '\0' && value <= maxValue;
}

bool parseHexBytes(const char *text, uint8_t *bytes, size_t maxBytes,
                   size_t &byteCount) {
  const size_t length = strlen(text);
  if (length == 0 || (length & 1u) != 0 || length / 2 > maxBytes) {
    return false;
  }
  byteCount = length / 2;
  for (size_t index = 0; index < byteCount; ++index) {
    char high = text[index * 2];
    char low = text[index * 2 + 1];
    const auto nibble = [](char digit) -> int {
      if (digit >= '0' && digit <= '9') return digit - '0';
      if (digit >= 'a' && digit <= 'f') return digit - 'a' + 10;
      return -1;
    };
    const int highValue = nibble(high);
    const int lowValue = nibble(low);
    if (highValue < 0 || lowValue < 0) return false;
    bytes[index] = static_cast<uint8_t>((highValue << 4) | lowValue);
  }
  return true;
}
}

void printHelp() {
  Serial.println("Commands: start, run <addr>, load <addr> <hex-bytes>, save <addr> <count>, clearsave, pintest, stop, terminal, status, peek <addr>, poke <addr> <byte>, dump <addr> [count], irq <hex-byte>");
  Serial.println("UART: $D000 status, $D001 data. IRQ is active-low.");
  Serial.println("RAM programs: stop, load hex bytes at $0002-$07FF, then run <addr>.");
  Serial.println("save stores RAM and autostarts it after reset; clearsave disables autostart.");
  Serial.println("Terminal mode: Ctrl-X returns to the monitor prompt.");
}

void printStatus() {
  Serial.print("Test: ");
  Serial.println(testRunning ? "running" : "stopped (RESET low)");
  Serial.print("Read cycles: ");
  Serial.println(readCycles);
  Serial.print("Write cycles: ");
  Serial.println(writeCycles);
  Serial.print("NOP bytes supplied: ");
  Serial.println(nopResponses);
  Serial.print("Clock cycles: ");
  Serial.println(testCycles);
  Serial.print("Reset vector reads: ");
  Serial.println(resetVectorReads);
  Serial.print("Program reads: ");
  Serial.println(programReads);
  Serial.print("IRQ handler reads: ");
  Serial.println(irqHandlerReads);
  Serial.print("IRQ vector reads: ");
  Serial.println(irqVectorReads);
  Serial.print("UART data reads: ");
  Serial.println(uartDataReads);
  Serial.print("UART queue length: ");
  Serial.println(uartCount);
  Serial.print("UART bytes dropped: ");
  Serial.println(uartBytesDropped);
  Serial.print("Terminal mode: ");
  Serial.println(terminalMode ? "active" : "off");
  Serial.print("IRQ output pin: ");
  Serial.println(gpio_get(IRQ_PIN) ? "HIGH" : "LOW");
  Serial.print("UART bytes received: ");
  Serial.println(uartBytesReceived);
  Serial.print("Last received byte: $");
  Serial.println(lastReceivedByte, HEX);
  Serial.print("UART bytes echoed: ");
  Serial.println(uartBytesEchoed);
  Serial.print("Last echoed byte: $");
  Serial.println(lastEchoedByte, HEX);
  Serial.print("UART echo test: ");
  Serial.println(uartBytesReceived > 0 &&
                         uartBytesEchoed == uartBytesReceived &&
                         lastEchoedByte == lastReceivedByte
                     ? "PASS"
                     : "not yet passed");
  Serial.print("RAM $0200: $");
  Serial.println(ram[0x0200], HEX);
  Serial.print("RAM $0201: $");
  Serial.println(ram[0x0201], HEX);
  Serial.print("IRQ test: ");
  Serial.println(uartBytesReceived > 0 && uartCount == 0 &&
                         irqVectorReads == uartBytesReceived &&
                         ram[0x0200] == lastReceivedByte &&
                         ram[0x0201] == static_cast<uint8_t>(uartBytesReceived)
                     ? "PASS"
                     : "not yet passed");
  Serial.print("Last address: $");
  Serial.println(lastAddress, HEX);
  Serial.print("Last write data: $");
  Serial.println(lastWriteData, HEX);
  Serial.print("Pin test: ");
  if (!pinTestMode) {
    Serial.println("not run");
  } else if (!pinTestComplete) {
    Serial.println(testRunning ? "running" : "incomplete");
  } else {
    const bool passed = pinTestAddressHigh == 0xffff &&
                        pinTestAddressLow == 0xffff &&
                        pinTestDataHigh == 0xff && pinTestDataLow == 0xff &&
                        !pinTestMismatch;
    Serial.println(passed ? "PASS" : "FAIL");
  }
  if (pinTestMode) {
    Serial.print("Pin test writes: ");
    Serial.println(pinTestWrites);
    Serial.print("Address high mask: $");
    Serial.println(pinTestAddressHigh, HEX);
    Serial.print("Address low mask: $");
    Serial.println(pinTestAddressLow, HEX);
    Serial.print("Data high mask: $");
    Serial.println(pinTestDataHigh, HEX);
    Serial.print("Data low mask: $");
    Serial.println(pinTestDataLow, HEX);
    Serial.print("Write sequence mismatch: ");
    Serial.println(pinTestMismatch ? "yes" : "no");
  }
}

void pollSerial() {
  while (Serial.available() > 0) {
    const int received = Serial.read();
    if (terminalMode) {
      const uint8_t input = static_cast<uint8_t>(received);
      if (input == 0x18) {
        terminalMode = false;
        Serial.println();
        Serial.println("UART terminal closed.");
        Serial.print("> ");
      } else {
        enqueueUartByte(input);
      }
      continue;
    }

    const char input = static_cast<char>(received);
    if (input == '\r' || input == '\n') {
      if (commandLength > 0) {
        runCommand();
      }
    } else if (commandLength < COMMAND_SIZE - 1) {
      command[commandLength++] = input;
    } else {
      commandLength = 0;
      Serial.println("Command too long.");
      Serial.print("> ");
    }
  }
}

void runCommand() {
  command[commandLength] = '\0';
  for (size_t index = 0; index < commandLength; ++index) {
    if (command[index] >= 'A' && command[index] <= 'Z') {
      command[index] = static_cast<char>(command[index] - 'A' + 'a');
    }
  }

  char *save = nullptr;
  char *verb = strtok_r(command, " \t", &save);
  char *arg1 = strtok_r(nullptr, " \t", &save);
  char *arg2 = strtok_r(nullptr, " \t", &save);
  char *extra = strtok_r(nullptr, " \t", &save);
  unsigned long address = 0;
  unsigned long value = 0;

  if (verb == nullptr) {
  } else if (strcmp(verb, "help") == 0) {
    printHelp();
  } else if (strcmp(verb, "start") == 0) {
    startTest();
  } else if (strcmp(verb, "run") == 0 && arg1 != nullptr && arg2 == nullptr &&
             parseHex(arg1, RAM_SIZE - 1, address) && address >= 2) {
    startTest(false, static_cast<uint16_t>(address));
  } else if (strcmp(verb, "load") == 0 && arg1 != nullptr && arg2 != nullptr &&
             extra == nullptr && parseHex(arg1, RAM_SIZE - 1, address) &&
             address >= 2 && !testRunning) {
    uint8_t bytes[COMMAND_SIZE / 2] = {};
    size_t byteCount = 0;
    if (parseHexBytes(arg2, bytes, sizeof(bytes), byteCount) &&
        address + byteCount <= RAM_SIZE) {
      memcpy(ram + address, bytes, byteCount);
      Serial.print("Loaded ");
      Serial.print(byteCount);
      Serial.print(" bytes at $");
      Serial.println(address, HEX);
    } else {
      Serial.println("Invalid hex bytes or RAM range.");
    }
  } else if (strcmp(verb, "save") == 0 && arg1 != nullptr && arg2 != nullptr &&
             extra == nullptr && !testRunning &&
             parseHex(arg1, RAM_SIZE - 1, address) && address >= 2 &&
             parseHex(arg2, RAM_SIZE, value) && value > 0 &&
             address + value <= RAM_SIZE) {
    if (saveRamProgram(static_cast<uint16_t>(address),
                       static_cast<uint16_t>(value))) {
      Serial.println("RAM image saved; it will autostart after reset.");
    } else {
      Serial.println("Save failed: flash space, range, or flash write error.");
    }
  } else if (strcmp(verb, "clearsave") == 0 && arg1 == nullptr) {
    if (!testRunning && clearSavedProgram()) {
      Serial.println("Saved image erased; autostart disabled.");
    } else {
      Serial.println("Clear failed; stop the CPU first or check flash space.");
    }
  } else if (strcmp(verb, "terminal") == 0 && arg1 == nullptr) {
    if (!testRunning) {
      startTest();
    }
    terminalMode = true;
    Serial.println("UART terminal active. Ctrl-X returns to the monitor.");
  } else if (strcmp(verb, "pintest") == 0 && arg1 == nullptr) {
    startTest(true);
  } else if (strcmp(verb, "stop") == 0) {
    stopTest();
  } else if (strcmp(verb, "status") == 0) {
    printStatus();
  } else if (strcmp(verb, "peek") == 0 && arg1 != nullptr && arg2 == nullptr &&
             parseHex(arg1, 0xffff, address)) {
    Serial.print("$");
    Serial.print(address, HEX);
    Serial.print(" = $");
    Serial.println(peekMemory(static_cast<uint16_t>(address)), HEX);
  } else if (strcmp(verb, "poke") == 0 && arg1 != nullptr && arg2 != nullptr &&
             extra == nullptr && parseHex(arg1, 0xffff, address) &&
             parseHex(arg2, 0xff, value)) {
    const uint16_t target = static_cast<uint16_t>(address);
    if (target >= 2 && target < RAM_SIZE) {
      ram[target] = static_cast<uint8_t>(value);
      Serial.println("OK");
    } else {
      Serial.println("Poke allowed only in RAM $0002-$07FF.");
    }
  } else if (strcmp(verb, "dump") == 0 && arg1 != nullptr && extra == nullptr &&
             parseHex(arg1, 0xffff, address) &&
             (arg2 == nullptr || parseHex(arg2, 0x100, value))) {
    const size_t count = arg2 == nullptr ? 16 : value;
    for (size_t offset = 0; offset < count; offset += 16) {
      const uint32_t rowAddress = address + offset;
      Serial.print("$");
      Serial.print(rowAddress, HEX);
      Serial.print(": ");
      const size_t rowCount = count - offset < 16 ? count - offset : 16;
      for (size_t column = 0; column < rowCount; ++column) {
        const uint32_t current = rowAddress + column;
        if (current > 0xffff) break;
        const uint8_t byte = peekMemory(static_cast<uint16_t>(current));
        if (byte < 0x10) Serial.print("0");
        Serial.print(byte, HEX);
        Serial.print(" ");
      }
      Serial.println();
    }
  } else if (strcmp(verb, "irq") == 0 && arg1 != nullptr && arg2 == nullptr &&
             parseHex(arg1, 0xff, value)) {
    if (enqueueUartByte(static_cast<uint8_t>(value))) {
      Serial.print("Queued IRQ byte: $");
      Serial.println(value, HEX);
    } else {
      Serial.println("UART queue full.");
    }
  } else {
    Serial.println("Invalid command or arguments. Send 'help'.");
  }

  commandLength = 0;
  if (!terminalMode) {
    Serial.print("> ");
  }
}
}