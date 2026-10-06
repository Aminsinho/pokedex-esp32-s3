import re
data = open('tools/app_dump.bin','rb').read()
strs = sorted(set(re.findall(rb'[\x20-\x7e]{10,}', data)))
# candidate board/factory names
for s in strs:
    t = s.decode()
    if re.search(r'(?i)(devkit|t-display|lilygo|makerfabs|m5stack|cores3|freenove|xiao|seeed|wvsh|waveshare|touch-lcd|3\.5|320|240|480|640|170|320x|240x|esp32)', t) and not t.startswith(('/IDF','/IDF','C:' + chr(92))) and 'ESP_ERR' not in t and not re.match(r'^[EIWD] \(', t):
        print(t[:150])
