#pragma once
#include <Arduino.h>

class StorageManager {
public:
  bool begin();
  bool writeFile(const String& path, const uint8_t* data, size_t len);
  bool readFile(const String& path, uint8_t* buf, size_t maxLen, size_t* outLen);
  bool exists(const String& path);
  bool deleteFile(const String& path);
  bool mkdir(const String& path);
  uint64_t getFreeBytes();
  uint64_t getTotalBytes();

private:
  bool _mounted = false;
};

extern StorageManager storageMgr;
