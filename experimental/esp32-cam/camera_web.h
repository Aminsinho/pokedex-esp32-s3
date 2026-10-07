#pragma once
#include "camera_settings.h"

// Web layer: control server (port 80) + MJPEG stream server (port 81).
void camera_web_start();

// Current settings snapshot (thread-safe read).
CameraSettings camera_current_settings();
