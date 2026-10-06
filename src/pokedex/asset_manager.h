#pragma once
#include <Arduino.h>

class AssetManager {
public:
  // Try to load a sprite. Returns true on success.
  // outBuf must be large enough for (w*h*2) pixels + 8 byte header.
  // outW/outH get the dimensions.
  bool loadSprite(uint16_t pokemonId, bool large, uint8_t* outBuf, size_t bufSize, uint16_t* outW, uint16_t* outH);

  // Check if sprite is cached on SD
  bool isCached(uint16_t pokemonId, bool large);

  // Explicitly fetch from backend and cache to SD
  bool fetchAndCache(uint16_t pokemonId, bool large, uint8_t* buf, size_t bufSize, uint16_t* outW, uint16_t* outH);

private:
  String _spritePath(uint16_t id, bool large);
};

extern AssetManager assetMgr;
