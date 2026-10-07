#ifndef CAMERA_H
#define CAMERA_H

#include <Arduino.h>
#include "esp_camera.h"
#include "camera_settings.h"

// Initialize the camera (YUV422, VGA). Returns true on success.
bool camera_init();

// Human-readable sensor name from PID.
const char* camera_sensor_name();

// Memory info (KB)
uint32_t camera_psram_total();
uint32_t camera_psram_free();
uint32_t camera_heap_free();

// ---- Concurrency ----
// Exclusive sensor access. Returns true if lock acquired.
bool camera_lock(uint32_t timeout_ms);
void  camera_unlock();
bool  camera_hq_busy();   // true while an HQ capture is running

// Set the sensor to a given framesize (YUV422). Call while holding the lock.
void  camera_set_live_size(framesize_t fs);

// Discard N frames (sensor stabilization after a resolution change).
void  camera_discard(int n);

// Grab one frame at the current sensor size and convert to JPEG (heap, caller frees).
// Call while holding the lock.
bool  camera_grab_jpeg(int quality, uint8_t** out, size_t* outLen);

// High-quality capture with best-of-N. Handles lock + size switch + restore internally.
// *out is heap-allocated on success (caller frees). Returns false on total failure.
bool  camera_capture_hq(const CameraSettings& s, uint8_t** out, size_t* outLen);

// Sharpness metric (Laplacian variance on downsampled Y plane). Higher = sharper.
float camera_sharpness(const uint8_t* yuv422, uint16_t w, uint16_t h);

// Upload JPEG bytes to the backend scan (multipart: device_id + image file).
// POST /api/v1/scan/{scan_id}/image. Returns HTTP status code (0 = network error).
int   camera_upload(const char* host, int port, const char* scan_id,
                    const uint8_t* data, size_t len);

// Report a capture failure for a scan. POST /api/v1/scan/{scan_id}/camera-error.
int   camera_report_error(const char* host, int port, const char* scan_id);

// Heartbeat: POST /api/v1/devices/status with JSON {device_id, device_type, status}.
int   camera_heartbeat(const char* host, int port, const char* device_id, const char* status);

#endif // CAMERA_H
