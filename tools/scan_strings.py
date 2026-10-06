import re, sys
data = open('tools/app_dump.bin','rb').read()
strs = sorted(set(re.findall(rb'[\x20-\x7e]{6,}', data)))
pat = re.compile(rb'(?i)(display|tft|lcd|st77|st75|axs|nv3|ili9|gc9a|sh86|eqm|cst8|cst3|gt9|ft5|touch|backlight|screen|devkit|lilygo|maker|wvsh|esp32-s3|board|n16|r8|wroom|serial|button|boot|reset|pwr|battery|adk)', re.S)
for s in strs:
    t = s.decode()
    skip = ('/IDF', '/IDF', 'C:' + chr(92), 'E (', 'I (', 'W (', 'D (', 'E (d', 'esp_err', 'ESP_ERR', 'esp32s3', 'esp32-', 'ESP32-')
    if pat.search(s) and not any(t.startswith(x) or x in t for x in skip):
        print(t[:160])
