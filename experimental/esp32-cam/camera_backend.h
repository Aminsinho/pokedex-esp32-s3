#pragma once
#include <Arduino.h>

enum ScanStatus {
  SCAN_PROCESSING = 0,
  SCAN_COMPLETED  = 1,
  SCAN_FAILED     = 2,
};

struct ScanResult {
  ScanStatus status;
  int    pokemon_id;
  char   pokemon_name[64];
  float  confidence;
  float  match_score;
  int    time_ms;
  char   error_msg[128];
};

// Create a new scan on the backend. Returns scan_id or "" on failure.
const char* camera_request_scan(const char* host, int port);

// Check scan status. Returns 0=processing, 1=completed, 2=error, -1=network.
int camera_get_scan_result(const char* host, int port, const char* scan_id, ScanResult* out);

// Run full scan: request → capture → upload → poll → result.
// capture_fn: must capture and return JPEG in *out (heap allocated).
// Returns 1=success, -1=failure.
int camera_run_scan(const char* host, int port,
                    bool (*capture_fn)(uint8_t** out, size_t* outLen),
                    ScanResult* result);
