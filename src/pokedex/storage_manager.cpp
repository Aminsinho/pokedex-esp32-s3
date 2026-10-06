#include "storage_manager.h"
#include <SD_MMC.h>

// SDMMC pins (Freenove ESP32-S3 Display, documented in HARDWARE_PINOUT.md)
// CLK=38, CMD=40, D0=39, D1=41, D2=48, D3=47
#define SDMMC_CLK  38
#define SDMMC_CMD  40
#define SDMMC_D0   39
#define SDMMC_D1   41
#define SDMMC_D2   48
#define SDMMC_D3   47

StorageManager storageMgr;

bool StorageManager::begin() {
  if (_mounted) return true;

  SD_MMC.setPins(SDMMC_CLK, SDMMC_CMD, SDMMC_D0, SDMMC_D1, SDMMC_D2, SDMMC_D3);

  if (!SD_MMC.begin("/sdcard", false, false, 20000000, 5)) {
    Serial.println("[SD] MOUNT FAIL");
    _mounted = false;
    return false;
  }

  _mounted = true;

  uint64_t total = SD_MMC.totalBytes();
  uint64_t free  = SD_MMC.totalBytes() - SD_MMC.usedBytes();
  Serial.printf("[SD] MOUNT OK\n");
  Serial.printf("[SD] SIZE %llu MB\n", total / (1024*1024));
  Serial.printf("[SD] FREE %llu MB\n", free / (1024*1024));

  // Create directory structure
  const char* dirs[] = {
    "/pokedex",
    "/pokedex/pokemon",
    "/pokedex/pokemon/data",
    "/pokedex/pokemon/sprites",
    "/pokedex/pokemon/sprites/small",
    "/pokedex/pokemon/sprites/large",
    "/pokedex/audio",
    "/pokedex/cache",
    "/pokedex/system"
  };
  for (auto d : dirs) {
    if (!SD_MMC.exists(d)) {
      if (!SD_MMC.mkdir(d)) {
        Serial.printf("[SD] mkdir FAIL: %s\n", d);
      }
    }
  }
  Serial.println("[SD] DIRS OK");

  // SD read/write test
  const char* testPath = "/pokedex/system/sd_test.tmp";
  const char* testData = "POKEDEX_SD_OK";
  size_t testLen = strlen(testData);

  // Write
  File f = SD_MMC.open(testPath, FILE_WRITE);
  if (!f) {
    Serial.println("[SD] R/W FAIL (write open)");
    return true;  // SD is mounted, test failed
  }
  f.write((const uint8_t*)testData, testLen);
  f.close();

  // Read and verify
  f = SD_MMC.open(testPath);
  if (!f) {
    Serial.println("[SD] R/W FAIL (read open)");
    return true;
  }
  char buf[32] = {0};
  int n = f.read((uint8_t*)buf, sizeof(buf)-1);
  f.close();
  if (n != (int)testLen || strncmp(buf, testData, testLen) != 0) {
    Serial.println("[SD] R/W FAIL (verify)");
    SD_MMC.remove(testPath);
    return true;
  }

  // Cleanup
  SD_MMC.remove(testPath);
  Serial.println("[SD] READ/WRITE PASS");
  return true;
}

bool StorageManager::writeFile(const String& path, const uint8_t* data, size_t len) {
  if (!_mounted) return false;
  File f = SD_MMC.open(path, FILE_WRITE);
  if (!f) return false;
  size_t written = f.write(data, len);
  f.close();
  return written == len;
}

bool StorageManager::readFile(const String& path, uint8_t* buf, size_t maxLen, size_t* outLen) {
  if (!_mounted) return false;
  File f = SD_MMC.open(path);
  if (!f) return false;
  int n = f.read(buf, maxLen);
  f.close();
  if (outLen) *outLen = (n > 0) ? (size_t)n : 0;
  return n > 0;
}

bool StorageManager::exists(const String& path) {
  if (!_mounted) return false;
  return SD_MMC.exists(path);
}

bool StorageManager::deleteFile(const String& path) {
  if (!_mounted) return false;
  return SD_MMC.remove(path);
}

bool StorageManager::mkdir(const String& path) {
  if (!_mounted) return false;
  if (SD_MMC.exists(path)) return true;
  return SD_MMC.mkdir(path);
}

uint64_t StorageManager::getFreeBytes() {
  if (!_mounted) return 0;
  return SD_MMC.totalBytes() - SD_MMC.usedBytes();
}

uint64_t StorageManager::getTotalBytes() {
  if (!_mounted) return 0;
  return SD_MMC.totalBytes();
}
