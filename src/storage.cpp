#include "firmware.h"

#include <FlashIAP.h>
#include <hardware/flash.h>
#include <string.h>

namespace firmware {
namespace {
constexpr uint32_t SAVE_MAGIC = 0x50424c4bu;
constexpr uint32_t SAVE_VERSION = 1;

struct SaveHeader {
  uint32_t magic;
  uint32_t version;
  uint32_t address;
  uint32_t length;
  uint32_t checksum;
};

alignas(FLASH_PAGE_SIZE) uint8_t saveBuffer[FLASH_SECTOR_SIZE];
alignas(FLASH_PAGE_SIZE) uint8_t verifyBuffer[FLASH_SECTOR_SIZE];
extern "C" uint8_t __flash_binary_end;

uint32_t checksum(const uint8_t *data, size_t length) {
  uint32_t value = 2166136261u;
  for (size_t index = 0; index < length; ++index) {
    value = (value ^ data[index]) * 16777619u;
  }
  return value;
}

bool openStorage(mbed::FlashIAP &flash, uint32_t &sectorAddress) {
  if (flash.init() != 0) return false;
  const uint32_t flashStart = flash.get_flash_start();
  const uint32_t flashSize = flash.get_flash_size();
  if (flashSize < FLASH_SECTOR_SIZE) {
    flash.deinit();
    return false;
  }
  const uint32_t flashEnd = flashStart + flashSize;
  const uint32_t sectorSize = flash.get_sector_size(flashEnd - 1);
  if (sectorSize != FLASH_SECTOR_SIZE) {
    flash.deinit();
    return false;
  }
  sectorAddress = flashEnd - sectorSize;
  if (reinterpret_cast<uintptr_t>(&__flash_binary_end) > sectorAddress) {
    flash.deinit();
    return false;
  }
  return true;
}

bool validHeader(const SaveHeader &header) {
  return header.magic == SAVE_MAGIC && header.version == SAVE_VERSION &&
         header.address >= 2 && header.address < RAM_SIZE &&
         header.length > 0 && header.length <= RAM_SIZE - header.address;
}
}

bool saveRamProgram(uint16_t address, uint16_t length) {
  if (address < 2 || length == 0 ||
      static_cast<uint32_t>(address) + length > RAM_SIZE) {
    return false;
  }

  mbed::FlashIAP flash;
  uint32_t sectorAddress = 0;
  if (!openStorage(flash, sectorAddress)) return false;

  memset(saveBuffer, 0xff, sizeof(saveBuffer));
  const SaveHeader header = {
      SAVE_MAGIC, SAVE_VERSION, address, length,
      checksum(ram + address, length)};
  memcpy(saveBuffer, &header, sizeof(header));
  memcpy(saveBuffer + sizeof(header), ram + address, length);
  const uint32_t pageSize = flash.get_page_size();
  const uint32_t bytesToProgram =
      (sizeof(header) + length + pageSize - 1) / pageSize * pageSize;

  bool success = bytesToProgram <= FLASH_SECTOR_SIZE &&
                 flash.erase(sectorAddress, FLASH_SECTOR_SIZE) == 0 &&
                 flash.program(saveBuffer, sectorAddress, bytesToProgram) == 0 &&
                 flash.read(verifyBuffer, sectorAddress, bytesToProgram) == 0 &&
                 memcmp(saveBuffer, verifyBuffer, bytesToProgram) == 0;
  flash.deinit();
  return success;
}

bool clearSavedProgram() {
  mbed::FlashIAP flash;
  uint32_t sectorAddress = 0;
  if (!openStorage(flash, sectorAddress)) return false;
  const bool success = flash.erase(sectorAddress, FLASH_SECTOR_SIZE) == 0;
  flash.deinit();
  return success;
}

bool restoreSavedProgram(uint16_t &entryAddress) {
  mbed::FlashIAP flash;
  uint32_t sectorAddress = 0;
  if (!openStorage(flash, sectorAddress)) return false;

  SaveHeader header = {};
  bool success = flash.read(&header, sectorAddress, sizeof(header)) == 0 &&
                 validHeader(header);
  if (success) {
    success = flash.read(verifyBuffer, sectorAddress + sizeof(header),
                         header.length) == 0 &&
              checksum(verifyBuffer, header.length) == header.checksum;
  }
  if (success) {
    memcpy(ram + header.address, verifyBuffer, header.length);
    entryAddress = static_cast<uint16_t>(header.address);
  }
  flash.deinit();
  return success;
}
}