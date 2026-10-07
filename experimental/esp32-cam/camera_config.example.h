#pragma once
/* Copia este archivo como camera_config.h. El archivo privado se ignora en Git. */

#define WIFI_SSID "TU_RED_WIFI"
#define WIFI_PASS "TU_CONTRASENA_WIFI"

// IPv4 del PC que ejecuta el backend, en la misma red local.
#define BACKEND_HOST      "192.168.1.100"
#define BACKEND_PORT      8000
#define BACKEND_DEVICE_ID "pokedex-camera-esp32-01"
#define BACKEND_API_BASE  "/api/v1"

#define HEARTBEAT_MS       10000
#define POLL_INTERVAL_MS     500
#define MAX_IMAGE_BYTES    (5*1024*1024)

// Ajustes estables validados para el sensor GC2145 de la placa del proyecto.
#define CAM_LIVE_SIZE        FRAMESIZE_VGA
#define CAM_HQ_SIZE          FRAMESIZE_VGA
#define DEF_JPEG_QUALITY     10
#define DEF_STREAM_QUALITY   14
#define DEF_BEST_OF_N        1

#define CAM_CONTROL_PORT     80
#define CAM_STREAM_PORT      81
#define WEB_TIMEOUT_MS       300000
