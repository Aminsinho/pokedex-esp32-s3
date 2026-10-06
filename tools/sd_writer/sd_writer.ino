/* SD Writer v3 — simple push protocol.
 * PC sends: "BEGIN <path> <size>\r\n" then raw bytes.
 * ESP32 reads exactly <size> bytes and writes to SD.
 */
#include <SD_MMC.h>
#include <Arduino.h>

void setup() {
    Serial.begin(115200);
    delay(500);
    SD_MMC.setPins(38, 40, 39, 41, 48, 47);
    if (!SD_MMC.begin("/sdcard", false, false, 20000000, 4)) {
        Serial.println("SD_MOUNT_FAIL");
        return;
    }
    Serial.print("SD_MOUNT_OK ");
    Serial.print(SD_MMC.totalBytes() / 1024 / 1024);
    Serial.println(" MB");
}

static void drainSerial() {
    while (Serial.available()) Serial.read();
}

void loop() {
    if (!Serial.available()) return;

    String line = Serial.readStringUntil('\n');
    line.trim();

    if (line.startsWith("BEGIN ")) {
        int space1 = line.indexOf(' ', 6);
        String path = line.substring(6, space1);
        uint32_t size = (uint32_t)line.substring(space1 + 1).toInt();

        // Ensure parent dir
        int lastSlash = path.lastIndexOf('/');
        if (lastSlash > 0) {
            SD_MMC.mkdir(path.substring(0, lastSlash));
        }

        File f = SD_MMC.open(path, FILE_WRITE);
        if (!f) {
            Serial.println("ERR_OPEN");
            return;
        }

        Serial.println("READY");
        Serial.flush();

        // Read exactly <size> bytes
        uint32_t received = 0;
        uint32_t deadline = millis() + 120000;  // 2 min timeout
        uint8_t buf[512];

        while (received < size && millis() < deadline) {
            if (Serial.available() > 0) {
                int avail = Serial.available();
                int toRead = (int)(size - received);
                if (avail < toRead) toRead = avail;
                if (toRead > (int)sizeof(buf)) toRead = sizeof(buf);
                int rd = 0;
                while (rd < toRead) {
                    int b = Serial.read();
                    if (b < 0) break;
                    buf[rd++] = (uint8_t)b;
                }
                if (rd > 0) {
                    f.write(buf, rd);
                    received += rd;
                }
            } else {
                delay(1);
            }
        }

        f.close();

        if (received == size) {
            Serial.println("OK");
        } else {
            Serial.println("ERR_TIMEOUT");
        }

    } else if (line.startsWith("MKDIR ")) {
        SD_MMC.mkdir(line.substring(6));
        Serial.println("OK");

    } else if (line.startsWith("SIZE ")) {
        String path = line.substring(5);
        File f = SD_MMC.open(path, FILE_READ);
        if (f) {
            Serial.print("SIZE_");
            Serial.println(f.size());
            f.close();
        } else {
            Serial.println("NOT_FOUND");
        }

    } else if (line.startsWith("END")) {
        Serial.println("DONE");
    }
}
