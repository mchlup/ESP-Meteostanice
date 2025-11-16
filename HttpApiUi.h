#pragma once

#include <ArduinoJson.h>
#include <LittleFS.h>

// ====== UI API & SPA (LittleFS-hosted) ========================================
static const char* const UI_INDEX_PATH = "/ui/index.html";

static bool httpSendFileFromFS(const char* fsPath, const char* contentType){
  File file = LittleFS.open(fsPath, "r");
  if (!file) {
    DLOG("[HTTP] Missing file %s\r\n", fsPath);
    return false;
  }
  server.streamFile(file, contentType);
  file.close();
  return true;
}

static void handleUi(){
  if (!httpSendFileFromFS(UI_INDEX_PATH, "text/html; charset=utf-8")) {
    server.send(500, "text/plain", "UI file missing");
  }
}

static const char* detectContentType(const String& path){
  if (path.endsWith(".html")) return "text/html; charset=utf-8";
  if (path.endsWith(".css"))  return "text/css; charset=utf-8";
  if (path.endsWith(".js"))   return "application/javascript; charset=utf-8";
  if (path.endsWith(".json")) return "application/json; charset=utf-8";
  if (path.endsWith(".svg"))  return "image/svg+xml";
  if (path.endsWith(".png"))  return "image/png";
  if (path.endsWith(".ico"))  return "image/x-icon";
  if (path.endsWith(".woff2")) return "font/woff2";
  return "text/plain; charset=utf-8";
}

static bool httpTryServeStatic(const String& uri){
  String path = uri;
  if (path.endsWith("/")) path += "index.html";
  if (!LittleFS.exists(path)) return false;
  return httpSendFileFromFS(path.c_str(), detectContentType(path));
}


// ---------- Helpers to send JSON ----------
static void sendJSON(const String& s){ server.send(200, "application/json; charset=utf-8", s); }

template <typename TJsonDocument>
static void sendJSONDoc(const TJsonDocument& doc){
  String payload;
  payload.reserve(512);
  serializeJson(doc, payload);
  sendJSON(payload);
}

static void sendErr(int code, const String& msg){
  StaticJsonDocument<128> doc;
  doc["error"] = msg;

  String payload;
  serializeJson(doc, payload);
  server.send(code, "application/json; charset=utf-8", payload);
}

// ---------- API: /api/status ---------------------------------------------------
static void apiStatus(){
  // Modbus mirrors / live values
  auto H = [&](uint16_t r){ return mb.Hreg(r); };
  const uint32_t qnh = ((uint32_t)H(32) << 16) | H(33);
  const int32_t alt_cm = (int32_t)(((uint32_t)H(34) << 16) | H(35));
  char icao[5];
  readICAOFromRegs(47, 48, icao);

  const bool wifiConnected = (WiFi.status() == WL_CONNECTED);
  const IPAddress ip = WiFi.localIP();
  const bool mqttConnected = CFG.mqtt_enable && g_mqttClient.connected();

  updateSunTimesIfNeeded();

  StaticJsonDocument<1920> doc;
  doc["deviceName"] = CFG.deviceName;
  doc["fw_major"] = FW_MAJOR;
  doc["fw_minor"] = FW_MINOR;
  doc["wifi_ok"] = wifiConnected;
  doc["ip"] = wifiConnected ? ip.toString() : String("0.0.0.0");
  doc["rssi"] = WiFi.RSSI();
  doc["unitId"] = CFG.unitId;
  doc["uptime_s"] = (millis() - bootMillis) / 1000UL;
  doc["altMode"] = (unsigned)mb.Hreg(39);
  doc["pollMs"] = CFG.pollMs;
  doc["autoTest"] = CFG.autoTest;
  doc["cfgElevation_m"] = (int)CFG.cfgElevation_m;
  doc["cfgQNH_Pa"] = (unsigned long)CFG.cfgQNH_Pa;
  doc["autoQNH_enable"] = (unsigned)CFG.autoQNH_enable;
  doc["autoQNH_period_h"] = (unsigned)CFG.autoQNH_period_h;
  doc["autoQNH_manual_en"] = (unsigned)CFG.autoQNH_manual_en;
  doc["autoQNH_manual_icao"] = CFG.autoQNH_manual_icao;
  doc["mqtt_enabled"] = (unsigned)CFG.mqtt_enable;
  doc["mqtt_connected"] = mqttConnected;
  doc["mqtt_status"] = g_mqttState.status;
  doc["mqtt_host"] = CFG.mqtt_host;
  doc["mqtt_port"] = (unsigned)CFG.mqtt_port;
  doc["mqtt_baseTopic"] = CFG.mqtt_baseTopic;
  doc["mqtt_clientId"] = CFG.mqtt_clientId;
  doc["mqtt_ha_discovery"] = (unsigned)CFG.mqtt_ha_discovery;
  doc["mqtt_loxone_discovery"] = (unsigned)CFG.mqtt_loxone_discovery;
  doc["mqtt_last_state"] = g_mqttState.lastClientState;
  doc["mqtt_availability_online"] = g_mqttState.availabilityOnline;

  // sensors + derived
  doc["htu_ok"] = htu_ok;
  doc["bh_ok"] = bh_ok;
  doc["bmp_ok"] = bmp_ok;
  doc["t_htu"] = (int32_t)mb.Hreg(10);
  doc["rh_htu"] = (uint32_t)mb.Hreg(11);
  doc["t_bmp"] = (int32_t)mb.Hreg(12);
  doc["p_bmp"] = ((uint32_t)H(13) << 16) | H(14);
  doc["lux"] = ((uint32_t)H(15) << 16) | H(16);
  doc["qnh"] = qnh;
  doc["alt_cm"] = alt_cm;
  doc["td"] = (int32_t)H(40);
  doc["ah"] = (uint32_t)H(41);
  doc["vpd"] = (uint32_t)H(42);
  doc["icao"] = icao;

  doc["location_valid"] = g_locationValid;
  if (g_locationValid){
    doc["location_lat"] = g_locationLat;
    doc["location_lon"] = g_locationLon;
    doc["location_label"] = g_locationLabel;
    doc["location_source"] = g_locationSource;
  }

  if (g_sunTimesValid){
    doc["sunrise_ts"] = (uint32_t)g_sunrise_ts;
    doc["sunset_ts"] = (uint32_t)g_sunset_ts;
  } else {
    doc["sunrise_ts"] = 0;
    doc["sunset_ts"] = 0;
  }

  doc["bio_index"] = g_bioIndex;
  doc["bio_score"] = g_bioScore;
  doc["bio_label"] = g_bioLabel;
  doc["bio_comment"] = g_bioComment;
  doc["pressure_trend_hpa_h"] = g_pressureTrendPaPerHour / 100.0f;

  doc["forecast_valid"] = g_forecastValid;
  doc["forecast_summary"] = g_forecastSummary;
  doc["forecast_detail"] = g_forecastDetail;
  doc["forecast_confidence"] = g_forecastConfidence;
  doc["forecast_generated_ts"] = g_forecastGeneratedTs ? (uint32_t)g_forecastGeneratedTs : 0;
  if (isfinite(g_forecastTemp1h)) doc["forecast_temp_1h"] = g_forecastTemp1h; else doc["forecast_temp_1h"] = nullptr;
  if (isfinite(g_forecastTemp3h)) doc["forecast_temp_3h"] = g_forecastTemp3h; else doc["forecast_temp_3h"] = nullptr;
  if (isfinite(g_forecastHumidity1h)) doc["forecast_humidity_1h"] = g_forecastHumidity1h; else doc["forecast_humidity_1h"] = nullptr;
  if (isfinite(g_forecastHumidity3h)) doc["forecast_humidity_3h"] = g_forecastHumidity3h; else doc["forecast_humidity_3h"] = nullptr;
  if (isfinite(g_forecastPressure1h)) doc["forecast_pressure_1h"] = g_forecastPressure1h; else doc["forecast_pressure_1h"] = nullptr;
  if (isfinite(g_forecastPressure3h)) doc["forecast_pressure_3h"] = g_forecastPressure3h; else doc["forecast_pressure_3h"] = nullptr;
  if (isfinite(g_forecastLux1h)) doc["forecast_lux_1h"] = g_forecastLux1h; else doc["forecast_lux_1h"] = nullptr;
  if (isfinite(g_forecastLux3h)) doc["forecast_lux_3h"] = g_forecastLux3h; else doc["forecast_lux_3h"] = nullptr;

  // AutoQNH info
  doc["aq_last_result"] = (uint32_t)H(45);
  doc["aq_dist10"] = (uint32_t)H(46);
  doc["aq_last_up_s"] = ((uint32_t)H(49) << 16) | H(50);

  // I2C poslední scan sloty (96..111)
  JsonArray i2c = doc.createNestedArray("i2c");
  for (uint8_t i = 0; i < 16; i++) {
    uint16_t a = H(96 + i);
    if (a) i2c.add(a);
  }

  sendJSONDoc(doc);
}

// ---------- API: /api/config (GET/POST) ---------------------------------------
static void apiConfigGet(){
  StaticJsonDocument<640> doc;
  doc["deviceName"] = CFG.deviceName;
  doc["unitId"] = CFG.unitId;
  doc["pollMs"] = CFG.pollMs;
  doc["autoTest"] = CFG.autoTest;
  doc["cfgElevation_m"] = (int)CFG.cfgElevation_m;
  doc["cfgQNH_Pa"] = (unsigned long)CFG.cfgQNH_Pa;
  doc["altMode"] = (unsigned)CFG.altMode;
  doc["autoQNH_enable"] = (unsigned)CFG.autoQNH_enable;
  doc["autoQNH_period_h"] = (unsigned)CFG.autoQNH_period_h;
  doc["autoQNH_manual_en"] = (unsigned)CFG.autoQNH_manual_en;
  doc["autoQNH_manual_icao"] = CFG.autoQNH_manual_icao;
  doc["mqtt_enable"] = (unsigned)CFG.mqtt_enable;
  doc["mqtt_host"] = CFG.mqtt_host;
  doc["mqtt_port"] = (unsigned)CFG.mqtt_port;
  doc["mqtt_clientId"] = CFG.mqtt_clientId;
  doc["mqtt_username"] = CFG.mqtt_username;
  doc["mqtt_baseTopic"] = CFG.mqtt_baseTopic;
  doc["mqtt_ha_discovery"] = (unsigned)CFG.mqtt_ha_discovery;
  doc["mqtt_loxone_discovery"] = (unsigned)CFG.mqtt_loxone_discovery;
  doc["mqtt_password_set"] = CFG.mqtt_password.length() > 0;
  sendJSONDoc(doc);
}

template <size_t Capacity>
static bool readJsonBody(StaticJsonDocument<Capacity> &doc){
  if(!server.hasArg("plain")) return false;
  DeserializationError err = deserializeJson(doc, server.arg("plain"));
  return !err;
}

static void applyCfgFromJSON(const StaticJsonDocument<1024>& d){
  if (d.containsKey("deviceName"))        CFG.deviceName = String((const char*)d["deviceName"]);
  if (d.containsKey("unitId"))            CFG.unitId = (uint16_t)(unsigned)d["unitId"];
  if (d.containsKey("pollMs"))            CFG.pollMs = (uint32_t)(unsigned long)d["pollMs"];
  if (d.containsKey("autoTest"))          CFG.autoTest = (uint8_t)(unsigned)d["autoTest"];
  if (d.containsKey("cfgElevation_m"))    CFG.cfgElevation_m = (int16_t)(int)d["cfgElevation_m"];
  if (d.containsKey("cfgQNH_Pa"))         CFG.cfgQNH_Pa = (uint32_t)(unsigned long)d["cfgQNH_Pa"];
  if (d.containsKey("altMode"))           CFG.altMode = (uint8_t)(unsigned)d["altMode"];
  if (d.containsKey("autoQNH_enable"))    CFG.autoQNH_enable = (uint8_t)(unsigned)d["autoQNH_enable"];
  if (d.containsKey("autoQNH_period_h"))  CFG.autoQNH_period_h = (uint8_t)(unsigned)d["autoQNH_period_h"];
  if (d.containsKey("autoQNH_manual_en")) CFG.autoQNH_manual_en = (uint8_t)(unsigned)d["autoQNH_manual_en"];
  if (d.containsKey("autoQNH_manual_icao")){
    const char* s = d["autoQNH_manual_icao"]; if (s && strlen(s)==4) strlcpy(CFG.autoQNH_manual_icao, s, sizeof(CFG.autoQNH_manual_icao));
  }
  if (d.containsKey("mqtt_enable"))       CFG.mqtt_enable = (uint8_t)(unsigned)d["mqtt_enable"];
  if (d.containsKey("mqtt_host"))         CFG.mqtt_host = String((const char*)d["mqtt_host"]);
  if (d.containsKey("mqtt_port"))         CFG.mqtt_port = (uint16_t)(unsigned)d["mqtt_port"];
  if (d.containsKey("mqtt_clientId"))     CFG.mqtt_clientId = String((const char*)d["mqtt_clientId"]);
  if (d.containsKey("mqtt_username"))     CFG.mqtt_username = String((const char*)d["mqtt_username"]);
  if (d.containsKey("mqtt_password_clear") && d["mqtt_password_clear"]) CFG.mqtt_password = "";
  if (d.containsKey("mqtt_password")) {
    const char* pw = d["mqtt_password"];
    if (pw) CFG.mqtt_password = String(pw);
  }
  if (d.containsKey("mqtt_baseTopic"))    CFG.mqtt_baseTopic = String((const char*)d["mqtt_baseTopic"]);
  if (d.containsKey("mqtt_ha_discovery")) CFG.mqtt_ha_discovery = (uint8_t)(unsigned)d["mqtt_ha_discovery"];
  if (d.containsKey("mqtt_loxone_discovery")) CFG.mqtt_loxone_discovery = (uint8_t)(unsigned)d["mqtt_loxone_discovery"];
  // promítnout do Modbus mirroru i hostname
  applyConfigToModbusMirror();
  ensureMqttDefaults();
  mqttOnConfigChanged();
}

static void apiConfigPost(){
  StaticJsonDocument<1024> d;
  if(!readJsonBody(d)) return sendErr(400,"bad json");

  bool persist = d["persist"] | false;
  applyCfgFromJSON(d);
  if (persist) saveConfigFS();

  StaticJsonDocument<128> resp;
  resp["ok"] = true;
  resp["msg"] = persist ? F("Uloženo do FS.") : F("Aplikováno.");
  sendJSONDoc(resp);
}

// ---------- API: /api/action (POST) -------------------------------------------
static void apiActionPost(){
  StaticJsonDocument<192> d; if(!readJsonBody(d)) return sendErr(400,"bad json");
  const char* cmd = d["cmd"] | "";
  if (!strcmp(cmd,"scan")) {
    i2cScan();
    StaticJsonDocument<64> resp; resp["ok"] = true; sendJSONDoc(resp); return;
  }
  if (!strcmp(cmd,"selftest")) {
    sensorSelfTest();
    StaticJsonDocument<64> resp; resp["ok"] = true; sendJSONDoc(resp); return;
  }
  if (!strcmp(cmd,"aq_run")) {
    autoQNH_RunOnce();
    StaticJsonDocument<64> resp; resp["ok"] = true; sendJSONDoc(resp); return;
  }
  if (!strcmp(cmd,"i2c_reinit")) {
    reinitI2C();
    StaticJsonDocument<128> resp;
    resp["ok"] = true;
    resp["htu_ok"] = htu_ok;
    resp["bh_ok"] = bh_ok;
    resp["bmp_ok"] = bmp_ok;
    sendJSONDoc(resp);
    return;
  }
  if (!strcmp(cmd,"reboot")) {
    StaticJsonDocument<64> resp;
    resp["ok"] = true;
    resp["reboot"] = true;
    sendJSONDoc(resp);
    mqttPublishAvailability(false);
    delay(200);
    ESP.restart();
    return;
  }
  sendErr(400,"unknown cmd");
}

// ---------- API: /api/mb (GET read / POST write) ------------------------------
static void apiMBGet(){
  int addr = server.hasArg("addr") ? server.arg("addr").toInt() : 0;
  int n    = server.hasArg("n") ? server.arg("n").toInt() : 1; if(n<1) n=1; if(n>32) n=32;
  if (addr<0 || addr>199) return sendErr(400,"addr range");
  StaticJsonDocument<256> doc;
  JsonArray arr = doc.to<JsonArray>();
  for (int i = 0; i < n; i++) { arr.add((unsigned)mb.Hreg(addr + i)); }
  sendJSONDoc(doc);
}
static void apiMBPost(){
  StaticJsonDocument<160> d; if(!readJsonBody(d)) return sendErr(400,"bad json");
  int addr = d["addr"] | -1; uint16_t val = (uint16_t)(unsigned)d["val"];
  if (addr<0 || addr>199) return sendErr(400,"addr range");
  mb.Hreg(addr,val);
  StaticJsonDocument<64> resp; resp["ok"] = true; sendJSONDoc(resp);
}

// ---------- API: /api/airports -------------------------------------------------
static void apiAirports(){
  StaticJsonDocument<768> doc;
  JsonArray arr = doc.createNestedArray("airports");
  for (uint8_t i = 0; i < CZ_AIRPORTS_N; ++i) {
    Airport a; memcpy_P(&a, &CZ_AIRPORTS[i], sizeof(Airport));
    JsonObject obj = arr.createNestedObject();
    obj["icao"] = String(a.icao);
    obj["name"] = String(a.name);
    obj["lat"] = a.lat;
    obj["lon"] = a.lon;
  }
  sendJSONDoc(doc);
}

static void apiCalibrationGet(){
  CalibrationFileInfo info;
  calibrationCollectInfo(info);
  StaticJsonDocument<320> doc;
  doc["exists"] = info.exists;
  doc["entries"] = info.entries;
  doc["size_bytes"] = info.sizeBytes;
  doc["first_ts"] = (info.firstTs > 0) ? (uint32_t)info.firstTs : 0;
  doc["last_ts"] = (info.lastTs > 0) ? (uint32_t)info.lastTs : 0;
  uint32_t coverage = 0;
  if (info.entries > 1 && info.lastTs > info.firstTs){
    coverage = (uint32_t)((info.lastTs - info.firstTs) / 3600UL);
  }
  doc["coverage_h"] = coverage;
  if (!info.exists) doc["status"] = F("Soubor kalibrace zatím neexistuje.");
  else if (!info.entries) doc["status"] = F("Soubor je prázdný – čekám na první vzorek.");
  else doc["status"] = F("Historie je připravena pro predikce.");
  doc["forecast_valid"] = g_forecastValid;
  doc["forecast_summary"] = g_forecastSummary;
  doc["forecast_confidence"] = g_forecastConfidence;
  sendJSONDoc(doc);
}

static void apiCalibrationPost(){
  StaticJsonDocument<160> d; if(!readJsonBody(d)) return sendErr(400,"bad json");
  const char* cmd = d["cmd"] | "";
  if (!strcmp(cmd,"clear")){
    bool ok = calibrationDeleteHistory();
    StaticJsonDocument<160> resp;
    resp["ok"] = ok;
    resp["forecast_valid"] = g_forecastValid;
    resp["message"] = ok ? F("Kalibrační historie byla odstraněna.") : F("Soubor se nepodařilo odstranit.");
    sendJSONDoc(resp);
    return;
  }
  if (!strcmp(cmd,"refresh")){
    calibrationRegenerateForecast();
    CalibrationFileInfo info;
    calibrationCollectInfo(info);
    uint32_t coverage = 0;
    if (info.entries > 1 && info.lastTs > info.firstTs){
      coverage = (uint32_t)((info.lastTs - info.firstTs) / 3600UL);
    }
    StaticJsonDocument<192> resp;
    resp["ok"] = true;
    resp["forecast_valid"] = g_forecastValid;
    resp["entries"] = info.entries;
    resp["coverage_h"] = coverage;
    sendJSONDoc(resp);
    return;
  }
  sendErr(400,"unknown cmd");
}

// ---------- Mount routes + SPA ------------------------------------------------
void httpInstallUI(){
  // SPA
  server.on("/ui", handleUi);
  server.on("/ui/index.html", handleUi);
  server.on("/ui/", [](){ server.sendHeader("Location","/ui",true); server.send(302,"text/plain",""); });
  // Optionally make it default root:
  server.on("/", [](){ server.sendHeader("Location","/ui",true); server.send(302,"text/plain",""); });

  // API
  server.on("/api/status", HTTP_GET, apiStatus);
  server.on("/api/config", HTTP_GET, apiConfigGet);
  server.on("/api/config", HTTP_POST, apiConfigPost);
  server.on("/api/action", HTTP_POST, apiActionPost);
  server.on("/api/mb", HTTP_GET, apiMBGet);
  server.on("/api/mb", HTTP_POST, apiMBPost);
  server.on("/api/airports", HTTP_GET, apiAirports);
  server.on("/api/calibration", HTTP_GET, apiCalibrationGet);
  server.on("/api/calibration", HTTP_POST, apiCalibrationPost);

  server.onNotFound([](){
    if (httpTryServeStatic(server.uri())) return;
    server.send(404, "text/plain", "Not found");
  });
}
