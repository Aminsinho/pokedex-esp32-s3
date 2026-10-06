import re
data = open('tools/app_dump.bin','rb').read()
strs = sorted(set(re.findall(rb'[\x20-\x7e]{5,}', data)))
pat = re.compile(rb'(?i)(tft|lvgl|lv_|lv_|\bspi\b|ft5|cst|gt9|axs|tp_|touch|ilitek|ili|esp_lcd|driver|library|version|init|pin_map|pins?\b|\bGPIO\d|\d+\s*:\s*\d+)', re.S)
for s in strs:
    t = s.decode()
    if pat.search(s) and not t.startswith(('/IDF','/IDF','C:' + chr(92))) and 'ESP_ERR' not in t and not re.match(r'^[EIWD] \(', t):
        print(t[:150])
