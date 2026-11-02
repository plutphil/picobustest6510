#include "firmware.h"

using namespace firmware;

namespace {
bool monitorBannerPrinted = false;
}

void setup() {
  initializeBus();

  Serial.begin(115200);
  uint16_t entryAddress = 0;
  if (restoreSavedProgram(entryAddress)) {
    startTest(false, entryAddress, false);
  }
}

void loop() {
  if (!monitorBannerPrinted && Serial) {
    Serial.println(testRunning ? "6510 autostart program running."
                               : "6510 test ready. CPU held in RESET.");
    printHelp();
    Serial.print("> ");
    monitorBannerPrinted = true;
  }
  pollSerial();
  serviceBus();
}
