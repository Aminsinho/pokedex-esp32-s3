/* SD Diagnostic — tests SD card initialization with various parameters */
#include <SD_MMC.h>
#include <Arduino.h>

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("=== SD DIAGNOSTIC ===");

    // Pins
    const int CLK = 38, CMD = 40, D0 = 39, D1 = 41, D2 = 48, D3 = 47;
    Serial.printf("Pins: CLK=%d CMD=%d D0=%d D1=%d D2=%d D3=%d\n", CLK, CMD, D0, D1, D2, D3);

    // Test 1: default (1-bit, 400kHz)
    Serial.print("Test1 (1-bit 400kHz): ");
    SD_MMC.setPins(CLK, CMD, D0, D1, D2, D3);
    if (SD_MMC.begin()) {
        Serial.print("OK ");
        Serial.print(SD_MMC.totalBytes() / 1024 / 1024);
        Serial.println(" MB");
        SD_MMC.end();
    } else {
        Serial.println("FAIL");
    }

    delay(500);

    // Test 2: 4-bit, 20MHz
    Serial.print("Test2 (4-bit 20MHz): ");
    if (SD_MMC.begin("/sdcard", false, false, 20000000, 4)) {
        Serial.print("OK ");
        Serial.print(SD_MMC.totalBytes() / 1024 / 1024);
        Serial.println(" MB");
        SD_MMC.end();
    } else {
        Serial.println("FAIL");
    }

    delay(500);

    // Test 3: 1-bit, 20MHz
    Serial.print("Test3 (1-bit 20MHz): ");
    if (SD_MMC.begin("/sdcard", false, false, 20000000, 1)) {
        Serial.print("OK ");
        Serial.print(SD_MMC.totalBytes() / 1024 / 1024);
        Serial.println(" MB");
        SD_MMC.end();
    } else {
        Serial.println("FAIL");
    }

    delay(500);

    // Test 4: 4-bit, 400kHz
    Serial.print("Test4 (4-bit 400kHz): ");
    if (SD_MMC.begin("/sdcard", false, false, 400000, 4)) {
        Serial.print("OK ");
        Serial.print(SD_MMC.totalBytes() / 1024 / 1024);
        Serial.println(" MB");
        SD_MMC.end();
    } else {
        Serial.println("FAIL");
    }

    delay(500);

    // Test 5: 8-bit, 20MHz
    Serial.print("Test5 (8-bit 20MHz): ");
    if (SD_MMC.begin("/sdcard", false, false, 20000000, 8)) {
        Serial.print("OK ");
        Serial.print(SD_MMC.totalBytes() / 1024 / 1024);
        Serial.println(" MB");
        SD_MMC.end();
    } else {
        Serial.println("FAIL");
    }

    Serial.println("=== DONE ===");
}

void loop() {
    delay(1000);
}
