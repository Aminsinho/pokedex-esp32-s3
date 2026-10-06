#include "asset_manager.h"
#include "storage_manager.h"
#include "app_config.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <HTTPClient.h>

AssetManager assetMgr;

String AssetManager::_spritePath(uint16_t id, bool large) {
  char path[64];
  if (large) snprintf(path, sizeof(path), "/pokedex/pokemon/sprites/large/%04d.r565", id);
  else snprintf(path, sizeof(path), "/pokedex/pokemon/sprites/small/%04d.r565", id);
  return String(path);
}

bool AssetManager::isCached(uint16_t id, bool large) {
  return storageMgr.exists(_spritePath(id, large));
}

bool AssetManager::fetchAndCache(uint16_t id, bool large, uint8_t* buf, size_t bufSize, uint16_t* outW, uint16_t* outH) {
  if (WiFi.status() != WL_CONNECTED) return false;

  WiFiClient client;
  client.setTimeout(HTTP_TIMEOUT_MS * 3);  // 9s for asset download

  String url = String("http://") + BACKEND_HOST + ":" + String(BACKEND_PORT)
               + "/api/v1/assets/pokemon/" + String(id) + (large ? "/large" : "/small");

  HTTPClient http;
  if (!http.begin(client, url)) {
    Serial.printf("[ASSET] http.begin fail #%d\n", id);
    return false;
  }

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("[ASSET] HTTP %d for #%d\n", code, id);
    http.end();
    return false;
  }

  int total = http.getSize();
  if (total < 8 || total > (int)bufSize) {
    Serial.printf("[ASSET] bad size: %d (max %d)\n", total, (int)bufSize);
    http.end();
    return false;
  }

  WiFiClient* stream = http.getStreamPtr();
  size_t got = 0;
  uint32_t start = millis();
  while ((int)got < total && millis() - start < HTTP_TIMEOUT_MS * 3) {
    int avail = stream->available();
    if (avail > 0) {
      int chunk = min(avail, (int)(bufSize - got));
      int n = stream->readBytes(buf + got, chunk);
      if (n <= 0) break;
      got += n;
    } else {
      delay(1);
    }
  }
  http.end();

  if (got < 8) {
    Serial.println("[ASSET] fetch too short");
    return false;
  }

  // Parse header: uint32_t width, uint32_t height
  uint32_t w = (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) | ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
  uint32_t h = (uint32_t)buf[4] | ((uint32_t)buf[5] << 8) | ((uint32_t)buf[6] << 16) | ((uint32_t)buf[7] << 24);
  if (outW) *outW = (uint16_t)w;
  if (outH) *outH = (uint16_t)h;

  Serial.printf("[ASSET] fetched #%d %s: %ux%u (%d bytes)\n", id, large ? "L" : "S", w, h, (int)got);

  // Cache to SD
  String sdPath = _spritePath(id, large);
  storageMgr.writeFile(sdPath, buf, got);
  Serial.printf("[ASSET] cached to SD: %s\n", sdPath.c_str());

  return true;
}

bool AssetManager::loadSprite(uint16_t id, bool large, uint8_t* buf, size_t bufSize, uint16_t* outW, uint16_t* outH) {
  // 1. Try SD cache
  String sdPath = _spritePath(id, large);
  if (storageMgr.exists(sdPath)) {
    size_t len = 0;
    if (storageMgr.readFile(sdPath, buf, bufSize, &len) && len >= 8) {
      uint32_t w = (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) | ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
      uint32_t h = (uint32_t)buf[4] | ((uint32_t)buf[5] << 8) | ((uint32_t)buf[6] << 16) | ((uint32_t)buf[7] << 24);
      const uint32_t maxSide = large ? 96 : 48;
      if (!w || !h || w > maxSide || h > maxSide || len != 8 + w * h * 2) {
        Serial.printf("[ASSET] invalid SD sprite #%u\n", id);
        return false;
      }
      if (outW) *outW = (uint16_t)w;
      if (outH) *outH = (uint16_t)h;
      Serial.printf("[ASSET] SD cache hit #%d %s (%ux%u)\n", id, large ? "L" : "S", w, h);
      return true;
    }
  }

  // UI callers must remain SD-first: never block touch on an HTTP timeout.
  return false;
}
