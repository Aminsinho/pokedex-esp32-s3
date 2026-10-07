#include "camera_web.h"
#include "camera.h"
#include "camera_config.h"
#include "camera_settings.h"
#include "camera_backend.h"
#include "img_converters.h"
#include "WiFi.h"
#include "WebServer.h"
#include "ArduinoJson.h"
#include "esp_system.h"

static WebServer      g_server(CAM_CONTROL_PORT);
static CameraSettings g_settings;
static bool           g_settingsInited = false;

// ---- framesize parse ----
static framesize_t parse_framesize(const String& s) {
    if (s == "UXGA")   return FRAMESIZE_UXGA;
    if (s == "XGA")    return FRAMESIZE_XGA;
    if (s == "SVGA")   return FRAMESIZE_SVGA;
    if (s == "VGA")    return FRAMESIZE_VGA;
    if (s == "CIF")    return FRAMESIZE_CIF;
    if (s == "QVGA")   return FRAMESIZE_QVGA;
    if (s == "QQVGA")  return FRAMESIZE_QQVGA;
    return FRAMESIZE_VGA;
}

// ---- settings snapshot ----
CameraSettings camera_current_settings() {
    if (!g_settingsInited) { g_settings = camera_default_settings(); g_settingsInited = true; }
    return g_settings;
}
void camera_update_settings(const CameraSettings& s) {
    g_settings = s; g_settingsInited = true;
    camera_apply_image_settings(s);
}

// ---- status JSON ----
static String build_status_json() {
    CameraSettings s = camera_current_settings();
    JsonDocument doc;
    doc["ok"] = true;
    doc["sensor"] = camera_sensor_name();
    doc["live_size"] = framesize_name(s.live_size);
    doc["hq_size"]   = framesize_name(s.hq_size);
    doc["best_of_n"]  = s.best_of_n;
    doc["jpeg_quality"]   = s.jpeg_quality;
    doc["stream_quality"] = s.stream_quality;
    doc["brightness"] = s.brightness;
    doc["contrast"]   = s.contrast;
    doc["saturation"] = s.saturation;
    doc["sharpness"]  = s.sharpness;
    doc["denoise"]    = s.denoise;
    doc["gainceiling"]= s.gainceiling;
    doc["aec"]   = s.aec;  doc["aec_level"]  = s.aec_level;
    doc["agc"]   = s.agc;  doc["agc_gain"]   = s.agc_gain;
    doc["awb"]   = s.awb;  doc["awb_gain"]   = s.awb_gain;
    doc["wb_mode"]   = s.wb_mode;
    doc["bpc"] = s.bpc; doc["wpc"] = s.wpc; doc["raw_gma"] = s.raw_gma; doc["lenc"] = s.lenc;
    doc["special_effect"] = s.special_effect;
    doc["hmirror"] = s.hmirror; doc["vflip"] = s.vflip;
    doc["psram_total_kb"] = camera_psram_total();
    doc["psram_free_kb"]  = camera_psram_free();
    doc["heap_free_kb"]   = camera_heap_free();
    String out;
    serializeJson(doc, out);
    return out;
}

// ---- HTML helpers ----
static String slider(const char* id, const char* name, int min, int max, int val) {
    return String("<div class='row'><label>") + name + String("</label>")
      + String("<input type='range' id='") + id + String("' min='") + min
      + String("' max='") + max + String("' value='") + val
      + String("' oninput=\"document.getElementById('") + id + String("_v').textContent=this.value\">")
      + String("<span id='") + id + String("_v'>") + val + String("</span></div>");
}

// ---- HTML UI ----
static String build_ui() {
    IPAddress ip = WiFi.localIP();
    String html = String("<!DOCTYPE html><html><head><meta charset='utf-8'>")
      + String("<meta name='viewport' content='width=device-width,initial-scale=1'>")
      + "<title>POKEDEX CAM</title>"
      + "<style>"
      + "body{font-family:system-ui,Arial;background:#111;color:#eee;margin:12px}"
      + "h1{font-size:18px;margin:6px 0}"
      + "img{max-width:100%;background:#000;border-radius:8px}"
      + ".card{background:#1b1b1b;border-radius:10px;padding:12px;margin:10px 0}"
      + ".row{display:flex;flex-wrap:wrap;gap:10px;align-items:center;margin:6px 0}"
      + "label{min-width:150px;color:#bbb;font-size:13px}"
      + "input[type=range]{flex:1;min-width:120px}"
      + "select,input[type=number]{background:#222;color:#eee;border:1px solid #444;border-radius:6px;padding:4px}"
      + "button{background:#4a6;color:#032;border:0;border-radius:8px;padding:10px 14px;font-weight:bold;cursor:pointer}"
      + "button.warn{background:#a64}"
      + ".stat{color:#8cf;font-size:12px}"
      + ".shot{display:flex;align-items:center;gap:12px}"
      + "#hq{max-width:420px}"
      + ".ok{color:#6c6}.err{color:#c66}"
      + "</style></head><body>"
      + "<h1>POKEDEX CAM — " + String(ip.toString()) + " <span class='stat' id='sensor'></span></h1>"
      + "<div class='card'><b>LIVE VIEW</b> (stream :81)<div class='row'><img id='live' src='http://"
      + ip.toString() + ":" + String(CAM_STREAM_PORT) + "/stream'></div>"
      + "<div class='row'><button onclick='startStream()'>Start stream</button>"
      + "<button onclick='stopStream()'>Stop</button>"
      + "<span class='stat' id='mem'></span></div></div>"
      + "<div class='card'><b>CAPTURE HQ</b><div class='row shot'>"
      + "<select id='hq_size'><option>UXGA</option><option selected>XGA</option><option>SVGA</option><option>VGA</option></select>"
      + "<label>best_of_n</label><input type='number' id='best_of_n' value='" + String(g_settings.best_of_n) + "' min='1' max='5'>"
      + "<label>jpeg_q</label><input type='number' id='jpeg_quality' value='" + String(g_settings.jpeg_quality) + "' min='6' max='63'>"
      + "<button class='warn' id='btnCap' onclick='captureHQ()'>Capture HQ</button></div>"
      + "<div id='hqwrap'><img id='hq' style='display:none'><a id='hqdl' style='display:none' download='poke.jpg' target='_blank'></a></div></div>"
      + "<div class='card'><b>IMAGE SETTINGS</b>"
      + slider("brightness","Brightness",-3,3,0)
      + slider("contrast","Contrast",-3,3,0)
      + slider("saturation","Saturation",-4,4,0)
      + slider("sharpness","Sharpness",-2,2,0)
      + slider("denoise","Denoise (0-8)",0,8,0)
      + slider("aec_level","AEC level (-5..5)",-5,5,0)
      + "<div class='row'><label>AEC (auto exposure)</label><input type='checkbox' id='aec' checked><label>AWB</label><input type='checkbox' id='awb' checked><label>AGC</label><input type='checkbox' id='agc' checked><label>AWB gain</label><input type='checkbox' id='awb_gain' checked></div>"
      + "<div class='row'><label>BPC</label><input type='checkbox' id='bpc' checked><label>WPC</label><input type='checkbox' id='wpc' checked><label>Raw Gamma</label><input type='checkbox' id='raw_gma' checked><label>Lens Corr</label><input type='checkbox' id='lenc' checked></div>"
      + "<div class='row'><label>White balance</label><select id='wb_mode'><option value='0'>Auto</option><option value='1'>Sunlight</option><option value='2'>Cloudy</option><option value='3'>Office</option><option value='4'>Home</option></select>"
      + "<label>Effect</label><select id='special_effect'><option value='0'>None</option><option value='1'>Negative</option><option value='2'>Grayscale</option><option value='3'>Red tint</option><option value='4'>Green tint</option><option value='5'>Blue tint</option><option value='6'>Sepia</option></select></div>"
      + "<div class='row'><label>Mirror H</label><input type='checkbox' id='hmirror'><label>Flip V</label><input type='checkbox' id='vflip' checked></div>"
      + "<div class='row'><button onclick='applySettings()'>Apply settings</button><span id='applymsg'></span></div>"
      + "</div>"
      + "<script>"
      + "function startStream(){document.getElementById('live').src='http://" + ip.toString() + ":" + String(CAM_STREAM_PORT) + "/stream?t='+Date.now();}"
      + "function stopStream(){document.getElementById('live').removeAttribute('src');}"
      + "function applySettings(){"
      + " var body={brightness:+g('brightness').value,contrast:+g('contrast').value,saturation:+g('saturation').value,"
      + " sharpness:+g('sharpness').value,denoise:+g('denoise').value,aec_level:+g('aec_level').value,"
      + " aec:g('aec').checked,awb:g('awb').checked,agc:g('agc').checked,awb_gain:g('awb_gain').checked,"
      + " bpc:g('bpc').checked,wpc:g('wpc').checked,raw_gma:g('raw_gma').checked,lenc:g('lenc').checked,"
      + " wb_mode:+g('wb_mode').value,special_effect:+g('special_effect').value,hmirror:g('hmirror').checked,vflip:g('vflip').checked};"
      + " fetch('/control',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'data='+encodeURIComponent(JSON.stringify(body))})"
      + " .then(r=>r.json()).then(d=>{var m=document.getElementById('applymsg');m.textContent=d.ok?'OK':'ERR';m.className=d.ok?'ok':'err';refreshStatus();})"
      + " .catch(e=>{var m=document.getElementById('applymsg');m.textContent='ERR '+e;m.className='err';});}"
      + "function captureHQ(){"
      + " var btn=g('btnCap');btn.disabled=true;btn.textContent='Capturing...';"
      + " var body={hq_size:g('hq_size').value,best_of_n:+g('best_of_n').value,jpeg_quality:+g('jpeg_quality').value};"
      + " fetch('/capture_hq',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'data='+encodeURIComponent(JSON.stringify(body))})"
      + " .then(r=>{ if(!r.ok) throw new Error('http '+r.status); return r.blob(); })"
      + " .then(b=>{ var img=g('hq'); img.src=URL.createObjectURL(b); img.style.display='inline';"
      + " var a=g('hqdl'); a.href=img.src; a.download='poke.jpg'; a.style.display='inline'; a.textContent='Download ('+b.size+' bytes)';"
      + " btn.disabled=false;btn.textContent='Capture HQ';})"
      + " .catch(e=>{btn.disabled=false;btn.textContent='Capture HQ';var m=document.getElementById('applymsg');m.textContent='CAPTURE FAIL '+e;m.className='err';});}"
      + "function g(id){return document.getElementById(id);}"
      + "function refreshStatus(){fetch('/status').then(r=>r.json()).then(d=>{"
      + " g('sensor').textContent=' | '+d.sensor+' | live='+d.live_size+' | hq='+d.hq_size;"
      + " g('mem').textContent='PSRAM: '+d.psram_free_kb+'/'+d.psram_total_kb+' KB | heap: '+d.heap_free_kb+' KB';});}"
      + "refreshStatus();setInterval(refreshStatus,2000);"
      + "function slider(id,name,min,max,val){return '<div class=\"row\"><label>'+name+'</label><input type=\"range\" id=\"'+id+'\" min=\"'+min+'\" max=\"'+max+'\" value=\"'+val+'\"><span id=\"'+id+'_v\">'+val+'</span></div>';}"
      + "</script></body></html>";
    return html;
}

// ---- control handlers ----
static void handle_status() {
    String json = build_status_json();
    g_server.send(200, "application/json", json);
}

static void handle_control() {
    String body = g_server.arg("data");
    JsonDocument doc;
    if (deserializeJson(doc, body)) { g_server.send(400, "application/json", "{\"ok\":false,\"err\":\"json\"}"); return; }
    CameraSettings s = camera_current_settings();
    auto setI = [&](const char* k, int& dst) { if (doc.containsKey(k)) dst = doc[k] | dst; };
    setI("brightness", s.brightness);  setI("contrast", s.contrast);
    setI("saturation", s.saturation);  setI("sharpness", s.sharpness);
    setI("denoise", s.denoise);        setI("gainceiling", s.gainceiling);
    setI("aec_level", s.aec_level);    setI("agc_gain", s.agc_gain);
    setI("best_of_n", s.best_of_n);    setI("jpeg_quality", s.jpeg_quality);
    setI("stream_quality", s.stream_quality);
    setI("wb_mode", s.wb_mode);        setI("special_effect", s.special_effect);
    auto setB = [&](const char* k, bool& dst) { if (doc.containsKey(k)) dst = doc[k] | dst; };
    setB("aec", s.aec);  setB("awb", s.awb);  setB("agc", s.agc);  setB("awb_gain", s.awb_gain);
    setB("bpc", s.bpc);  setB("wpc", s.wpc);  setB("raw_gma", s.raw_gma);  setB("lenc", s.lenc);
    setB("hmirror", s.hmirror); setB("vflip", s.vflip);
    if (doc.containsKey("live_size")) s.live_size = parse_framesize(doc["live_size"].as<String>());
    if (doc.containsKey("hq_size"))   s.hq_size   = parse_framesize(doc["hq_size"].as<String>());
    camera_update_settings(s);
    g_server.send(200, "application/json", build_status_json());
}

void stream_task_force_restart(); // forward decl

// ---- /scan: full AI recognition pipeline ----
static bool scan_capture_fn(uint8_t** out, size_t* outLen) {
    CameraSettings s = camera_current_settings();
    return camera_capture_hq(s, out, outLen);
}

static void handle_scan() {
    unsigned long t0 = millis();
    Serial.printf("[SCAN] POST /scan received\n");

    ScanResult result;
    memset(&result, 0, sizeof(result));

    int rc = camera_run_scan(BACKEND_HOST, BACKEND_PORT, scan_capture_fn, &result);

    String json = String("{\"ok\":") + (rc == 1 ? "true" : "false") + String(",\"total_ms\":") + String(millis() - t0);
    if (rc == 1) {
        json += String(",\"scan\":{\"status\":\"completed\",\"pokemon_id\":") + String(result.pokemon_id);
        json += String(",\"pokemon_name\":\"") + String(result.pokemon_name) + String("\"");
        json += String(",\"confidence\":") + String(result.confidence, 3);
        json += String(",\"match_score\":") + String(result.match_score, 3);
        json += String(",\"ai_time_ms\":") + String(result.time_ms);
        json += "}";
    } else {
        json += String(",\"error\":\"") + String(result.error_msg[0] ? result.error_msg : "scan failed") + String("\"");
    }
    json += "}";

    g_server.send(rc == 1 ? 200 : 500, "application/json", json);
}

static void handle_capture_hq() {
    String body = g_server.arg("data");
    CameraSettings s = camera_current_settings();
    if (body.length()) {
        JsonDocument doc;
        if (!deserializeJson(doc, body)) {
            if (doc.containsKey("hq_size")) s.hq_size = parse_framesize(doc["hq_size"].as<String>());
            if (doc.containsKey("best_of_n")) s.best_of_n = doc["best_of_n"] | s.best_of_n;
            if (doc.containsKey("jpeg_quality")) s.jpeg_quality = doc["jpeg_quality"] | s.jpeg_quality;
        }
    }
    camera_update_settings(s);
    uint8_t* jpg = nullptr; size_t jlen = 0;
    if (!camera_capture_hq(s, &jpg, &jlen) || !jpg) {
        g_server.send(500, "application/json", "{\"ok\":false,\"err\":\"capture failed\"}");
        return;
    }
    g_server.send(200, "image/jpeg", String((char*)jpg, jlen));
    free(jpg);
    // No reboot needed: set_framesize() doesn't corrupt PSRAM/LWIP state.
}

// ---- MJPEG stream (port 81) ----
static void send_stream_frame(WiFiClient& client, int quality) {
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) return;
    uint8_t* jpg = nullptr; size_t jlen = 0;
    bool ok = frame2jpg(fb, (uint8_t)quality, &jpg, &jlen);
    esp_camera_fb_return(fb);
    if (!ok) { if (jpg) free(jpg); return; }
    client.print("--frame\r\n");
    client.print("Content-Type: image/jpeg\r\n");
    client.print("Content-Length: ");
    client.print(jlen);
    client.print("\r\n\r\n");
    client.write(jpg, jlen);
    free(jpg);
}

// --- raw LWIP stream server (bypasses WiFiServer) ---
#include "lwip/sockets.h"

static TaskHandle_t g_streamTask = nullptr;

static int stream_make_server() {
    int srv = lwip_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (srv < 0) return -1;
    int opt = 1;
    lwip_setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(CAM_STREAM_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (lwip_bind(srv, (struct sockaddr*)&addr, sizeof(addr)) != 0) { lwip_close(srv); return -1; }
    if (lwip_listen(srv, 2) != 0) { lwip_close(srv); return -1; }
    return srv;
}

// ---- /scan handler on the raw LWIP server ----
static void handle_scan_lwip(int client) {
    Serial.println("[SCAN] request received");
    uint32_t t0 = millis();

    ScanResult res;
    int rc = camera_run_scan(BACKEND_HOST, BACKEND_PORT, scan_capture_fn, &res);
    uint32_t elapsed = millis() - t0;

    // Build JSON response (only fields that exist in ScanResult)
    char body[256];
    int blen;
    if (rc == 1) { // completed
        blen = snprintf(body, sizeof(body),
            "{\"ok\":true,\"pokemon_name\":\"%s\",\"pokemon_id\":%d,"
            "\"confidence\":%.2f,\"match_score\":%.2f,\"time_ms\":%d}",
            res.pokemon_name, res.pokemon_id,
            res.confidence, res.match_score, res.time_ms);
    } else {
        blen = snprintf(body, sizeof(body), "{\"ok\":false,\"error\":\"%s\"}", res.error_msg[0] ? res.error_msg : "scan_failed");
    }

    const char* status = (rc == 1) ? "200 OK" : "500 Internal Server Error";
    char hdr[160];
    int hl = snprintf(hdr, sizeof(hdr),
        "HTTP/1.1 %s\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %d\r\n"
        "Access-Control-Allow-Origin: *\r\n\r\n", status, blen);

    lwip_send(client, hdr, hl, 0);
    lwip_send(client, body, blen, 0);

    Serial.printf("[SCAN] done rc=%d in %lums\n", rc, (unsigned long)elapsed);
}

static void stream_task(void*) {
    int srv = stream_make_server();
    if (srv < 0) {
        Serial.printf("[STREAM] init FAIL\n");
        vTaskDelete(nullptr);
        return;
    }
    Serial.printf("[STREAM] listening on :%d (stream+scan)\n", CAM_STREAM_PORT);

    while (true) {
        struct sockaddr_in caddr;
        socklen_t clen = sizeof(caddr);
        int client = lwip_accept(srv, (struct sockaddr*)&caddr, &clen);
        if (client < 0) { vTaskDelay(pdMS_TO_TICKS(10)); continue; }

        // Read HTTP request
        vTaskDelay(pdMS_TO_TICKS(20));
        char req[512];
        int n = lwip_recv(client, req, sizeof(req)-1, 0);
        if (n > 0) req[n] = 0;
        else { lwip_close(client); continue; }

        // Route based on path
        if (strstr(req, "/scan")) {
            handle_scan_lwip(client);
        } else {
            // Default: MJPEG stream
            CameraSettings s = camera_current_settings();
            const char* hdr = "HTTP/1.1 200 OK\r\n"
                             "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n"
                             "Cache-Control: no-cache\r\n"
                             "Access-Control-Allow-Origin: *\r\n"
                             "\r\n";
            lwip_send(client, hdr, strlen(hdr), 0);

            uint32_t start = millis();
            while (true) {
                if (millis() - start > WEB_TIMEOUT_MS) break;
                if (camera_hq_busy()) { vTaskDelay(pdMS_TO_TICKS(50)); continue; }
                if (!camera_lock(30)) { vTaskDelay(pdMS_TO_TICKS(5)); continue; }

                char hdr2[128];
                uint8_t* jpg = nullptr; size_t jlen = 0;
                if (camera_grab_jpeg(s.stream_quality, &jpg, &jlen) && jpg) {
                    int hl = snprintf(hdr2, sizeof(hdr2),
                        "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n",
                        (unsigned)jlen);
                    if (lwip_send(client, hdr2, hl, 0) < 0) { free(jpg); camera_unlock(); break; }
                    if (lwip_send(client, (const char*)jpg, jlen, 0) < 0) { free(jpg); camera_unlock(); break; }
                    free(jpg);
                }
                camera_unlock();
                vTaskDelay(pdMS_TO_TICKS(50));
            }
        }
        lwip_shutdown(client, 0);
        lwip_close(client);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

static void control_task(void*) {
    while (true) {
        g_server.handleClient();
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void camera_web_start() {
    g_settings = camera_default_settings();
    g_settingsInited = true;
    camera_apply_image_settings(g_settings);

    g_server.on("/", HTTP_GET, []() { g_server.send(200, "text/html", build_ui()); });
    g_server.on("/status", HTTP_GET, handle_status);
    g_server.on("/control", HTTP_POST, handle_control, []() { g_server.sendHeader("Connection","close"); });
    g_server.on("/capture_hq", HTTP_POST, handle_capture_hq, []() { g_server.sendHeader("Connection","close"); });
    g_server.on("/scan", HTTP_POST, handle_scan, []() { g_server.sendHeader("Connection","close"); });
    g_server.onNotFound([]() { g_server.send(404, "text/plain", "not found"); });
    g_server.begin();

    if (xTaskCreate(control_task, "cam_ctrl", 16384, nullptr, 1, nullptr) != pdPASS)
        Serial.println("[WEB] FATAL: control_task failed to start");
    if (xTaskCreate(stream_task, "cam_stream", 16384, nullptr, 1, &g_streamTask) != pdPASS)
        Serial.println("[WEB] FATAL: stream_task failed to start");
    else
        Serial.printf("[WEB] control :%d  stream :%d\n", CAM_CONTROL_PORT, CAM_STREAM_PORT);
}

// After HQ capture, the camera reinit (deinit+init) can crash the stream task
// due to PSRAM fragmentation affecting LWIP. Safest recovery: reboot.
// The JPEG has already been sent to the client before this is called.
void stream_task_force_restart() {
    Serial.println("[CAM] HQ done, rebooting in 2s to restore stream...");
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();
}
