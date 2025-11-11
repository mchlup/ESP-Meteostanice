/*
  Meteostanice ESP8266 (HTU21D, BH1750, BMP180) s Modbus TCP (emelianov/modbus-esp8266)
  + WiFiManager modeless portál (krátký stisk FLASH GPIO0, IRQ)
  + RGB LED (ESP-Witty: R=GPIO15, G=GPIO12, B=GPIO13)
  + Diagnostika I2C (scanner), self-test senzorů
  + Odvozené veličiny: QNH / nadmořská výška (hypsometrie s T_v), rosný bod, absolutní vlhkost, VPD
  + AutoQNH: orientační poloha (IP), nejbližší METAR (NOAA TGFTP), auto-aktualizace (periodicky, fallback)
  + Modbus TCP echo self-test (reg 70..77) pro ověření komunikace s Loxone
  + **NOVĚ: Serial DEBUG konzole pro nastavení parametrů**

  WiFiManager captive portal – custom parametry:
    Device Name, Modbus Unit ID, Poll Interval (ms), AutoTest (0/1),
    Elevation (m), QNH (Pa), AltMode (0=AUTO,1=QNH,2=ISA,3=ELEV),
    AutoQNH (0/1), AutoQNH period (h), AutoQNH Manual ICAO (0/1), Manual ICAO (4 znaky)

  Konfigurace v LittleFS: /config.json
  Modbus TCP server: port 502, holding registry 0..199 (0-based)

  Vybrané registry:
    0  status bitmask (bit0 HTU, bit1 BH1750, bit2 BMP)
    1  unitId (info), 2 FW_MAJOR, 3 FW_MINOR
    10 T_HTU cdegC (S16), 11 RH_HTU c% (U16), 12 T_BMP cdegC (S16)
    13-14 P Pa (U32), 15-16 Lux (U32), 20-21 Uptime s (U32)
    32-33 QNH Pa (U32), 34-35 ALT cm (S32), 36 Elev m (S16), 37-38 QNH_cfg Pa (U32), 39 AltMode (U16)
    40 Td cdegC (S16), 41 AH x100 g/m3 (U16), 42 VPD Pa (U16)
    43 AutoQNH enable (0/1), 44 AutoQNH period h
    45 AutoQNH last result (0=OK,1=WiFi,2=Geo,3=Near,4=METAR), 46 AutoQNH distance*10 km (U16)
    47-48 AutoQNH ICAO (ASCII páry), 49-50 AutoQNH last update uptime s (U32)
    51 Manual ICAO enable (0/1), 52-53 Manual ICAO (ASCII páry)
    70 MB Test Cmd (rezervace), 71 MB Test Result, 72/73 FW mirror, 74 token, 75 echo return (W), 76-77 echo OK time (U32)
    90..99 I2C diag, 112..114 self-test result, 100 save&reboot (W=1), 101 I2C scan (W=1), 102 self-test (W=1)

  FW verze: 2.1-autoqnh-echo
*/
// === ESP32-C3 POWEBSERVER ===
#if defined(ARDUINO_ARCH_ESP32)
  #include <WebServer.h>
  WebServer server(80);
#else
  #include <ESP8266WebServer.h>
  ESP8266WebServer server(80);
#endif

// === ESP32-C3 PORT: platform guardy ===
#if defined(ARDUINO_ARCH_ESP32)
  #include <WiFi.h>
  #include <WiFiClient.h>
  //#include <WiFiClientSecure.h>
  #include <HTTPClient.h>
  #define ISR_ATTR IRAM_ATTR
#else
  #include <ESP8266WiFi.h>
  #include <WiFiClient.h>
  //#include <WiFiClientSecure.h>
  #include <ESP8266HTTPClient.h>
  #define ISR_ATTR ICACHE_RAM_ATTR
#endif

#include <WiFiManager.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <pgmspace.h>
#include <BH1750.h>
#include <Adafruit_BMP085.h>
#include <SparkFunHTU21D.h>
#include <ModbusIP_ESP8266.h>
#include <PubSubClient.h>
#include <math.h>
#include <time.h>
#include <stdlib.h>
#include <ctype.h>

// ----------------------------- DEBUG ------------------------------------------
#define DEBUG 1
static const uint32_t DEBUG_INTERVAL_MS = 10000;
#if DEBUG
  #define DLOG(...)  do { Serial.printf(__VA_ARGS__); } while(0)
#else
  #define DLOG(...)  do {} while(0)
#endif

// ----------------------------- Wi-Fi ------------------------------------------
#define WIFI_ENABLE_LIGHT_SLEEP  1
// ESP8266: číslo v dBm, ESP32: enum wifi_power_t
#if defined(ARDUINO_ARCH_ESP32)
  // nic – řeší se v setupWiFi()
#else
  #define WIFI_TX_POWER_DBM        17.0f
#endif
// ----------------------------- I2C PINY (ESP8266 Witty) -----------------------
#ifndef I2C_SDA_PIN
  #if defined(ARDUINO_ARCH_ESP32)
    #define I2C_SDA_PIN 8     // ESP32-C3 Super Mini (doporučeno)
  #else
    #define I2C_SDA_PIN 4     // ESP8266 D2
  #endif
#endif
#ifndef I2C_SCL_PIN
  #if defined(ARDUINO_ARCH_ESP32)
    #define I2C_SCL_PIN 9    // ESP32-C3 Super Mini (doporučeno)
  #else
    #define I2C_SCL_PIN 5     // ESP8266 D1
  #endif
#endif
// ----------------------------- RGB LED (ESP-Witty) ----------------------------
// C3 Super Mini typicky NEMÁ 3× GPIO pro RGB jako ESP-Witty.
// Zavedl jsem fallback: pokud není definován RGB trojpin, použije se SINGLE LED.
#if !defined(LED_R_PIN) && !defined(LED_G_PIN) && !defined(LED_B_PIN)
  // Single LED mód (HIGH = svítí). Změň dle desky (často 8 nebo 2).
  #define SINGLE_LED_PIN -1
#endif

enum class LedMode : uint8_t { SOLID, BLINK };
struct LedPattern {
  LedMode mode = LedMode::SOLID;
  bool r = 0, g = 0, b = 0;
  uint16_t onMs = 0, offMs = 0;
  bool phaseOn = false;
  uint32_t lastMs = 0;
} g_led;

inline void ledWrite(bool r, bool g, bool b){
#if defined(SINGLE_LED_PIN)
  // Mapuji: "zelená OK" → ON, jinak blikání podle patternu
  bool on = (r||g||b);
  digitalWrite(SINGLE_LED_PIN, on ? HIGH : LOW);
#else
  digitalWrite(LED_R_PIN,r); digitalWrite(LED_G_PIN,g); digitalWrite(LED_B_PIN,b);
#endif
}
void ledSetSolid(bool r,bool g,bool b){ g_led={LedMode::SOLID,r,g,b,0,0,true,0}; ledWrite(r,g,b); }
void ledSetBlink(bool r,bool g,bool b,uint16_t onMs,uint16_t offMs){ g_led={LedMode::BLINK,r,g,b,onMs,offMs,true,millis()}; ledWrite(r,g,b); }
void ledUpdate(){ if(g_led.mode!=LedMode::BLINK) return; uint32_t now=millis(); uint16_t per=g_led.phaseOn?g_led.onMs:g_led.offMs; if(now-g_led.lastMs>=per){ g_led.phaseOn=!g_led.phaseOn; g_led.lastMs=now; ledWrite(g_led.phaseOn?g_led.r:false, g_led.phaseOn?g_led.g:false, g_led.phaseOn?g_led.b:false);} }
// ----------------------------- FLASH tlačítko (GPIO0, IRQ) --------------------
#ifndef FLASH_BTN_PIN
  #if defined(ARDUINO_ARCH_ESP32)
    #define FLASH_BTN_PIN 2
  #else
    #define FLASH_BTN_PIN 0
  #endif
#endif
#define BTN_ISR_GUARD_MS        250
#define PORTAL_DISCONNECT_STA   1
volatile bool g_flashReq = false;
volatile uint32_t g_btnLastIsrMs = 0;
ISR_ATTR void isrFlash(){
  uint32_t now = millis();
  if (now - g_btnLastIsrMs > BTN_ISR_GUARD_MS) { g_btnLastIsrMs = now; g_flashReq = true; }
}
bool g_portalActive = false;

// ----------------------------- Modbus -----------------------------------------
ModbusIP mb;

// ----------------------------- FW verze ---------------------------------------
static const uint16_t FW_MAJOR = 2;
static const uint16_t FW_MINOR = 1;

// ----------------------------- Konfigurace ------------------------------------
struct AppConfig {
  String  deviceName = "ESP-MeteoStanice";
  uint16_t unitId    = 1;
  uint32_t pollMs    = 10000;
  uint8_t  autoTest  = 0;
  int16_t  cfgElevation_m = 0;     // m AMSL (S16)
  uint32_t cfgQNH_Pa      = 0;     // Pa (U32)
  uint8_t  altMode        = 0;     // 0=AUTO,1=QNH,2=ISA,3=ELEV
  uint8_t  autoQNH_enable = 1;     // 0/1
  uint8_t  autoQNH_period_h = 8;   // výchozí 3× denně
  uint8_t  autoQNH_manual_en = 0;  // manuální ICAO override
  char     autoQNH_manual_icao[5] = ""; // 4 znaky + '\0'
  uint8_t  mqtt_enable = 0;
  String   mqtt_host = "";
  uint16_t mqtt_port = 1883;
  String   mqtt_clientId = "";
  String   mqtt_username = "";
  String   mqtt_password = "";
  String   mqtt_baseTopic = "";
  uint8_t  mqtt_ha_discovery = 1;
  uint8_t  mqtt_loxone_discovery = 1;
};
AppConfig CFG;

bool shouldSaveConfig = false;
const char* CONFIG_PATH = "/config.json";

static String chipIdHex(){
#if defined(ARDUINO_ARCH_ESP32)
  uint64_t mac = ESP.getEfuseMac();
  char buf[17];
  snprintf(buf, sizeof(buf), "%012llX", (unsigned long long)mac);
  return String(buf);
#else
  uint32_t id = ESP.getChipId();
  char buf[11];
  snprintf(buf, sizeof(buf), "%06X", id & 0xFFFFFF);
  return String(buf);
#endif
}

static String sanitizeIdentifier(const String& in){
  String out;
  out.reserve(in.length());
  for (size_t i = 0; i < in.length(); ++i) {
    char c = in[i];
    if (isalnum((unsigned char)c)) {
      out += (char)tolower((unsigned char)c);
    } else if (c == '-' || c == '_' ) {
      out += c;
    }
  }
  if (!out.length()) out = F("esp_meteostanice");
  return out;
}

static String sanitizeClientId(const String& raw){
  String out;
  out.reserve(raw.length());
  for (size_t i = 0; i < raw.length(); ++i) {
    char c = raw[i];
    if (isalnum((unsigned char)c) || c == '-' || c == '_' ) {
      out += c;
    }
  }
  if (!out.length()) out = String(F("ESP-Meteo-")) + chipIdHex();
  const size_t MAX_LEN = 32;
  if (out.length() > MAX_LEN) out.remove(MAX_LEN);
  return out;
}

static String sanitizeTopicBase(const String& raw){
  String out;
  out.reserve(raw.length());
  bool prevSlash = true;
  for (size_t i = 0; i < raw.length(); ++i) {
    char c = raw[i];
    if (isalnum((unsigned char)c)) {
      out += (char)tolower((unsigned char)c);
      prevSlash = false;
    } else if (c == '-' || c == '_') {
      out += c;
      prevSlash = false;
    } else if (c == '/') {
      if (prevSlash) continue;
      out += '/';
      prevSlash = true;
    }
  }
  while (out.startsWith("/")) out.remove(0, 1);
  while (out.endsWith("/")) out.remove(out.length() - 1);
  if (!out.length()) out = F("meteostanice");
  return out;
}

static String defaultMqttClientId(){
  String id = sanitizeClientId(CFG.deviceName.length() ? (CFG.deviceName + String('-') + chipIdHex()) : String());
  return id;
}

static String defaultMqttBaseTopic(){
  String base = String(F("meteostanice/")) + sanitizeIdentifier(CFG.deviceName.length() ? CFG.deviceName : String(F("esp")));
  base += '/';
  base += chipIdHex();
  return sanitizeTopicBase(base);
}

static void ensureMqttDefaults(){
  CFG.mqtt_clientId = sanitizeClientId(CFG.mqtt_clientId.length() ? CFG.mqtt_clientId : defaultMqttClientId());
  CFG.mqtt_baseTopic = sanitizeTopicBase(CFG.mqtt_baseTopic.length() ? CFG.mqtt_baseTopic : defaultMqttBaseTopic());
  if (CFG.mqtt_port == 0 || CFG.mqtt_port > 65535) CFG.mqtt_port = 1883;
}

WiFiClient g_mqttNetClient;
PubSubClient g_mqttClient(g_mqttNetClient);

struct {
  bool configDirty = true;
  bool discoverySent = false;
  bool needHaClear = false;
  bool needLoxClear = false;
  bool availabilityOnline = false;
  uint32_t lastConnectAttempt = 0;
  uint32_t lastStatePublish = 0;
  String status = F("MQTT neaktivní");
  int lastClientState = 0;
} g_mqttState;

static uint8_t g_mqttPrevEnable = 0;
static uint8_t g_mqttPrevHaDiscovery = 1;
static uint8_t g_mqttPrevLoxDiscovery = 1;

static String mqttGetBaseTopic(){ return CFG.mqtt_baseTopic; }
static String mqttAvailabilityTopic(){ return mqttGetBaseTopic() + F("/status"); }
static String mqttDeviceId(){ return sanitizeIdentifier(CFG.deviceName.length() ? CFG.deviceName : String(F("esp_meteo"))) + String('_') + chipIdHex(); }
static String mqttStateTopic(const char* key){ String base = mqttGetBaseTopic(); if (!base.length()) return String(); String topic = base; topic += '/'; topic += key; return topic; }
static String mqttGetClientId(){ return CFG.mqtt_clientId; }

static void mqttOnConfigChanged();
static void mqttPublishAvailability(bool online);
static void mqttPublishState(bool force);
static void mqttLoop();
static void mqttPublishDiscovery();
static void mqttPublishHaDiscovery();
static void mqttPublishLoxoneDiscovery();
static void mqttPublishHaClear();
static void mqttPublishLoxoneClear();

// ----------------------------- WiFiManager params -----------------------------
WiFiManager wm;
char deviceNameBuf[33] = "ESP-MeteoStanice";
char unitIdBuf[8]      = "1";
char pollMsBuf[12]     = "10000";
char autoTestBuf[4]    = "0";
char elevBuf[8]        = "0";
char qnhBuf[12]        = "0";
char altModeBuf[4]     = "0";
char autoQNHBuf[4]     = "1";
char autoQNHperBuf[4]  = "8";
char aqManualEnBuf[4]  = "0";
char aqManualICAOBuf[6]= "";

WiFiManagerParameter p_deviceName("devname", "Device Name", deviceNameBuf, sizeof(deviceNameBuf));
WiFiManagerParameter p_unitId("unitid", "Modbus Unit ID (1-247)", unitIdBuf, sizeof(unitIdBuf));
WiFiManagerParameter p_pollMs("pollms", "Poll interval (ms)", pollMsBuf, sizeof(pollMsBuf));
WiFiManagerParameter p_autoTest("autotest", "AutoTest at boot (0/1)", autoTestBuf, sizeof(autoTestBuf));
WiFiManagerParameter p_elev("elev_m", "Elevation (m AMSL)", elevBuf, sizeof(elevBuf));
WiFiManagerParameter p_qnh ("qnh_pa", "QNH (Pa, optional)",  qnhBuf,  sizeof(qnhBuf));
WiFiManagerParameter p_altm("altmode","AltMode (0=AUTO,1=QNH,2=ISA,3=ELEV)", altModeBuf, sizeof(altModeBuf));
WiFiManagerParameter p_aqen("autoqnh","AutoQNH (0/1)", autoQNHBuf, sizeof(autoQNHBuf));
WiFiManagerParameter p_aqpr("aqperiod","AutoQNH period (h)", autoQNHperBuf, sizeof(autoQNHperBuf));
WiFiManagerParameter p_aqme("aqmanen","AutoQNH Manual ICAO (0/1)", aqManualEnBuf, sizeof(aqManualEnBuf));
WiFiManagerParameter p_aqmc("aqicao","Manual ICAO (4 chars)", aqManualICAOBuf, sizeof(aqManualICAOBuf));

void onSaveParams() { shouldSaveConfig = true; }

// ----------------------------- Senzory ----------------------------------------
HTU21D htu;
BH1750 bh1750;
Adafruit_BMP085 bmp;

bool htu_ok = false, bh_ok = false, bmp_ok = false;

// Poslední hodnoty
float   g_htu_t = NAN;   // °C
float   g_htu_h = NAN;   // %
float   g_bmp_t = NAN;   // °C
uint32_t g_bmp_p = 0;    // Pa
float   g_bh_lux = NAN;  // lx

// ----------------------------- LED stavové flagy ------------------------------
bool g_i2cScanActive  = false;
bool g_selfTestActive = false;

// ----------------------------- Modbus registry --------------------------------
const uint16_t HOLD_COUNT  = 200;

// ----------------------------- Časování ---------------------------------------
uint32_t lastPollMs  = 0;
uint32_t bootMillis  = 0;
uint32_t lastDbgMs   = 0;

// AutoQNH plánovač
uint32_t lastAutoQNH_ms = 0;

// Modbus echo self-test
uint16_t mbEchoToken = 0;
uint32_t mbEchoLastMs = 0;

// ----------------------------- Lokace & slunce --------------------------------
enum { LOC_PRIORITY_DEFAULT = 0, LOC_PRIORITY_GEO = 1, LOC_PRIORITY_MANUAL = 2 };
static const float DEFAULT_LAT = 50.0755f;
static const float DEFAULT_LON = 14.4378f;

struct GeoCache {
  bool   valid = false;
  float  lat   = DEFAULT_LAT;
  float  lon   = DEFAULT_LON;
  String label = "Praha";
};

static float  g_locationLat = DEFAULT_LAT;
static float  g_locationLon = DEFAULT_LON;
static bool   g_locationValid = false;
static uint8_t g_locationPriority = LOC_PRIORITY_DEFAULT;
static String g_locationLabel = "Praha (výchozí)";
static String g_locationSource = "Výchozí";
static GeoCache g_geoCache;

static time_t g_sunrise_ts = 0;
static time_t g_sunset_ts  = 0;
static bool   g_sunTimesValid = false;
static uint32_t g_lastSunCalcMs = 0;

static int   g_bioIndex = -1;
static float g_bioScore = 0.0f;
static float g_pressureTrendPaPerHour = 0.0f;
static uint32_t g_lastPressurePa = 0;
static uint32_t g_lastPressureMs = 0;
static String g_bioLabel = "Bez dat";
static String g_bioComment = "Čekám na stabilní měření.";

static void setLocation(float lat, float lon, const String& label, const String& source, uint8_t priority){
  if (!isfinite(lat) || !isfinite(lon)) return;
  if (priority < g_locationPriority) return;
  g_locationLat = lat;
  g_locationLon = lon;
  g_locationLabel = label;
  g_locationSource = source;
  g_locationPriority = priority;
  g_locationValid = true;
  g_sunTimesValid = false; // přepočet při dalším dotazu
}

static void resetToDefaultLocation(){
  g_locationPriority = LOC_PRIORITY_DEFAULT;
  setLocation(DEFAULT_LAT, DEFAULT_LON, String("Praha (výchozí)"), String("Výchozí"), LOC_PRIORITY_DEFAULT);
}

// ----------------------------- Helpers ----------------------------------------
static inline void fromUint32Split(uint32_t v, uint16_t &hi, uint16_t &lo){ hi=uint16_t((v>>16)&0xFFFF); lo=uint16_t(v&0xFFFF); }
static inline void writeU32ToRegs(uint16_t regHi, uint32_t v){ uint16_t hi,lo; fromUint32Split(v,hi,lo); mb.Hreg(regHi,hi); mb.Hreg(regHi+1,lo); }
static inline void readICAOFromRegs(uint16_t rA, uint16_t rB, char out[5]){
  uint16_t a=mb.Hreg(rA), b=mb.Hreg(rB);
  out[0]=(char)(a>>8); out[1]=(char)(a&0xFF); out[2]=(char)(b>>8); out[3]=(char)(b&0xFF); out[4]='\0';
}
static inline void writeICAOToRegs(uint16_t rA, uint16_t rB, const char* icao){
  uint16_t A=((uint16_t)(uint8_t)icao[0]<<8) | (uint16_t)(uint8_t)icao[1];
  uint16_t B=((uint16_t)(uint8_t)icao[2]<<8) | (uint16_t)(uint8_t)icao[3];
  mb.Hreg(rA,A); mb.Hreg(rB,B);
}

// ---- FS config I/O ----
bool saveConfigFS(){
  StaticJsonDocument<896> doc;
  doc["deviceName"]=CFG.deviceName; doc["unitId"]=CFG.unitId; doc["pollMs"]=CFG.pollMs; doc["autoTest"]=CFG.autoTest;
  doc["cfgElevation_m"]=CFG.cfgElevation_m; doc["cfgQNH_Pa"]=CFG.cfgQNH_Pa; doc["altMode"]=CFG.altMode;
  doc["autoQNH_enable"]=CFG.autoQNH_enable; doc["autoQNH_period_h"]=CFG.autoQNH_period_h;
  doc["autoQNH_manual_en"]=CFG.autoQNH_manual_en; doc["autoQNH_manual_icao"]=CFG.autoQNH_manual_icao;
  doc["mqtt_enable"] = CFG.mqtt_enable;
  doc["mqtt_host"] = CFG.mqtt_host;
  doc["mqtt_port"] = CFG.mqtt_port;
  doc["mqtt_clientId"] = CFG.mqtt_clientId;
  doc["mqtt_username"] = CFG.mqtt_username;
  doc["mqtt_password"] = CFG.mqtt_password;
  doc["mqtt_baseTopic"] = CFG.mqtt_baseTopic;
  doc["mqtt_ha_discovery"] = CFG.mqtt_ha_discovery;
  doc["mqtt_loxone_discovery"] = CFG.mqtt_loxone_discovery;
  File f=LittleFS.open(CONFIG_PATH,"w"); if(!f) return false; if(serializeJson(doc,f)==0){ f.close(); return false; } f.close(); return true;
}
bool loadConfigFS(){
  if(!LittleFS.exists(CONFIG_PATH)) return false;
  File f=LittleFS.open(CONFIG_PATH,"r"); if(!f) return false;
  StaticJsonDocument<896> doc; DeserializationError err=deserializeJson(doc,f); f.close(); if(err) return false;
  CFG.deviceName=doc["deviceName"].as<String>(); CFG.unitId=doc["unitId"]|1; CFG.pollMs=doc["pollMs"]|10000; CFG.autoTest=doc["autoTest"]|0;
  CFG.cfgElevation_m = (int16_t)(int)(doc["cfgElevation_m"] | 0);
  CFG.cfgQNH_Pa      = (uint32_t)(unsigned long)(doc["cfgQNH_Pa"] | 0);
  CFG.altMode        = (uint8_t)(unsigned int)(doc["altMode"] | 0);
  CFG.autoQNH_enable = (uint8_t)(unsigned int)(doc["autoQNH_enable"] | 1);
  CFG.autoQNH_period_h = (uint8_t)(unsigned int)(doc["autoQNH_period_h"] | 8);
  CFG.autoQNH_manual_en = (uint8_t)(unsigned int)(doc["autoQNH_manual_en"] | 0);
  const char* man = doc["autoQNH_manual_icao"] | "";
  strlcpy(CFG.autoQNH_manual_icao, man, sizeof(CFG.autoQNH_manual_icao));
  CFG.mqtt_enable = (uint8_t)(unsigned int)(doc["mqtt_enable"] | CFG.mqtt_enable);
  CFG.mqtt_host = doc["mqtt_host"] | CFG.mqtt_host;
  CFG.mqtt_port = (uint16_t)(unsigned int)(doc["mqtt_port"] | CFG.mqtt_port);
  CFG.mqtt_clientId = doc["mqtt_clientId"] | CFG.mqtt_clientId;
  CFG.mqtt_username = doc["mqtt_username"] | CFG.mqtt_username;
  CFG.mqtt_password = doc["mqtt_password"] | CFG.mqtt_password;
  CFG.mqtt_baseTopic = doc["mqtt_baseTopic"] | CFG.mqtt_baseTopic;
  CFG.mqtt_ha_discovery = (uint8_t)(unsigned int)(doc["mqtt_ha_discovery"] | CFG.mqtt_ha_discovery);
  CFG.mqtt_loxone_discovery = (uint8_t)(unsigned int)(doc["mqtt_loxone_discovery"] | CFG.mqtt_loxone_discovery);
  if (CFG.altMode>3) CFG.altMode=0;
  if (CFG.autoQNH_period_h==0) CFG.autoQNH_period_h=8;

  ensureMqttDefaults();

  // mirroring do WiFiManager bufferů
  strlcpy(deviceNameBuf, CFG.deviceName.c_str(), sizeof(deviceNameBuf));
  snprintf(unitIdBuf, sizeof(unitIdBuf), "%u", CFG.unitId);
  snprintf(pollMsBuf, sizeof(pollMsBuf), "%u", CFG.pollMs);
  snprintf(autoTestBuf, sizeof(autoTestBuf), "%u", CFG.autoTest);
  snprintf(elevBuf, sizeof(elevBuf), "%d", (int)CFG.cfgElevation_m);
  snprintf(qnhBuf,  sizeof(qnhBuf),  "%lu", (unsigned long)CFG.cfgQNH_Pa);
  snprintf(altModeBuf, sizeof(altModeBuf), "%u", (unsigned)CFG.altMode);
  snprintf(autoQNHBuf, sizeof(autoQNHBuf), "%u", (unsigned)CFG.autoQNH_enable);
  snprintf(autoQNHperBuf, sizeof(autoQNHperBuf), "%u", (unsigned)CFG.autoQNH_period_h);
  snprintf(aqManualEnBuf, sizeof(aqManualEnBuf), "%u", (unsigned)CFG.autoQNH_manual_en);
  strlcpy(aqManualICAOBuf, CFG.autoQNH_manual_icao, sizeof(aqManualICAOBuf));
  return true;
}

// ----------------------------- MQTT -------------------------------------------
static bool mqttPublishNumber(const String& topic, float value, uint8_t decimals, bool retain = true){
  if (!topic.length() || !isfinite(value)) return false;
  String payload = String(value, decimals);
  payload.replace(',', '.');
  return g_mqttClient.publish(topic.c_str(), payload.c_str(), retain);
}

static void mqttPublishAvailability(bool online){
  if (!g_mqttClient.connected()) return;
  String topic = mqttAvailabilityTopic();
  if (!topic.length()) return;
  const char* payload = online ? "online" : "offline";
  if (g_mqttClient.publish(topic.c_str(), payload, true)){
    g_mqttState.availabilityOnline = online;
  }
}

static void mqttPublishHaClear(){
  if (!g_mqttClient.connected()) return;
  static const char* KEYS[] = {"temperature","humidity","pressure","lux","dew_point","abs_humidity","vpd","qnh","altitude","bmp_temperature"};
  const String deviceId = mqttDeviceId();
  for (const char* key : KEYS){
    String topic = String(F("homeassistant/sensor/")) + deviceId + '/' + key + F("/config");
    g_mqttClient.publish(topic.c_str(), "", true);
  }
}

static void mqttPublishLoxoneClear(){
  if (!g_mqttClient.connected()) return;
  String topic = String(F("loxone/")) + mqttDeviceId() + F("/config");
  g_mqttClient.publish(topic.c_str(), "", true);
}

static void mqttPublishHaDiscovery(){
  if (!g_mqttClient.connected() || !CFG.mqtt_ha_discovery) return;
  struct SensorDesc { const char* key; const char* label; const char* unit; const char* deviceClass; const char* stateClass; uint8_t decimals; };
  static const SensorDesc SENSORS[] = {
    {"temperature",      "Teplota",          "°C",  "temperature", "measurement", 2},
    {"humidity",         "Relativní vlhkost","%",   "humidity",    "measurement", 2},
    {"pressure",         "Tlak",             "hPa", "pressure",    "measurement", 1},
    {"lux",              "Osvit",            "lx",  "illuminance", "measurement", 0},
    {"dew_point",        "Rosný bod",        "°C",  "temperature", "measurement", 2},
    {"abs_humidity",     "Abs. vlhkost",     "g/m³",nullptr,        "measurement", 2},
    {"vpd",              "VPD",              "Pa",  nullptr,        "measurement", 0},
    {"qnh",              "QNH",              "hPa", "pressure",    "measurement", 1},
    {"altitude",         "Nadmořská výška",  "m",   "distance",    "measurement", 2},
    {"bmp_temperature",  "Teplota BMP",      "°C",  "temperature", "measurement", 2},
  };
  const String deviceId = mqttDeviceId();
  String availability = mqttAvailabilityTopic();
  String deviceName = CFG.deviceName.length() ? CFG.deviceName : String(F("ESP Meteostanice"));
  char fwBuf[16]; snprintf(fwBuf, sizeof(fwBuf), "%u.%u", FW_MAJOR, FW_MINOR);

  for (const SensorDesc& s : SENSORS){
    String topic = String(F("homeassistant/sensor/")) + deviceId + '/' + s.key + F("/config");
    StaticJsonDocument<512> doc;
    String name = deviceName + String(F(" ")) + String(s.label);
    doc["name"] = name;
    doc["uniq_id"] = deviceId + '_' + s.key;
    doc["stat_t"] = mqttStateTopic(s.key);
    if (s.unit) doc["unit_of_meas"] = s.unit;
    if (s.deviceClass) doc["dev_cla"] = s.deviceClass;
    if (s.stateClass) doc["stat_cla"] = s.stateClass;
    doc["avty_t"] = availability;
    doc["pl_avail"] = "online";
    doc["pl_not_avail"] = "offline";
    doc["suggested_display_precision"] = s.decimals;
    JsonObject device = doc.createNestedObject("device");
    JsonArray ids = device.createNestedArray("identifiers");
    ids.add(deviceId);
    device["name"] = deviceName;
    device["manufacturer"] = "ESP Meteostanice";
    device["model"] = "ESP Meteostanice";
    device["sw_version"] = fwBuf;

    String payload;
    serializeJson(doc, payload);
    g_mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }
}

static void mqttPublishLoxoneDiscovery(){
  if (!g_mqttClient.connected() || !CFG.mqtt_loxone_discovery) return;
  StaticJsonDocument<512> doc;
  doc["device"] = CFG.deviceName.length() ? CFG.deviceName : String(F("ESP Meteostanice"));
  doc["base_topic"] = mqttGetBaseTopic();
  JsonObject topics = doc.createNestedObject("topics");
  topics["temperature"] = mqttStateTopic("temperature");
  topics["humidity"] = mqttStateTopic("humidity");
  topics["pressure_hpa"] = mqttStateTopic("pressure");
  topics["lux"] = mqttStateTopic("lux");
  topics["dew_point"] = mqttStateTopic("dew_point");
  topics["abs_humidity"] = mqttStateTopic("abs_humidity");
  topics["vpd_pa"] = mqttStateTopic("vpd");
  topics["qnh_hpa"] = mqttStateTopic("qnh");
  topics["altitude_m"] = mqttStateTopic("altitude");
  topics["bmp_temperature"] = mqttStateTopic("bmp_temperature");

  String payload;
  serializeJson(doc, payload);
  String topic = String(F("loxone/")) + mqttDeviceId() + F("/config");
  g_mqttClient.publish(topic.c_str(), payload.c_str(), true);
}

static void mqttPublishDiscovery(){
  if (!g_mqttClient.connected()) return;
  if (!CFG.mqtt_ha_discovery && !CFG.mqtt_loxone_discovery){
    g_mqttState.discoverySent = true;
    return;
  }
  if (CFG.mqtt_ha_discovery) mqttPublishHaDiscovery();
  else if (g_mqttState.needHaClear) { mqttPublishHaClear(); g_mqttState.needHaClear = false; }
  if (CFG.mqtt_loxone_discovery) mqttPublishLoxoneDiscovery();
  else if (g_mqttState.needLoxClear) { mqttPublishLoxoneClear(); g_mqttState.needLoxClear = false; }
  g_mqttState.discoverySent = true;
}

static void mqttPublishState(bool force){
  if (!CFG.mqtt_enable || !g_mqttClient.connected()) return;
  uint32_t now = millis();
  if (!force && (now - g_mqttState.lastStatePublish) < 200) return;
  g_mqttState.lastStatePublish = now;

  if (isfinite(g_htu_t)) mqttPublishNumber(mqttStateTopic("temperature"), g_htu_t, 2);
  if (isfinite(g_htu_h)) mqttPublishNumber(mqttStateTopic("humidity"), g_htu_h, 2);
  if (g_bmp_p > 0) mqttPublishNumber(mqttStateTopic("pressure"), g_bmp_p / 100.0f, 1);
  if (isfinite(g_bh_lux)) mqttPublishNumber(mqttStateTopic("lux"), g_bh_lux, 1);
  if (!isnan(g_bmp_t)) mqttPublishNumber(mqttStateTopic("bmp_temperature"), g_bmp_t, 2);

  float Td = NAN;
  float AH = NAN;
  float VPD = NAN;
  if (!isnan(g_htu_t) && !isnan(g_htu_h)){
    Td = dewPointC(g_htu_t, g_htu_h);
    AH = absHumidity_gm3(g_htu_t, g_htu_h);
    VPD = es_Pa(g_htu_t) * (1.0f - clampf(g_htu_h,0,100)/100.0f);
  }
  if (isfinite(Td)) mqttPublishNumber(mqttStateTopic("dew_point"), Td, 2);
  if (isfinite(AH)) mqttPublishNumber(mqttStateTopic("abs_humidity"), AH, 2);
  if (isfinite(VPD)) mqttPublishNumber(mqttStateTopic("vpd"), VPD, 0);

  uint32_t qnh = ((uint32_t)mb.Hreg(32) << 16) | (uint32_t)mb.Hreg(33);
  if (qnh > 0) mqttPublishNumber(mqttStateTopic("qnh"), qnh / 100.0f, 1);

  int32_t alt_cm = (int32_t)(((uint32_t)mb.Hreg(34) << 16) | (uint32_t)mb.Hreg(35));
  mqttPublishNumber(mqttStateTopic("altitude"), alt_cm / 100.0f, 2);
}

static void mqttOnConfigChanged(){
  ensureMqttDefaults();
  CFG.mqtt_host.trim();
  CFG.mqtt_username.trim();
  CFG.mqtt_baseTopic = sanitizeTopicBase(CFG.mqtt_baseTopic);
  CFG.mqtt_clientId = sanitizeClientId(CFG.mqtt_clientId);
  bool enabled = CFG.mqtt_enable != 0;

  if (!enabled){
    if (g_mqttClient.connected()){
      mqttPublishHaClear();
      mqttPublishLoxoneClear();
      mqttPublishAvailability(false);
      g_mqttClient.disconnect();
    }
    g_mqttState.status = F("MQTT vypnuto");
    g_mqttState.availabilityOnline = false;
  }
  else {
    g_mqttState.status = CFG.mqtt_host.length() ? String(F("MQTT připraveno")) : String(F("MQTT: chybí host"));
  }

  if (g_mqttPrevHaDiscovery && !CFG.mqtt_ha_discovery) g_mqttState.needHaClear = true;
  if (g_mqttPrevLoxDiscovery && !CFG.mqtt_loxone_discovery) g_mqttState.needLoxClear = true;

  g_mqttState.configDirty = true;
  g_mqttState.discoverySent = false;

  g_mqttPrevEnable = enabled ? 1 : 0;
  g_mqttPrevHaDiscovery = CFG.mqtt_ha_discovery ? 1 : 0;
  g_mqttPrevLoxDiscovery = CFG.mqtt_loxone_discovery ? 1 : 0;
}

static void mqttLoop(){
  if (!CFG.mqtt_enable){
    return;
  }

  if (!CFG.mqtt_host.length()){
    g_mqttState.status = F("MQTT: chybí host");
    return;
  }

  if (WiFi.status() != WL_CONNECTED){
    if (g_mqttClient.connected()){
      mqttPublishAvailability(false);
      g_mqttClient.disconnect();
      g_mqttState.availabilityOnline = false;
    }
    g_mqttState.status = F("MQTT: čekám na Wi-Fi");
    return;
  }

  if (g_mqttState.configDirty){
    g_mqttClient.setServer(CFG.mqtt_host.c_str(), CFG.mqtt_port ? CFG.mqtt_port : 1883);
    g_mqttState.configDirty = false;
  }

  if (!g_mqttClient.connected()){
    g_mqttState.availabilityOnline = false;
    if (millis() - g_mqttState.lastConnectAttempt < 5000) return;
    g_mqttState.lastConnectAttempt = millis();
    String clientId = mqttGetClientId();
    const char* user = CFG.mqtt_username.length() ? CFG.mqtt_username.c_str() : nullptr;
    const char* pass = CFG.mqtt_password.length() ? CFG.mqtt_password.c_str() : nullptr;
    bool ok;
    if (user){
      ok = g_mqttClient.connect(clientId.c_str(), user, pass ? pass : "");
    } else {
      ok = g_mqttClient.connect(clientId.c_str());
    }
    if (ok){
      g_mqttState.status = String(F("MQTT připojeno k ")) + CFG.mqtt_host + ':' + String(CFG.mqtt_port);
      mqttPublishAvailability(true);
      if (g_mqttState.needHaClear){ mqttPublishHaClear(); g_mqttState.needHaClear = false; }
      if (g_mqttState.needLoxClear){ mqttPublishLoxoneClear(); g_mqttState.needLoxClear = false; }
      mqttPublishDiscovery();
      mqttPublishState(true);
    } else {
      g_mqttState.lastClientState = g_mqttClient.state();
      g_mqttState.status = String(F("MQTT chyba (state=")) + String(g_mqttState.lastClientState) + ')';
    }
    return;
  }

  g_mqttClient.loop();
}
static bool icaoEquals(const char* a, const char* b){
  if (!a || !b) return false;
  for (uint8_t i=0;i<4;i++){
    char ca = toupper((unsigned char)a[i]);
    char cb = toupper((unsigned char)b[i]);
    if (ca != cb) return false;
    if (ca == '\0' || cb == '\0') break;
  }
  return true;
}

static bool g_timeSyncInit = false;

static void initTimeSync(){
  if (g_timeSyncInit) return;
  setenv("TZ","CET-1CEST,M3.5.0/2,M10.5.0/3",1);
  tzset();
  configTime(0, 0, "pool.ntp.org", "time.google.com", "time.cloudflare.com");
  g_timeSyncInit = true;
}

static bool isTimeValid(){
  time_t now = time(nullptr);
  return now > 1609459200; // 1.1.2021 jako hranice
}

static inline float sinDeg(float deg){ return sinf(deg * DEG_TO_RAD); }
static inline float cosDeg(float deg){ return cosf(deg * DEG_TO_RAD); }
static inline float tanDeg(float deg){ return tanf(deg * DEG_TO_RAD); }
static inline float acosDeg(float x){
  if (x < -1.0f) x = -1.0f;
  if (x >  1.0f) x =  1.0f;
  return RAD_TO_DEG * acosf(x);
}

static bool computeSunEvent(time_t nowLocal, float lat, float lon, bool sunrise, time_t &eventOut){
  struct tm lt;
  if (!localtime_r(&nowLocal, &lt)) return false;
  int N = lt.tm_yday + 1;
  float lngHour = lon / 15.0f;
  float approx = sunrise ? 6.0f : 18.0f;
  float t = N + ((approx - lngHour) / 24.0f);
  float M = 0.9856f * t - 3.289f;
  float L = M + 1.916f * sinDeg(M) + 0.020f * sinDeg(2.0f * M) + 282.634f;
  L = fmodf(L, 360.0f);
  if (L < 0) L += 360.0f;

  float RA = RAD_TO_DEG * atanf(0.91764f * tanDeg(L));
  RA = fmodf(RA, 360.0f);
  if (RA < 0) RA += 360.0f;

  float Lquadrant = floorf(L / 90.0f) * 90.0f;
  float RAquadrant = floorf(RA / 90.0f) * 90.0f;
  RA = (RA + (Lquadrant - RAquadrant)) / 15.0f;

  float sinDec = 0.39782f * sinDeg(L);
  float cosDec = cosf(asinf(sinDec));
  float cosH = (cosDeg(90.833f) - sinDec * sinDeg(lat)) / (cosDec * cosDeg(lat));
  if (cosH > 1.0f) return false; // nikdy nevychází
  if (cosH < -1.0f) return false; // nikdy nezapadá

  float H = sunrise ? (360.0f - acosDeg(cosH)) : acosDeg(cosH);
  H /= 15.0f;

  float T = H + RA - 0.06571f * t - 6.622f;
  float UT = fmodf(T - lngHour, 24.0f);
  if (UT < 0) UT += 24.0f;

  struct tm gmt;
  gmtime_r(&nowLocal, &gmt);
  time_t gmAsLocal = mktime(&gmt);
  float tzOffsetHours = (float)difftime(nowLocal, gmAsLocal) / 3600.0f;

  float localHours = UT + tzOffsetHours;
  while (localHours < 0) localHours += 24.0f;
  while (localHours >= 24.0f) localHours -= 24.0f;

  struct tm dayLocal = lt;
  dayLocal.tm_hour = 0; dayLocal.tm_min = 0; dayLocal.tm_sec = 0;
  time_t midnight = mktime(&dayLocal);

  int hour = (int)floorf(localHours);
  float minF = (localHours - hour) * 60.0f;
  int minute = (int)floorf(minF);
  int second = (int)lroundf((minF - minute) * 60.0f);

  dayLocal.tm_hour = hour;
  dayLocal.tm_min = minute;
  dayLocal.tm_sec = second;
  eventOut = mktime(&dayLocal);
  return true;
}

static void updateSunTimesIfNeeded(){
  if (!g_locationValid) return;
  if (!isTimeValid()) return;
  uint32_t nowMs = millis();
  if (g_sunTimesValid && (nowMs - g_lastSunCalcMs) < 60000UL) return;

  time_t nowLocal = time(nullptr);
  time_t sunrise=0, sunset=0;
  bool riseOk = computeSunEvent(nowLocal, g_locationLat, g_locationLon, true, sunrise);
  bool setOk  = computeSunEvent(nowLocal, g_locationLat, g_locationLon, false, sunset);
  if (riseOk && setOk){
    g_sunrise_ts = sunrise;
    g_sunset_ts = sunset;
    g_sunTimesValid = true;
  } else {
    g_sunTimesValid = false;
  }
  g_lastSunCalcMs = nowMs;
}

static bool lookupAirportByICAO(const char* icao, float &lat, float &lon, String &name);
static void updateLocationFromConfig();
static void initTimeSync();
static bool computeSunEvent(time_t nowLocal, float lat, float lon, bool sunrise, time_t &eventOut);
static void updateSunTimesIfNeeded();
static void updateBioForecast();

bool g_httpStarted = false;

static void httpSendOK(const String& body){
  server.send(200, "text/html; charset=utf-8", body);
}
static String htmlHeader(const char* title){
  String s = F("<!DOCTYPE html><meta charset='utf-8'><title>");
  s += title; s += F("</title><style>body{font:14px system-ui;margin:20px;}code{background:#eee;padding:2px 4px;border-radius:4px}</style>");
  return s;
}
static String yesno(bool b){ return b?F("OK"):F("NOK"); }

static void handleRoot(){
  uint32_t qnh = ((uint32_t)mb.Hreg(32)<<16) | (uint32_t)mb.Hreg(33);
  int32_t  alt_cm = (int32_t)(((uint32_t)mb.Hreg(34)<<16) | (uint32_t)mb.Hreg(35));
  char icao[5]; readICAOFromRegs(47,48,icao);

  String s = htmlHeader("ESP Meteo – Status");
  s += F("<h1>ESP Meteo – Status</h1><ul>");
  s += F("<li>Device: <b>"); s += CFG.deviceName; s += F("</b></li>");
  s += F("<li>Wi-Fi: "); s += (WiFi.status()==WL_CONNECTED?F("<b>OK</b>"):F("<b>NOK</b>"));
  s += F(", IP: <code>"); s += (WiFi.status()==WL_CONNECTED?WiFi.localIP().toString():"0.0.0.0"); s += F("</code>");
  s += F(", RSSI: "); s += String(WiFi.RSSI()); s += F(" dBm</li>");
  s += F("<li>Senzory: HTU="); s += yesno(htu_ok); s += F(", BH1750="); s += yesno(bh_ok); s += F(", BMP180="); s += yesno(bmp_ok); s += F("</li>");
  s += F("<li>T/RH (HTU): "); if(!isnan(g_htu_t)) s+=String(g_htu_t,2); else s+=F("NaN");
  s += F(" °C / "); if(!isnan(g_htu_h)) s+=String(g_htu_h,2); else s+=F("NaN"); s+=F(" %</li>");
  s += F("<li>P/T (BMP): "); if(g_bmp_p) s+=String(g_bmp_p); else s+=F("0"); s+=F(" Pa / ");
  if(!isnan(g_bmp_t)) s+=String(g_bmp_t,2); else s+=F("NaN"); s+=F(" °C</li>");
  s += F("<li>Lux (BH1750): "); if(!isnan(g_bh_lux)) s+=String(g_bh_lux,1); else s+=F("NaN"); s+=F(" lx</li>");
  s += F("<li>QNH: "); s += String(qnh); s += F(" Pa, ALT: "); s += String((long)alt_cm/100.0f,2);
  s += F(" m, ICAO: '"); s += String(icao); s += F("'</li>");
  s += F("<li>Modbus TCP: port 502</li>");
  s += F("</ul><p>Akce: ");
  s += F("<a href='/scan'>I2C Scan</a> · ");
  s += F("<a href='/selftest'>Self-test</a> · ");
  s += F("<a href='/reboot' onclick='return confirm(\"Reboot?\")'>Reboot</a>");
  s += F("</p><p>WiFiManager portál spustíš tlačítkem FLASH (GPIO0) nebo přes Serial příkaz <code>portal</code>.</p>");
  httpSendOK(s);
}

static void handleScan(){ i2cScan(); httpSendOK(htmlHeader("I2C scan") + F("<h1>I2C scan spuštěn</h1><p><a href='/'>Zpět</a></p>")); }
static void handleSelfTest(){ sensorSelfTest(); httpSendOK(htmlHeader("Self-test") + F("<h1>Self-test spuštěn</h1><p><a href='/'>Zpět</a></p>")); }
static void handleReboot(){ httpSendOK(htmlHeader("Reboot") + F("<h1>Reboot…</h1>")); delay(150); ESP.restart(); }

static void httpSetupRoutes(){
  server.on("/", handleRoot);
  server.on("/scan", handleScan);
  server.on("/selftest", handleSelfTest);
  server.on("/reboot", handleReboot);
  server.onNotFound([](){ server.send(404, "text/plain", "Not found"); });
}

static void httpTryStart(){
  if (!g_httpStarted && WiFi.status()==WL_CONNECTED){
    httpSetupRoutes();
    server.begin();
    g_httpStarted = true;
    DLOG("[HTTP] WebServer started on http://%s/\r\n", WiFi.localIP().toString().c_str());
  }
}

// --- použitelné pro WM i CLI ---
void applyParamsFromWM(){
  CFG.deviceName=String(p_deviceName.getValue());
  CFG.unitId=(uint16_t)strtoul(p_unitId.getValue(),nullptr,10); CFG.unitId=(CFG.unitId>=1 && CFG.unitId<=247)?CFG.unitId:1;
  CFG.pollMs=(uint32_t)strtoul(p_pollMs.getValue(),nullptr,10); if(CFG.pollMs<200) CFG.pollMs=200;
  CFG.autoTest=(uint8_t)strtoul(p_autoTest.getValue(),nullptr,10); CFG.autoTest=(CFG.autoTest?1:0);
  CFG.cfgElevation_m = (int16_t) strtol(p_elev.getValue(), nullptr, 10);
  CFG.cfgQNH_Pa      = (uint32_t) strtoul(p_qnh.getValue(),  nullptr, 10);
  CFG.altMode        = (uint8_t)  strtoul(p_altm.getValue(), nullptr, 10); if (CFG.altMode>3) CFG.altMode=0;
  CFG.autoQNH_enable = (uint8_t)  strtoul(p_aqen.getValue(), nullptr, 10); CFG.autoQNH_enable=(CFG.autoQNH_enable?1:0);
  CFG.autoQNH_period_h = (uint8_t)strtoul(p_aqpr.getValue(), nullptr, 10); if (!CFG.autoQNH_period_h) CFG.autoQNH_period_h=8;
  CFG.autoQNH_manual_en = (uint8_t)strtoul(p_aqme.getValue(), nullptr, 10); CFG.autoQNH_manual_en=(CFG.autoQNH_manual_en?1:0);
  String icao = String(p_aqmc.getValue()); icao.trim(); icao.toUpperCase();
  if (icao.length()==4){ strlcpy(CFG.autoQNH_manual_icao, icao.c_str(), sizeof(CFG.autoQNH_manual_icao)); }
  else { CFG.autoQNH_manual_icao[0]='\0'; if (CFG.autoQNH_manual_en) CFG.autoQNH_manual_en=0; }
  updateLocationFromConfig();
  ensureMqttDefaults();
  mqttOnConfigChanged();
}

// ----------------------------- Derived calc (Td, AH, VPD, Tv) ----------------
static inline float clampf(float v, float lo, float hi){ return v<lo?lo:(v>hi?hi:v); }
static inline float es_Pa(float T){ return 611.2f * expf(17.62f*T/(243.12f+T)); } // Pa
static inline float dewPointC(float T, float RH){
  if (isnan(T) || isnan(RH) || RH<=0) return NAN;
  RH = clampf(RH, 0.1f, 100.0f);
  const float a=17.62f, b=243.12f;
  float gamma = logf(RH/100.0f) + (a*T)/(b+T);
  return (b*gamma)/(a - gamma);
}
static inline float absHumidity_gm3(float T, float RH){
  if (isnan(T) || isnan(RH)) return NAN;
  float e = (RH/100.0f) * es_Pa(T);
  return 2.1674f * e / (T + 273.15f); // g/m3
}
static inline float virtTemp_K_from_TRH_P(float T_C, float RH, uint32_t p_Pa){
  const float eps=0.622f;
  if (isnan(T_C) || isnan(RH) || p_Pa==0) return 288.15f; // fallback 15 °C
  float T_K = T_C + 273.15f;
  RH = clampf(RH, 0.0f, 100.0f);
  float e  = (RH/100.0f) * es_Pa(T_C); // Pa
  float q  = eps*e / (p_Pa - (1.0f - eps)*e);
  return T_K * (1.0f + 0.61f*q);
}

static void updateBioForecast(){
  if (isnan(g_htu_t) || isnan(g_htu_h) || g_bmp_p == 0){
    g_bioIndex = -1;
    g_bioScore = 0.0f;
    g_bioLabel = "Bez dat";
    g_bioComment = "Čekám na stabilní měření.";
    return;
  }

  float temp = g_htu_t;
  float rh = clampf(g_htu_h, 0.0f, 100.0f);
  float pressure_hpa = g_bmp_p / 100.0f;
  float score = 0.0f;

  if (temp < 5.0f || temp > 26.0f) score += 0.6f;
  if (temp < -5.0f || temp > 30.0f) score += 0.8f;
  if (temp < -12.0f || temp > 35.0f) score += 0.9f;

  if (rh < 35.0f || rh > 75.0f) score += 0.5f;
  if (rh < 25.0f || rh > 85.0f) score += 0.7f;
  if (rh < 15.0f || rh > 95.0f) score += 0.9f;

  if (pressure_hpa < 995.0f || pressure_hpa > 1035.0f) score += 0.4f;
  if (pressure_hpa < 985.0f || pressure_hpa > 1040.0f) score += 0.4f;

  float trend_hpa = g_pressureTrendPaPerHour / 100.0f;
  float trendAbs = fabsf(trend_hpa);
  if (trendAbs > 1.5f) score += 0.4f;
  if (trendAbs > 3.0f) score += 0.4f;

  float vpd_kpa = es_Pa(temp) * (1.0f - rh/100.0f) / 1000.0f;
  if (vpd_kpa > 2.0f) score += 0.3f;
  if (vpd_kpa > 3.0f) score += 0.3f;

  g_bioScore = score;
  int idx;
  if (score < 0.8f) idx = 0;
  else if (score < 1.6f) idx = 1;
  else if (score < 2.5f) idx = 2;
  else idx = 3;
  g_bioIndex = idx;

  switch (idx) {
    case 0:
      g_bioLabel = "Nízká";
      g_bioComment = "Pohodové podmínky pro většinu populace.";
      break;
    case 1:
      g_bioLabel = "Střední";
      g_bioComment = "Mírná zátěž – citlivější osoby mohou pociťovat únavu.";
      break;
    case 2:
      g_bioLabel = "Vysoká";
      g_bioComment = "Zvýšená zátěž – doporučen odpočinek a pitný režim.";
      break;
    default:
      g_bioLabel = "Extrémní";
      g_bioComment = "Výrazná zátěž – omezte zátěž a sledujte zdravotní stav.";
      break;
  }

  if (trendAbs > 1.5f){
    g_bioComment += String(" Tlak se mění ") + (trend_hpa >= 0 ? "vzestupně" : "sestupně") +
                    String(" (") + String(trend_hpa, 1) + " hPa/h).";
  }
}

// Hypsometrie: ALT z QNH (m)
static inline float altitude_from_QNH_m(uint32_t p_Pa, float T_C, float RH, uint32_t qnh_Pa){
  if (!p_Pa || !qnh_Pa) return 0.0f;
  const float Rd=287.05f, g=9.80665f;
  float Tv = virtTemp_K_from_TRH_P(T_C, RH, p_Pa);
  return (Rd*Tv/g) * logf((float)qnh_Pa/(float)p_Pa);
}
// Hypsometrie: QNH z Elevation (Pa)
static inline uint32_t qnh_from_elev_Pa(uint32_t p_Pa, float T_C, float RH, float elev_m){
  if (!p_Pa) return 0;
  const float Rd=287.05f, g=9.80665f;
  float Tv = virtTemp_K_from_TRH_P(T_C, RH, p_Pa);
  float p0 = (float)p_Pa * expf(g*elev_m/(Rd*Tv));
  if (p0 < 0) p0 = 0; if (p0 > 200000.0f) p0 = 200000.0f;
  return (uint32_t)lroundf(p0);
}

// ----------------------------- Wi-Fi & WiFiManager ----------------------------
void setupWiFi(){
  WiFi.persistent(true);
  WiFi.setAutoReconnect(true);

#if defined(ARDUINO_ARCH_ESP32)
  #if WIFI_ENABLE_LIGHT_SLEEP
    WiFi.setSleep(true);
  #else
    WiFi.setSleep(false);
  #endif
  // ~17 dBm
  WiFi.setTxPower(WIFI_POWER_17dBm);
#else
  #if WIFI_ENABLE_LIGHT_SLEEP
    WiFi.setSleepMode(WIFI_LIGHT_SLEEP);
  #endif
  WiFi.setOutputPower(WIFI_TX_POWER_DBM);
#endif

  // WiFiManager – beze změny
  wm.setSaveParamsCallback(onSaveParams);
  wm.addParameter(&p_deviceName); wm.addParameter(&p_unitId); wm.addParameter(&p_pollMs); wm.addParameter(&p_autoTest);
  wm.addParameter(&p_elev); wm.addParameter(&p_qnh); wm.addParameter(&p_altm);
  wm.addParameter(&p_aqen); wm.addParameter(&p_aqpr);
  wm.addParameter(&p_aqme); wm.addParameter(&p_aqmc);

  wm.setConfigPortalBlocking(false);
  wm.setBreakAfterConfig(true);
  wm.setCaptivePortalEnable(true);
  wm.setConfigPortalTimeout(300);
  wm.setTimeout(180);

  char apName[40]; snprintf(apName, sizeof(apName), "%s-SETUP", (CFG.deviceName.length()?CFG.deviceName.c_str():"ESP-Meteo"));
  if(!wm.autoConnect(apName)){
    // portál lze kdykoliv vyvolat tlačítkem
  }
}

// ----------------------------- I2C re-init (oprava bez Wire.end) --------------
void reinitI2C(){
  // ESP8266: TwoWire nemá end(), prosté znovu-inicializování sběrnice
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(400000);

  // re-init senzorů na sběrnici
  bh_ok  = bh1750.begin(BH1750::CONTINUOUS_HIGH_RES_MODE);
  bmp_ok = bmp.begin();

  DLOG("[I2C] Reinit done. BH:%s BMP:%s\r\n", bh_ok?"OK":"NOK", bmp_ok?"OK":"NOK");
}

// ----------------------------- Senzory ----------------------------------------
// --- řízené napájení senzorů (ESP32-C3) ---
//#define SENSOR_PWR_PIN 7

static void sensorsPowerInit(){
  //pinMode(SENSOR_PWR_PIN, OUTPUT);
  //digitalWrite(SENSOR_PWR_PIN, HIGH); // vypnuto
}
static void sensorsPowerOn(){
  //digitalWrite(SENSOR_PWR_PIN, LOW);  // zapnout (aktivní LOW)
  //delay(10);
}
static void sensorsPowerOff(){
  //digitalWrite(SENSOR_PWR_PIN, HIGH); // vypnout
}

void setupSensors(){
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);  // SDA=GPIO4(D2), SCL=GPIO5(D1) na ESP-Witty
  Wire.setClock(400000);

  htu.begin();                  // SparkFun HTU21D: begin() je void
  { float t=htu.readTemperature(), h=htu.readHumidity();
    htu_ok = (t>-50 && t<150 && h>=0 && h<=100);
  }
  bh_ok  = bh1750.begin(BH1750::CONTINUOUS_HIGH_RES_MODE);
  bmp_ok = bmp.begin();

  DLOG("[INIT] HTU21D:%s, BH1750:%s, BMP180:%s\r\n", htu_ok?"OK":"NOK", bh_ok?"OK":"NOK", bmp_ok?"OK":"NOK");
}

// ----------------------------- Modbus (emelianov) -----------------------------
void setupModbus(){
  mb.server();                       // naslouchá na TCP portu 502
  mb.addHreg(0,0,HOLD_COUNT);

  // Základ
  mb.Hreg(0,0); mb.Hreg(1,CFG.unitId); mb.Hreg(2,FW_MAJOR); mb.Hreg(3,FW_MINOR);

  // Nulování vybraných registrů
  uint16_t initRegs[]={10,11,12,13,14,15,16,20,21,32,33,34,35,40,41,42,90,91,92,93,94,95,112,113,114,100,101,102,
                       43,44,45,46,47,48,49,50,51,52,53,70,71,72,73,74,75,76,77};
  for(uint16_t r:initRegs) mb.Hreg(r,0);

  // Info o pinech I2C
  mb.Hreg(94,I2C_SDA_PIN); mb.Hreg(95,I2C_SCL_PIN);
  for(uint16_t i=0;i<16;++i) mb.Hreg(96+i,0);

  // Konfigurační položky (zrcadlení)
  mb.Hreg(36, (uint16_t)CFG.cfgElevation_m);       // S16
  writeU32ToRegs(37, CFG.cfgQNH_Pa);               // U32
  mb.Hreg(39, (uint16_t)CFG.altMode);              // U16
  mb.Hreg(43, (uint16_t)CFG.autoQNH_enable);
  mb.Hreg(44, (uint16_t)CFG.autoQNH_period_h);
  mb.Hreg(51, (uint16_t)CFG.autoQNH_manual_en);
  if (strlen(CFG.autoQNH_manual_icao)==4) writeICAOToRegs(52,53, CFG.autoQNH_manual_icao);

  // Modbus echo-test init
  mb.Hreg(72, FW_MAJOR);
  mb.Hreg(73, FW_MINOR);
}

// ----------------------------- Stav senzorů/utility ---------------------------
uint16_t computeStatus(){ uint16_t st=0; if(htu_ok) st|=(1<<0); if(bh_ok) st|=(1<<1); if(bmp_ok) st|=(1<<2); return st; }

// ----------------------------- I2C scan ---------------------------------------
void i2cScan(){
  DLOG("[I2C] Scan start...\r\n"); g_i2cScanActive=true;
  uint32_t t0=millis(); uint16_t found=0,lastErr=0;
  for(uint16_t i=0;i<16;++i) mb.Hreg(96+i,0);
  uint8_t slot=0;
  for(uint8_t a=0x08;a<=0x77;a++){
    Wire.beginTransmission(a); uint8_t e=Wire.endTransmission();
    if(e==0){ if(slot<16){ mb.Hreg(96+slot,(uint16_t)a); ++slot; } ++found; DLOG("  - 0x%02X\r\n",a); }
    else lastErr=e;
    yield();
  }
  uint32_t dt=millis()-t0; uint16_t cnt=mb.Hreg(90);
  mb.Hreg(90,cnt+1); mb.Hreg(91,found); mb.Hreg(92,(uint16_t)(dt>0xFFFF?0xFFFF:dt)); mb.Hreg(93,lastErr); mb.Hreg(101,0);
  DLOG("[I2C] Done: found=%u, dt=%lums, lastErr=%u\r\n",found,dt,lastErr);
  g_i2cScanActive=false;

  // Volitelně: při chybách reinit sběrnice
  if (lastErr != 0) reinitI2C();
}

// ----------------------------- Self-test --------------------------------------
void sensorSelfTest(){
  DLOG("[TEST] Sensor self-test...\r\n"); g_selfTestActive=true;
  uint16_t htuStat=2,bhStat=2,bmpStat=2;

  { float t=htu.readTemperature(), h=htu.readHumidity();
    if(!isnan(t)&&!isnan(h)) htuStat=((t>-50 && t<150 && h>=0 && h<=100)?0:1);
  }
  { float lux=bh1750.readLightLevel(); if(!isnan(lux)) bhStat=((lux>=0 && lux<=200000)?0:1); }
  { float t=bmp.readTemperature(); int32_t p=bmp.readPressure();
    bool tOk=(!isnan(t) && t>-50 && t<150), pOk=(p>15000 && p<120000);
    bmpStat=(tOk && pOk)?0:((!tOk && !pOk)?2:1);
  }

  mb.Hreg(112,htuStat); mb.Hreg(113,bhStat); mb.Hreg(114,bmpStat);
  htu_ok=(htuStat==0); bh_ok=(bhStat==0); bmp_ok=(bmpStat==0);
  mb.Hreg(0,computeStatus()); mb.Hreg(102,0);
  DLOG("[TEST] HTU:%u BH:%u BMP:%u\r\n", htuStat,bhStat,bmpStat);
  g_selfTestActive=false;

  // Pokud je některý kriticky špatně, zkus reinit sběrnice
  if (htuStat==2 || bhStat==2 || bmpStat==2) reinitI2C();
}

// ----------------------------- Měření + derivace ------------------------------
void pollAndPublishSensors(){
  // HTU21D
  if(htu_ok){
    float t=htu.readTemperature(), h=htu.readHumidity();
    if(t>-50 && t<150){ g_htu_t=t; mb.Hreg(10,(int16_t)lroundf(t*100.0f)); }
    if(h>=0 && h<=100){ g_htu_h=h; mb.Hreg(11,(uint16_t)lroundf(h*100.0f)); }
  }
  // BMP180
  if(bmp_ok){
    float t=bmp.readTemperature(); if(t>-50 && t<150){ g_bmp_t=t; mb.Hreg(12,(int16_t)lroundf(t*100.0f)); }
    uint32_t p=(uint32_t)bmp.readPressure(); g_bmp_p=p; writeU32ToRegs(13,p);
  }
  // BH1750
  if(bh_ok){
    float lux=bh1750.readLightLevel(); if(lux<0) lux=0; g_bh_lux=lux;
    uint32_t lx=(lux>4294967040.0f)?4294967040u:(uint32_t)lroundf(lux); writeU32ToRegs(15,lx);
  }

  uint32_t nowMs = millis();
  if (g_bmp_p > 0){
    if (g_lastPressurePa != 0 && g_lastPressureMs != 0){
      float dt_h = (nowMs - g_lastPressureMs) / 3600000.0f;
      if (dt_h >= 0.01f){
        g_pressureTrendPaPerHour = ((int32_t)g_bmp_p - (int32_t)g_lastPressurePa) / dt_h;
      }
    }
    g_lastPressurePa = g_bmp_p;
    g_lastPressureMs = nowMs;
  }

  updateBioForecast();

  // Derived: Td, AH, VPD
  if (!isnan(g_htu_t) && !isnan(g_htu_h)){
    float Td  = dewPointC(g_htu_t, g_htu_h);
    float AH  = absHumidity_gm3(g_htu_t, g_htu_h);
    float VPD = es_Pa(g_htu_t) * (1.0f - clampf(g_htu_h,0,100)/100.0f);
    mb.Hreg(40, (int16_t)lroundf(Td * 100.0f));                       // cdegC
    mb.Hreg(41, (uint16_t)(AH < 0 ? 0 : lroundf(AH * 100.0f)));        // g/m3 *100
    mb.Hreg(42, (uint16_t)(VPD < 0 ? 0 : (VPD > 65535 ? 65535 : lroundf(VPD)))); // Pa
  }

  // Altitude/QNH (hypsometrie s T_v)
  uint8_t mode = (uint8_t)mb.Hreg(39); if (mode>3) mode=0;
  uint32_t qnh_cfg = ((uint32_t)mb.Hreg(37) << 16) | (uint32_t)mb.Hreg(38);
  int16_t elev_m   = (int16_t) mb.Hreg(36);
  const uint32_t QNH_ISA = 101325;

  uint32_t qnh_out = 0;
  int32_t  alt_cm  = 0;

  if (bmp_ok && g_bmp_p>0) {
    switch (mode) {
      case 1: // QNH only
        if (qnh_cfg>0) { qnh_out=qnh_cfg; alt_cm=(int32_t)lroundf( altitude_from_QNH_m(g_bmp_p, g_htu_t, g_htu_h, qnh_out) * 100.0f ); }
        break;
      case 2: // ISA only
        qnh_out = QNH_ISA;
        alt_cm  = (int32_t)lroundf( altitude_from_QNH_m(g_bmp_p, g_htu_t, g_htu_h, qnh_out) * 100.0f );
        break;
      case 3: // ELEV only → QNH z elevace, ALT=0
        if (elev_m!=0) { qnh_out=qnh_from_elev_Pa(g_bmp_p, g_htu_t, g_htu_h, (float)elev_m); }
        break;
      case 0: default: // AUTO
        if (qnh_cfg>0) {
          qnh_out=qnh_cfg; alt_cm=(int32_t)lroundf( altitude_from_QNH_m(g_bmp_p, g_htu_t, g_htu_h, qnh_out) * 100.0f );
        } else if (elev_m!=0) {
          qnh_out=qnh_from_elev_Pa(g_bmp_p, g_htu_t, g_htu_h, (float)elev_m);
        } else {
          qnh_out=QNH_ISA; alt_cm=(int32_t)lroundf( altitude_from_QNH_m(g_bmp_p, g_htu_t, g_htu_h, qnh_out) * 100.0f );
        }
        break;
    }
  }

  writeU32ToRegs(32, qnh_out);
  writeU32ToRegs(34, (uint32_t)alt_cm);

  // Status + uptime
  mqttPublishState(false);
  mb.Hreg(0,computeStatus());
  writeU32ToRegs(20,(millis()-bootMillis)/1000u);
}

// ----------------------------- Periodický DEBUG -------------------------------
void debugLogPeriodic(){
#if DEBUG
  uint32_t now=millis(); if(now-lastDbgMs<DEBUG_INTERVAL_MS) return; lastDbgMs=now;
  uint16_t status=mb.Hreg(0);
  DLOG("[STAT] up=%lus status=0x%03X WiFi=%s IP=%s RSSI=%d portal=%d AltMode=%u AutoQNH=%u/%uh manICAO=%u\r\n",
       (now-bootMillis)/1000UL, status,
       (WiFi.status()==WL_CONNECTED?"OK":"NOK"),
       (WiFi.status()==WL_CONNECTED?WiFi.localIP().toString().c_str():"0.0.0.0"), WiFi.RSSI(),
       g_portalActive, (unsigned)mb.Hreg(39), (unsigned)mb.Hreg(43), (unsigned)mb.Hreg(44), (unsigned)mb.Hreg(51));
  DLOG("       HTU:T=%.2fC RH=%.2f%% (%s)\r\n", isnan(g_htu_t)?NAN:g_htu_t, isnan(g_htu_h)?NAN:g_htu_h, htu_ok?"OK":"NOK");
  DLOG("       BMP:T=%.2fC P=%luPa (%s)\r\n", isnan(g_bmp_t)?NAN:g_bmp_t, (unsigned long)g_bmp_p, bmp_ok?"OK":"NOK");
  DLOG("       BH :Lux=%.2f lx (%s)\r\n", isnan(g_bh_lux)?NAN:g_bh_lux, bh_ok?"OK":"NOK");
  uint32_t qnh = ((uint32_t)mb.Hreg(32)<<16) | (uint32_t)mb.Hreg(33);
  int32_t alt_cm = (int32_t)(((uint32_t)mb.Hreg(34)<<16) | (uint32_t)mb.Hreg(35));
  DLOG("       QNH=%lu Pa  ALT=%ld cm  Elev=%d m  (AQres=%u)\r\n",
       (unsigned long)qnh, (long)alt_cm, (int16_t)mb.Hreg(36), (unsigned)mb.Hreg(45));
#endif
}

// ----------------------------- LED stavový automat ----------------------------
void ledSelectPattern(){
  if (g_portalActive){                   // magenta rychlé blikání
    ledSetBlink(true,false,true,200,200); return;
  }
  if (g_i2cScanActive){                  // cyan rychlé blikání
    ledSetBlink(false,true,true,150,150); return;
  }
  if (g_selfTestActive){                 // bílá rychlé blikání
    ledSetBlink(true,true,true,150,150); return;
  }
  if (WiFi.status()!=WL_CONNECTED){      // modrá pomalé blikání
    ledSetBlink(false,false,true,250,750); return;
  }
  uint16_t st=computeStatus();
  if (st==0){                            // žádný senzor OK → červená
    ledSetSolid(true,false,false);
  } else if (st!=0x7){                   // některý senzor chybí → žlutá
    ledSetSolid(true,true,false);
  } else {                               // vše OK → zelená
    ledSetSolid(false,true,false);
  }
}

// ----------------------------- FLASH → start portálu --------------------------
void startPortal(){
  char apName[40]; snprintf(apName, sizeof(apName), "%s-SETUP", (CFG.deviceName.length()?CFG.deviceName.c_str():"ESP8266-Meteo"));
  DLOG("[FLASH] Start portal '%s'\r\n", apName);
  #if PORTAL_DISCONNECT_STA
    if(WiFi.status()==WL_CONNECTED){ WiFi.disconnect(); delay(100); }
  #endif
  WiFi.mode(WIFI_AP_STA);
  wm.setConfigPortalTimeout(300);
  wm.startConfigPortal(apName);
  g_portalActive = true;
}
void handleFlashIRQ(){
  if(g_flashReq){ g_flashReq = false; startPortal(); }
  if (g_portalActive && WiFi.softAPgetStationNum()==0 && WiFi.status()==WL_CONNECTED){
    g_portalActive=false;
  }
}

// ----------------------------- AutoQNH: whitelist METAR letišť (CZ) ----------
struct Airport { const char* icao; float lat; float lon; const char* name; };
static const Airport CZ_AIRPORTS[] PROGMEM = {
  {"LKPD", 50.013f, 15.738f, "Pardubice"},
  {"LKPR", 50.101f, 14.260f, "Praha"},
  {"LKTB", 49.151f, 16.695f, "Brno"},
  {"LKMT", 49.696f, 18.111f, "Ostrava"},
  {"LKKV", 50.203f, 12.915f, "Karlovy Vary"},
  {"LKPO", 49.427f, 17.405f, "Přerov"},
  {"LKCS", 48.946f, 14.427f, "České Budějovice"},
};
static const uint8_t CZ_AIRPORTS_N = sizeof(CZ_AIRPORTS)/sizeof(CZ_AIRPORTS[0]);

static bool lookupAirportByICAO(const char* icao, float &lat, float &lon, String &name){
  if (!icao) return false;
  for (uint8_t i=0;i<CZ_AIRPORTS_N;i++){
    Airport a; memcpy_P(&a, &CZ_AIRPORTS[i], sizeof(Airport));
    if (icaoEquals(icao, a.icao)){
      lat = a.lat; lon = a.lon;
      name = String(a.icao) + " (" + String(a.name) + ")";
      return true;
    }
  }
  return false;
}

static void applyGeoFallback(){
  if (g_geoCache.valid){
    setLocation(g_geoCache.lat, g_geoCache.lon, g_geoCache.label, String("GeoIP"), LOC_PRIORITY_GEO);
  } else {
    resetToDefaultLocation();
  }
}

static void updateLocationFromConfig(){
  if (CFG.autoQNH_manual_en && strlen(CFG.autoQNH_manual_icao)==4){
    float lat=DEFAULT_LAT, lon=DEFAULT_LON; String label;
    if (lookupAirportByICAO(CFG.autoQNH_manual_icao, lat, lon, label)){
      setLocation(lat, lon, label, String("Manuální ICAO"), LOC_PRIORITY_MANUAL);
      return;
    }
  }

  if (g_locationPriority == LOC_PRIORITY_MANUAL){
    // manuální lokace deaktivována → vrať GeoIP / výchozí
    g_locationPriority = LOC_PRIORITY_GEO;
    applyGeoFallback();
    return;
  }

  if (!g_locationValid){
    applyGeoFallback();
  }
}

static inline float toRad(float d){ return d * 0.017453292519943295f; }
static float haversine_km(float lat1, float lon1, float lat2, float lon2){
  float dlat = toRad(lat2-lat1), dlon = toRad(lon2-lon1);
  float a = sinf(dlat/2)*sinf(dlat/2) + cosf(toRad(lat1))*cosf(toRad(lat2))*sinf(dlon/2)*sinf(dlon/2);
  float c = 2*atan2f(sqrtf(a), sqrtf(1-a));
  return 6371.0f * c;
}
static int nearestIdxNotTried(float lat, float lon, const bool* tried){
  float bestD = 1e9f; int best = -1;
  for (uint8_t i=0;i<CZ_AIRPORTS_N;i++){
    if (tried[i]) continue;
    Airport a; memcpy_P(&a, &CZ_AIRPORTS[i], sizeof(Airport));
    float d = haversine_km(lat, lon, a.lat, a.lon);
    if (d < bestD){ bestD=d; best=i; }
  }
  return best;
}

// ----------------------------- AutoQNH: HTTP utility --------------------------
static bool httpGET(const String& url, String& out){
  HTTPClient http;
  http.setTimeout(10000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  WiFiClient client;
  if (!http.begin(client, url)) return false;

  int code = http.GET();
  bool ok = (code == HTTP_CODE_OK);
  if (ok) out = http.getString();
  else DLOG("[HTTP] GET fail %s (code=%d)\r\n", url.c_str(), code);

  http.end();
  return ok;
}

// IP geolokace (ip-api.com) – FREE endpoint jen HTTP
static bool geoLocateIP(float &lat, float &lon, String &city){
  String body;
  if (!httpGET("http://ip-api.com/json", body)) return false;
  StaticJsonDocument<512> doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err) return false;
  if (doc["status"] != "success") return false;
  lat = doc["lat"] | NAN;
  lon = doc["lon"] | NAN;
  city = doc["city"] | "";
  return !(isnan(lat) || isnan(lon));
}

// METAR z NOAA TGFTP -> QNH v Pa (Qxxxx nebo Axxxx) — Nyní přes HTTPS
static bool metarQNH_Pa_from_NOAA(const char* icao, uint32_t &qnh_pa_out){
  String url = String("http://tgftp.nws.noaa.gov/data/observations/metar/stations/") + icao + ".TXT";
  String body;
  if (!httpGET(url, body)) return false;

  int nl = body.indexOf('\n');
  String metar = (nl>=0 && nl+1 < (int)body.length()) ? body.substring(nl+1) : body;
  metar.trim();

  int idx = metar.indexOf(" Q");
  if (idx >= 0 && idx+6 <= (int)metar.length()){
    String s = metar.substring(idx+2, idx+6);
    if (s.length()==4 && s.charAt(0)>='0'){
      int q = s.toInt(); if (q>800 && q<1100){ qnh_pa_out = (uint32_t)q * 100U; return true; }
    }
  }
  idx = metar.indexOf(" A");
  if (idx >= 0 && idx+6 <= (int)metar.length()){
    String s = metar.substring(idx+2, idx+6);
    if (s.length()==4 && s.charAt(0)>='0'){
      int a = s.toInt(); // 2992 => 29.92 inHg
      float inHg = a / 100.0f;
      float hPa = inHg * 33.8639f;
      uint32_t Pa = (uint32_t)lroundf(hPa * 100.0f);
      if (Pa>80000 && Pa<110000){ qnh_pa_out = Pa; return true; }
    }
  }
  return false;
}
void modbusSetICAO(const char* icao){ writeICAOToRegs(47,48,icao); }

// ----------------------------- AutoQNH: hlavní procedura ----------------------
enum { AQ_OK=0, AQ_ERR_WIFI=1, AQ_ERR_GEO=2, AQ_ERR_NEAR=3, AQ_ERR_METAR=4 };

void autoQNH_RunOnce(){
  if (WiFi.status()!=WL_CONNECTED){ mb.Hreg(45, AQ_ERR_WIFI); return; }

  // 1) Manuální ICAO override?
  char manualICAO[5]="";
  if (mb.Hreg(51)) {
    readICAOFromRegs(52,53,manualICAO);
    if (strlen(manualICAO)==4){
      uint32_t qnh=0;
      if (metarQNH_Pa_from_NOAA(manualICAO, qnh)){
        writeU32ToRegs(37, qnh);
        modbusSetICAO(manualICAO);
        mb.Hreg(46, 0);
        writeU32ToRegs(49, (millis()-bootMillis)/1000u);
        mb.Hreg(45, AQ_OK);
        DLOG("[AQ] MANUAL %s: QNH=%lu Pa\r\n", manualICAO, (unsigned long)qnh);
        return;
      } else {
        DLOG("[AQ] Manual ICAO METAR FAIL (%s) -> fallback\r\n", manualICAO);
      }
    }
  }

  // 2) Geolokace IP
  float lat=NAN, lon=NAN; String city;
  if (!geoLocateIP(lat, lon, city)){ DLOG("[AQ] Geolocate FAIL\r\n"); mb.Hreg(45, AQ_ERR_GEO); return; }

  g_geoCache.valid = true;
  g_geoCache.lat = lat;
  g_geoCache.lon = lon;
  g_geoCache.label = city.length() ? city : String("GeoIP lokalita");
  if (g_locationPriority <= LOC_PRIORITY_GEO){
    String src = String("GeoIP");
    setLocation(lat, lon, g_geoCache.label, src, LOC_PRIORITY_GEO);
  }

  // 3) Hledání nejbližších + fallback (zkusíme všechna let.)
  bool tried[CZ_AIRPORTS_N]; memset(tried, 0, sizeof(tried));
  uint32_t qnh=0; char icao[5]=""; uint16_t dist10=0;
  int successIdx = -1;

  for (int attempt=0; attempt < CZ_AIRPORTS_N; ++attempt){
    int idx = nearestIdxNotTried(lat, lon, tried);
    if (idx < 0) break;
    tried[idx] = true;

    Airport a; memcpy_P(&a, &CZ_AIRPORTS[idx], sizeof(Airport));
    strncpy(icao, a.icao, 4); icao[4]='\0';
    dist10 = (uint16_t)lroundf(haversine_km(lat,lon,a.lat,a.lon)*10.0f);

    if (metarQNH_Pa_from_NOAA(icao, qnh)){
      successIdx = idx;
      break;
    } else {
      DLOG("[AQ] METAR fetch FAIL (%s) -> trying next...\r\n", icao);
    }
  }

  if (successIdx < 0){ mb.Hreg(45, AQ_ERR_METAR); return; }

  writeU32ToRegs(37, qnh);
  modbusSetICAO(icao);
  mb.Hreg(46, dist10);
  writeU32ToRegs(49, (millis()-bootMillis)/1000u);
  mb.Hreg(45, AQ_OK);

  if (g_locationPriority <= LOC_PRIORITY_GEO){
    Airport a; memcpy_P(&a, &CZ_AIRPORTS[successIdx], sizeof(Airport));
    String src = String("GeoIP + ") + String(icao);
    String label = g_geoCache.label.length() ? g_geoCache.label : String(a.icao);
    setLocation(lat, lon, label, src, LOC_PRIORITY_GEO);
  }

  DLOG("[AQ] %s: QNH=%lu Pa, dist=%.1f km, city='%s'\r\n",
       icao, (unsigned long)qnh, dist10/10.0f, city.c_str());
}

// ----------------------------- KONZOLNÍ (SERIAL) ROZHRANÍ ---------------------
// Příkazy (bez diakritiky):
//  help
//  show
//  set name "ESP Meteo" | set unit 1 | set poll 10000 | set autotest 0|1
//  set elev <m> | set qnh <Pa> | set alt|altmode 0|1|2|3
//  set aq_en 0|1 | set aq_per <h> | set aq_man 0|1 | set aq_icao XXXX
//  save | load | reboot
//  portal
//  aq run
//  scan
//  selftest
//  mb rd <addr> [n]
//  mb wr <addr> <val>
static char g_cliBuf[128];
static uint8_t g_cliLen = 0;

static void printConfig(){
  uint32_t qnh = ((uint32_t)mb.Hreg(32)<<16) | (uint32_t)mb.Hreg(33);
  int32_t  alt_cm = (int32_t)(((uint32_t)mb.Hreg(34)<<16) | (uint32_t)mb.Hreg(35));
  char icao[5]; readICAOFromRegs(47,48,icao);
  Serial.println(F("\r\n=== CONFIG ==="));
  Serial.printf("Name: %s\r\n", CFG.deviceName.c_str());
  Serial.printf("UnitID: %u, PollMs: %lu, AutoTest: %u\r\n", CFG.unitId, (unsigned long)CFG.pollMs, CFG.autoTest);
  Serial.printf("Elev: %d m, QNH_cfg: %lu Pa, AltMode: %u\r\n", (int)CFG.cfgElevation_m, (unsigned long)CFG.cfgQNH_Pa, (unsigned)CFG.altMode);
  Serial.printf("AutoQNH: %u / %uh, Manual: %u, ICAO: '%s'\r\n", (unsigned)CFG.autoQNH_enable, (unsigned)CFG.autoQNH_period_h, (unsigned)CFG.autoQNH_manual_en, CFG.autoQNH_manual_icao);
  Serial.printf("MQTT: %s host=%s:%u base='%s' clientId='%s' user='%s' HA=%u Lox=%u\r\n",
                CFG.mqtt_enable?"EN":"DIS",
                CFG.mqtt_host.c_str(), (unsigned)CFG.mqtt_port,
                CFG.mqtt_baseTopic.c_str(), CFG.mqtt_clientId.c_str(),
                CFG.mqtt_username.c_str(),
                (unsigned)CFG.mqtt_ha_discovery, (unsigned)CFG.mqtt_loxone_discovery);
  Serial.printf("MQTT status: %s\r\n", g_mqttState.status.c_str());
  Serial.printf("Runtime: QNH=%lu Pa, ALT=%ld cm, AQres=%u, UsedICAO='%s', Dist10=%u\r\n",
                (unsigned long)qnh, (long)alt_cm, (unsigned)mb.Hreg(45), icao, (unsigned)mb.Hreg(46));
  Serial.printf("WiFi: %s  IP: %s  RSSI:%d  Portal:%u\r\n",
                (WiFi.status()==WL_CONNECTED?"OK":"NOK"),
                (WiFi.status()==WL_CONNECTED?WiFi.localIP().toString().c_str():"0.0.0.0"),
                WiFi.RSSI(), g_portalActive);
  Serial.println(F("==============\r\n"));
}

static void printHelp(){
  Serial.println(F(
    "\r\n--- ESP Meteo CLI ---\r\n"
    "help                       - napoveda\r\n"
    "show                       - vypis aktualniho nastaveni\r\n"
    "set name \"ESP8266-Meteo\"  - nazev zarizeni (host name)\r\n"
    "set unit <1..247>          - Modbus Unit ID\r\n"
    "set poll <ms>              - perioda mereni (min 200 ms)\r\n"
    "set autotest 0|1           - autotest pri bootu\r\n"
    "set elev <m>               - elevace (m AMSL)\r\n"
    "set qnh <Pa>               - QNH konfiguracni (Pa)\r\n"
    "set alt|altmode 0|1|2|3    - AltMode (0=AUTO,1=QNH,2=ISA,3=ELEV)\r\n"
    "set aq_en 0|1              - AutoQNH enable\r\n"
    "set aq_per <h>             - AutoQNH perioda (h)\r\n"
    "set aq_man 0|1             - manual ICAO enable\r\n"
    "set aq_icao XXXX           - manual ICAO (4 znaky)\r\n"
    "set mqtt_en 0|1            - MQTT povolit / zakázat\r\n"
    "set mqtt_host host         - MQTT broker\r\n"
    "set mqtt_port <1..65535>   - MQTT port\r\n"
    "set mqtt_id ID             - MQTT clientId\r\n"
    "set mqtt_user user         - MQTT uživatel\r\n"
    "set mqtt_pass password     - MQTT heslo (použijte \"\" pro mezery)\r\n"
    "set mqtt_base topic        - MQTT base topic\r\n"
    "set mqtt_ha 0|1            - Home Assistant autodiscovery\r\n"
    "set mqtt_lox 0|1           - Loxone autodiscovery\r\n"
    "save / load                - ulozit / nacist z FS\r\n"
    "portal                     - spustit WiFi portal\r\n"
    "aq run                     - spustit AutoQNH nyni\r\n"
    "scan                       - I2C scan\r\n"
    "selftest                   - self-test senzoru\r\n"
    "mb rd <addr> [n]           - cteni Hreg (0-based)\r\n"
    "mb wr <addr> <val>         - zapis Hreg (0-based)\r\n"
  ));
}

static void applyConfigToModbusMirror(){
  mb.Hreg(1,CFG.unitId);
  mb.Hreg(36,(uint16_t)CFG.cfgElevation_m);
  writeU32ToRegs(37,CFG.cfgQNH_Pa);
  mb.Hreg(39,(uint16_t)CFG.altMode);
  mb.Hreg(43,(uint16_t)CFG.autoQNH_enable);
  mb.Hreg(44,(uint16_t)CFG.autoQNH_period_h);
  mb.Hreg(51,(uint16_t)CFG.autoQNH_manual_en);
  if (strlen(CFG.autoQNH_manual_icao)==4) writeICAOToRegs(52,53,CFG.autoQNH_manual_icao);
  WiFi.hostname(CFG.deviceName); // uplatni se po reconnectu
  updateLocationFromConfig();
}

#include "HttpApiUi.h"

// jednoduchy tokenizator s uvozovkami
static int tokenize(const String& line, String* args, int maxArgs){
  int argc=0; bool inQ=false; String cur;
  for (size_t i=0;i<line.length();++i){
    char c=line[i];
    if (c=='"'){ inQ=!inQ; continue; }
    if (!inQ && (c==' '||c=='\t')){
      if (cur.length()){ if (argc<maxArgs) args[argc++]=cur; cur=""; }
    }else{
      cur+=c;
    }
  }
  if (cur.length() && argc<maxArgs) args[argc++]=cur;
  return argc;
}

static bool isInt(const String& s){
  if(!s.length()) return false;
  for (size_t i=0;i<s.length();++i){ char c=s[i]; if (!(isDigit(c) || ((c=='-'||c=='+')&&i==0))) return false; }
  return true;
}

static void cliHandleLine(const String& line){
  String args[8]; int argc=tokenize(line, args, 8);
  if (argc==0) return;
  String cmd=args[0]; cmd.toLowerCase();

  if (cmd=="help" || cmd=="?"){ printHelp(); return; }
  if (cmd=="show" || cmd=="config"){ printConfig(); return; }

  if (cmd=="set" && argc>=3){
    String key=args[1]; key.toLowerCase();
    String val=args[2];

    if (key=="name"){
      String name = line.substring(line.indexOf("name")+4); name.trim();
      if (name.length() && name[0]=='"'){ // odstranit uvozovky
        int q2 = name.lastIndexOf('"');
        name = (q2>0)? name.substring(1,q2) : name.substring(1);
      }
      name.trim();
      if (name.length()>0 && name.length()<32){
        CFG.deviceName = name;
        applyConfigToModbusMirror();
        Serial.printf("OK: name='%s'\r\n", CFG.deviceName.c_str());
      } else Serial.println(F("ERR: invalid name length"));
      return;
    }
    if (key=="unit" && isInt(val)){
      long v=val.toInt(); if (v>=1 && v<=247){ CFG.unitId=(uint16_t)v; applyConfigToModbusMirror(); Serial.printf("OK: unit=%ld\r\n",v); }
      else Serial.println(F("ERR: unit range 1..247"));
      return;
    }
    if (key=="poll" && isInt(val)){
      long v=val.toInt(); if (v<200) v=200; CFG.pollMs=(uint32_t)v; Serial.printf("OK: pollMs=%lu\r\n",(unsigned long)CFG.pollMs); return;
    }
    if (key=="autotest" && isInt(val)){ CFG.autoTest=(val.toInt()!=0); Serial.printf("OK: autotest=%u\r\n",CFG.autoTest); return; }
    if (key=="elev" && isInt(val)){
      long v=val.toInt(); if (v<-32768) v=-32768; if (v>32767) v=32767; CFG.cfgElevation_m=(int16_t)v; applyConfigToModbusMirror(); Serial.printf("OK: elev=%ld\r\n",v); return;
    }
    if (key=="qnh" && isInt(val)){
      long v=val.toInt(); if (v<0) v=0; if (v>200000) v=200000; CFG.cfgQNH_Pa=(uint32_t)v; applyConfigToModbusMirror(); Serial.printf("OK: qnh=%ld\r\n",v); return;
    }
    if ((key=="alt" || key=="altmode" || key=="mode") && isInt(val)){   // ← aliasy
      long v=val.toInt(); if (v<0||v>3){ Serial.println(F("ERR: alt 0..3")); return; }
      CFG.altMode=(uint8_t)v; applyConfigToModbusMirror(); Serial.printf("OK: altMode=%ld\r\n",v); return;
    }
    if (key=="aq_en" && isInt(val)){ CFG.autoQNH_enable=(val.toInt()!=0); applyConfigToModbusMirror(); Serial.printf("OK: aq_en=%u\r\n",CFG.autoQNH_enable); return; }
    if (key=="aq_per" && isInt(val)){
      long v=val.toInt(); if (v<1) v=1; if (v>255) v=255; CFG.autoQNH_period_h=(uint8_t)v; applyConfigToModbusMirror(); Serial.printf("OK: aq_per=%ldh\r\n",v); return;
    }
    if (key=="aq_man" && isInt(val)){ CFG.autoQNH_manual_en=(val.toInt()!=0); applyConfigToModbusMirror(); Serial.printf("OK: aq_man=%u\r\n",CFG.autoQNH_manual_en); return; }
    if (key=="aq_icao"){
      val.trim(); val.toUpperCase();
      if (val.length()==4){
        strlcpy(CFG.autoQNH_manual_icao, val.c_str(), sizeof(CFG.autoQNH_manual_icao));
        applyConfigToModbusMirror();
        Serial.printf("OK: aq_icao='%s'\r\n", CFG.autoQNH_manual_icao);
      } else Serial.println(F("ERR: ICAO must be 4 chars"));
      return;
    }
    if (key=="mqtt_en" && isInt(val)){
      CFG.mqtt_enable = (uint8_t)(val.toInt()!=0);
      ensureMqttDefaults(); mqttOnConfigChanged();
      Serial.printf("OK: mqtt_en=%u\r\n", (unsigned)CFG.mqtt_enable);
      return;
    }
    if (key=="mqtt_host"){
      CFG.mqtt_host = val; CFG.mqtt_host.trim();
      ensureMqttDefaults(); mqttOnConfigChanged();
      Serial.printf("OK: mqtt_host='%s'\r\n", CFG.mqtt_host.c_str());
      return;
    }
    if (key=="mqtt_port" && isInt(val)){
      long p = val.toInt(); if (p<1) p=1; if (p>65535) p=65535;
      CFG.mqtt_port = (uint16_t)p;
      ensureMqttDefaults(); mqttOnConfigChanged();
      Serial.printf("OK: mqtt_port=%ld\r\n", p);
      return;
    }
    if (key=="mqtt_id"){
      CFG.mqtt_clientId = val;
      CFG.mqtt_clientId.trim();
      ensureMqttDefaults(); mqttOnConfigChanged();
      Serial.printf("OK: mqtt_id='%s'\r\n", CFG.mqtt_clientId.c_str());
      return;
    }
    if (key=="mqtt_user"){
      CFG.mqtt_username = val;
      CFG.mqtt_username.trim();
      ensureMqttDefaults(); mqttOnConfigChanged();
      Serial.printf("OK: mqtt_user='%s'\r\n", CFG.mqtt_username.c_str());
      return;
    }
    if (key=="mqtt_pass"){
      int idx = line.indexOf("mqtt_pass");
      String pass = (idx>=0)? line.substring(idx+9) : val;
      pass.trim();
      if (pass.length() && pass[0]=='"'){
        int q2 = pass.lastIndexOf('"');
        pass = (q2>0)? pass.substring(1,q2) : pass.substring(1);
      }
      CFG.mqtt_password = pass;
      ensureMqttDefaults(); mqttOnConfigChanged();
      Serial.println(F("OK: mqtt_pass set"));
      return;
    }
    if (key=="mqtt_base"){
      CFG.mqtt_baseTopic = val;
      CFG.mqtt_baseTopic.trim();
      ensureMqttDefaults(); mqttOnConfigChanged();
      Serial.printf("OK: mqtt_base='%s'\r\n", CFG.mqtt_baseTopic.c_str());
      return;
    }
    if (key=="mqtt_ha" && isInt(val)){
      CFG.mqtt_ha_discovery = (uint8_t)(val.toInt()!=0);
      ensureMqttDefaults(); mqttOnConfigChanged();
      Serial.printf("OK: mqtt_ha=%u\r\n", (unsigned)CFG.mqtt_ha_discovery);
      return;
    }
    if (key=="mqtt_lox" && isInt(val)){
      CFG.mqtt_loxone_discovery = (uint8_t)(val.toInt()!=0);
      ensureMqttDefaults(); mqttOnConfigChanged();
      Serial.printf("OK: mqtt_lox=%u\r\n", (unsigned)CFG.mqtt_loxone_discovery);
      return;
    }
    Serial.println(F("ERR: unknown key (see 'help')"));
    return;
  }

  if (cmd=="save"){ if (saveConfigFS()){ Serial.println(F("OK: config saved")); } else Serial.println(F("ERR: save failed")); return; }
  if (cmd=="load"){ if (loadConfigFS()){ applyConfigToModbusMirror(); ensureMqttDefaults(); mqttOnConfigChanged(); Serial.println(F("OK: config loaded")); } else Serial.println(F("ERR: load failed")); return; }
  if (cmd=="reboot"){ Serial.println(F("Rebooting...")); mqttPublishAvailability(false); delay(100); ESP.restart(); return; }

  if (cmd=="portal"){ startPortal(); Serial.println(F("OK: portal started")); return; }
  if (cmd=="aq" && argc>=2 && args[1]=="run"){ Serial.println(F("AQ: run now")); autoQNH_RunOnce(); return; }
  if (cmd=="scan"){ i2cScan(); Serial.println(F("I2C scan requested")); return; }
  if (cmd=="selftest"){ sensorSelfTest(); Serial.println(F("Selftest requested")); return; }

  if (cmd=="mb" && argc>=3){
    String sub=args[1]; sub.toLowerCase();
    if (sub=="rd"){
      int addr=args[2].toInt(); int n=(argc>=4)?args[3].toInt():1; if (n<1) n=1; if (n>32) n=32;
      for (int i=0;i<n;i++){ Serial.printf("H[%d]=%u\r\n", addr+i, (unsigned)mb.Hreg(addr+i)); }
      return;
    } else if (sub=="wr" && argc>=4){
      int addr=args[2].toInt(); uint16_t v=(uint16_t)args[3].toInt(); mb.Hreg(addr, v); Serial.printf("H[%d]<=%u\r\n", addr, (unsigned)v); return;
    }
  }

  Serial.println(F("ERR: unknown command (try 'help')"));
}

static void consoleTask(){
  while (Serial.available()){
    char c = Serial.read();
    if (c=='\r') continue;
    if (c=='\n'){
      g_cliBuf[g_cliLen]=0;
      String line = String(g_cliBuf);
      line.trim();
      if (line.length()) cliHandleLine(line);
      g_cliLen=0;
      Serial.print(F("> "));
    } else if (c==8 || c==127){ // backspace
      if (g_cliLen>0) g_cliLen--;
    } else {
      if (g_cliLen < sizeof(g_cliBuf)-1) g_cliBuf[g_cliLen++]=c;
    }
  }
}

// ----------------------------- setup / loop -----------------------------------
void setup(){
  Serial.begin(115200); Serial.println(); Serial.println(F("== Meteostanice ESPx / Modbus TCP =="));
  #if defined(SINGLE_LED_PIN)
    pinMode(SINGLE_LED_PIN,OUTPUT); digitalWrite(SINGLE_LED_PIN,LOW);
  #else
    pinMode(LED_R_PIN,OUTPUT); pinMode(LED_G_PIN,OUTPUT); pinMode(LED_B_PIN,OUTPUT);
    ledWrite(false,false,false);
  #endif
  pinMode(FLASH_BTN_PIN,INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(FLASH_BTN_PIN), isrFlash, FALLING);
  if(!LittleFS.begin()){ Serial.println(F("LittleFS mount FAILED, formatting...")); LittleFS.format(); LittleFS.begin(); }
  resetToDefaultLocation();
  loadConfigFS();
  ensureMqttDefaults();
  mqttOnConfigChanged();
  updateLocationFromConfig();
  WiFi.hostname(CFG.deviceName);
  setupWiFi();
  initTimeSync();
  httpTryStart();
  httpInstallUI();   // ← přidá /ui (a přesměrování z "/") + /api/*
  //sensorsPowerInit();
  //sensorsPowerOn();
  setupSensors();
  setupModbus();
  // promítnout manuální ICAO i do Modbus po bootu
  mb.Hreg(51, (uint16_t)CFG.autoQNH_manual_en);
  if (strlen(CFG.autoQNH_manual_icao)==4) writeICAOToRegs(52,53, CFG.autoQNH_manual_icao);
  if(CFG.autoTest){ i2cScan(); sensorSelfTest(); }
  bootMillis=millis(); lastDbgMs=0;
  ledSelectPattern();
  // AutoQNH – první běh cca po 10 s
  lastAutoQNH_ms = millis() - (CFG.autoQNH_period_h*3600000UL) + 10000UL;
  // CLI banner
  printHelp();
  printConfig();
  httpTryStart();

  Serial.print(F("> "));
}

void loop(){
  // --- Serial CLI ---
  consoleTask();

  wm.process();

  if(shouldSaveConfig){
    applyParamsFromWM(); saveConfigFS();
    // zrcadlení do Modbus
    applyConfigToModbusMirror();
    shouldSaveConfig=false;
    DLOG("[CFG] Saved. Name='%s' UnitID=%u PollMs=%lu AutoTest=%u Elev=%d QNH=%lu AltMode=%u AutoQNH=%u/%uh ManualICAO=%u '%s'\r\n",
         CFG.deviceName.c_str(), CFG.unitId, (unsigned long)CFG.pollMs, CFG.autoTest,
         (int)CFG.cfgElevation_m, (unsigned long)CFG.cfgQNH_Pa, (unsigned)CFG.altMode,
         (unsigned)CFG.autoQNH_enable, (unsigned)CFG.autoQNH_period_h,
         (unsigned)CFG.autoQNH_manual_en, CFG.autoQNH_manual_icao);
  }

  handleFlashIRQ();

  if(WiFi.status()==WL_CONNECTED) mb.task();

  mqttLoop();

  // Modbus příkazy přes registry
  if(mb.Hreg(100)==1){ DLOG("[CMD] save&reboot\r\n"); saveConfigFS(); mb.Hreg(100,0); mqttPublishAvailability(false); delay(100); ESP.restart(); }
  if(mb.Hreg(101)==1){ i2cScan(); }
  if(mb.Hreg(102)==1){ sensorSelfTest(); }

  // Live update konfigu z Modbusu
  static int16_t  lastElev = CFG.cfgElevation_m;
  static uint32_t lastQNH  = CFG.cfgQNH_Pa;
  static uint8_t  lastMode = CFG.altMode;
  static uint8_t  lastAQen = CFG.autoQNH_enable;
  static uint8_t  lastAQph = CFG.autoQNH_period_h;
  static uint8_t  lastAQme = CFG.autoQNH_manual_en;
  static char     lastAQmc[5] = "";

  int16_t  elevNow = (int16_t)mb.Hreg(36);
  uint32_t qnhNow  = ((uint32_t)mb.Hreg(37) << 16) | (uint32_t)mb.Hreg(38);
  uint8_t  modeNow = (uint8_t)mb.Hreg(39);
  uint8_t  aqEnNow = (uint8_t)mb.Hreg(43);
  uint8_t  aqPhNow = (uint8_t)mb.Hreg(44);
  uint8_t  aqMeNow = (uint8_t)mb.Hreg(51);
  char     aqMcNow[5]; readICAOFromRegs(52,53, aqMcNow);

  if (elevNow != lastElev || qnhNow != lastQNH || modeNow != lastMode ||
      aqEnNow != lastAQen || aqPhNow != lastAQph || aqMeNow != lastAQme || strncmp(aqMcNow,lastAQmc,4)!=0) {
    CFG.cfgElevation_m = elevNow;
    CFG.cfgQNH_Pa = qnhNow;
    CFG.altMode = (modeNow<=3)?modeNow:0;
    CFG.autoQNH_enable = (aqEnNow?1:0);
    CFG.autoQNH_period_h = (aqPhNow?aqPhNow:8);
    CFG.autoQNH_manual_en = (aqMeNow?1:0);
    if (strlen(aqMcNow)==4){ strlcpy(CFG.autoQNH_manual_icao, aqMcNow, sizeof(CFG.autoQNH_manual_icao)); }
    else { CFG.autoQNH_manual_icao[0]='\0'; CFG.autoQNH_manual_en=0; mb.Hreg(51,0); }

    lastElev = elevNow; lastQNH = qnhNow; lastMode = CFG.altMode;
    lastAQen = CFG.autoQNH_enable; lastAQph = CFG.autoQNH_period_h; lastAQme = CFG.autoQNH_manual_en;
    strlcpy(lastAQmc, CFG.autoQNH_manual_icao, sizeof(lastAQmc));

    updateLocationFromConfig();

    DLOG("[CFG] Live: Elev=%d m, QNH=%lu Pa, AltMode=%u, AutoQNH=%u/%uh, ManualICAO=%u '%s'\r\n",
         (int)elevNow, (unsigned long)qnhNow, (unsigned)CFG.altMode,
         (unsigned)CFG.autoQNH_enable, (unsigned)CFG.autoQNH_period_h,
         (unsigned)CFG.autoQNH_manual_en, CFG.autoQNH_manual_icao);
  }

  // Periodické měření
  uint32_t now=millis();
  if(now-lastPollMs>=CFG.pollMs){ lastPollMs=now; pollAndPublishSensors(); }

  // AutoQNH plánovač
  if (CFG.autoQNH_enable){
    uint32_t per_ms = (uint32_t)CFG.autoQNH_period_h * 3600000UL;
    if (per_ms < 600000UL) per_ms = 600000UL; // min 10 min
    if (now - lastAutoQNH_ms >= per_ms){
      lastAutoQNH_ms = now;
      DLOG("[AQ] Periodic run...\r\n");
      autoQNH_RunOnce(); // blokující (typicky <5 s), volá se řídce
    }
  }

  // ---- Modbus echo self-test ----
  if (now - mbEchoLastMs >= 2000) {
    mbEchoLastMs = now;
    mbEchoToken++; if (mbEchoToken == 0) mbEchoToken = 1;
    mb.Hreg(74, mbEchoToken);            // publikuj nový token
  }
  if (mb.Hreg(75) == mbEchoToken) {
    uint32_t tS = (millis() - bootMillis) / 1000UL;
    writeU32ToRegs(76, tS);              // čas poslední shody tokenu
    mb.Hreg(75, 0);                      // vynulovat návrat pro další cyklus
  }
  // (mb.Hreg(71) = MB Test Result) nepoužíváme, necháváme 0=OK

  ledSelectPattern(); ledUpdate();
  debugLogPeriodic();
  httpTryStart();     // pokud se Wi-Fi připojí až později
  server.handleClient();

  yield();
}
