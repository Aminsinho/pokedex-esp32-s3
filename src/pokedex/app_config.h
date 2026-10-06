#pragma once
/* ============================================================
 * app_config.h
 * Build-time configuration for the Pokédex display firmware.
 *
 * Wi-Fi and backend settings live in wifi_config.h. That file is intentionally
 * ignored by Git so credentials are never published.
 *
 * Backend host: the LAN IP of the machine running
 *   cd backend && python -m uvicorn app.main:app --port 8000
 * ============================================================ */

#if __has_include("wifi_config.h")
#include "wifi_config.h"
#else
#error "Falta wifi_config.h. Ejecuta 'Preparar WiFi.bat' en la raiz del proyecto."
#endif

/* Device registry IDs (must match mock_camera.py / ESP32-CAM later) */
#define DISPLAY_DEVICE_ID "pokedex-display-01"
#define CAMERA_DEVICE_ID  "pokedex-camera-01"

/* Networking */
#define HTTP_TIMEOUT_MS     10000
#define SCAN_POLL_MS        700    /* display polls /scan/{id} at this rate */
#define BACKEND_CHECK_MS    5000   /* status re-check while offline */
#define BACKEND_CHECK_ONLINE_MS 30000 /* status re-check while online */
#define SCAN_MATCH_DELAY_MS 900    /* how long MATCH stays on screen */
