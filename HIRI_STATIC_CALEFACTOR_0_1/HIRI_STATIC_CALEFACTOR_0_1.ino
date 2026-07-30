/*
 * HIRI_STATIC_CALEFACTOR_0_1.ino - Version CAL V0.0.1
 * Creacion original: Alejandro Rebolledo <arebolledo@udd.cl>
 * SPDX-License-Identifier: CC-BY-NC-4.0
 * Firmware HIRI AUCA estatico: Plantower, calefactor, GNSS, SD, OLED y HTTP.
 * 
 * Basado en la version Debug 0.1.3V (la mas estable) pero con mejoras de performance:
 * - Modem inicializado de forma sincronica en boot para maxima estabilidad.
 * - Lectura de Plantower por serial ASINCRONICA (no bloqueante).
 * - Enlace serial independiente con el calefactor.
 * - Monitoreo de etapas (setStage) y Heartbeat (RAM/Heap).
 */

#include "config.h"
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include <Preferences.h>
#include <RTClib.h>
#include <SoftwareSerial.h>
#include <DNSServer.h>
#include <U8g2lib.h>
#include <WebServer.h>
#include <WiFi.h>
#include <Wire.h>
#include <esp_task_wdt.h>

// Modem
#define SerialAT Serial1
TinyGsm modem(SerialAT);

// -------------------- Global Constants --------------------
#define RTC_PROBE_PERIOD_MS 60000
#define RTC_SYNC_THRESHOLD 30
#define MIN_VALID_EPOCH 1672531200 // 2023-01-01
#define WDT_TIMEOUT 60
static const uint32_t I2C_CLOCK_HZ = 50000;
static const uint32_t HEARTBEAT_PERIOD_MS = 10000;
static const uint32_t MODEM_TESTAT_TOTAL_MS = 45000;
static const uint32_t MODEM_TESTAT_RETRY_MS = 1000;

const byte HEADER = 0xAA;
const byte CMD = 0xCF;
const byte TAIL = 0xAB;

String VERSION = "CAL V1.0.1";

// -------------------- Global States --------------------
bool oledOK;
bool rtcOK = false;
bool heaterConnected = false;
bool heaterVersionKnown = false;
String heaterFirmwareVersion = "";
bool SDOK = false;
bool wifiModeActive = false;
bool hasRed = false;
bool wdtStarted = false;
const char *lastStage = "BOOT";

SystemConfig config;

// Hardware Objects
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
Adafruit_NeoPixel pixels(NUMPIXELS, NEOPIX_PIN, NEO_GRB + NEO_KHZ800);
RTC_DS3231 rtc;
Preferences prefs;
SPIClass spiSD(HSPI);
WebServer server(80);
DNSServer dnsServer;

SoftwareSerial pms(pms_TX, pms_RX);

// Button flags (edge + debounce)
volatile bool btn1ClickFlag = false;
volatile bool btn2ClickFlag = false;
volatile bool btn2HoldFlag = false;
volatile uint32_t lastDebounceTime1 = 0;
volatile uint32_t lastDebounceTime2 = 0;
const uint32_t BTN1_DEBOUNCE_MS = 80;
const uint32_t BTN2_DEBOUNCE_MS = 80;
const uint32_t BOOT_REVIEW_HOLD_MS = 3000;

// Global Data
uint16_t PM1 = 0, PM25 = 0, PM10 = 0;
float pmsTempC = NAN, pmsHum = NAN;
float heaterExternalTemp = NAN;
float heaterShtTemp = NAN, heaterShtHum = NAN;
float heaterBmeTemp = NAN, heaterBmeHum = NAN;
float heaterThermoTemp = NAN;
bool heaterRelayOn = false, heaterByHumidity = false;
bool heaterByTemperature = false, heaterEmergencyMode = false;
uint32_t heaterLastDataMs = 0;
float rtcTempC = NAN;
float batV = 0.0f;
int csq = 99;
bool networkError = false;
bool streaming = false;
bool loggingEnabled = false;

String gpsLat = "NaN", gpsLon = "NaN";
String gpsTime = "N/A", gpsDate = "N/A";
String satellitesStr = "0", hdopStr = "N/A", gpsAlt = "N/A";
String gpsStatus = "NoFix";
String gpsSpeedKmh = "0.0";

uint8_t debugScreenIndex = 0;
uint32_t lastDebugRotationMs = 0;
const uint32_t DEBUG_ROTATION_INTERVAL_MS = 10000;

String csvFileName = "";
String logFilePath = "";
String failedTxPath = "";
String currentNote = "9";
String lastSavedCSVLine = "";
File uploadFile;
String deviceID = "/HIRI-AUCA";
const char *DEVICE_ID_STR = "4"; // 1..4 corresponden a HIRI-AUCA-1..4
String AP_SSID_STR = "";
const char *AP_PASSWORD = "12345678";
String apIpStr = "0.0.0.0";

const char apn[] = "flolive.net";
const char gprsUser[] = "";
const char gprsPass[] = "";
const char *API_BASE = "http://api-sensores.cmasccp.cl/insertarMedicion";
const char *GLOBAL_IDS_VARIABLES = "11,12,15,45,46,4,3,6,7,8,9,3,6,57,58,59";

uint32_t sendCounter = 0;
uint32_t sdSaveCounter = 0;
uint32_t lastHttpSend = 0;
uint32_t lastSdSave = 0;
uint32_t lastHttpActivityMs = 0;
uint32_t lastSdActivityMs = 0;
bool lastHttpOk = false;
bool lastSdOk = false;
bool lastTxPayloadSdOk = false;
String lastHttpMeasurementValues = "";
bool hasHttpAttempted = false;
bool bootHttpAttemptPending = true;
uint8_t lastDayLogged = 0;
bool wasStreamingBeforeBoot = false;

// Animation Variables
int logoXOffset = -25;
int hiriXOffset = 128;
int proYOffset = 64;
const int LOGO_FINAL_X = 4;
const int HIRI_FINAL_X = 48;
const int PRO_FINAL_X = 106;
const int HIRI_FINAL_Y = 44;
const int PRO_FINAL_Y = 52;

// Watchdog, network and diagnostics
String rebootReason = "Unknown";
String networkOperator = "N/A";
String networkTech = "N/A";
String signalQuality = "N/A";
String registrationStatus = "N/A";
uint32_t lastXtraDownload = 0;
bool xtraSupported = false;
bool xtraLastOk = false;
const uint32_t XTRA_REFRESH_MS = 3UL * 24UL * 60UL * 60UL * 1000UL;
const uint8_t HTTP_FAIL_MAX_CONSECUTIVE = 6;
const uint32_t HTTP_RETRY_BACKOFF_MS = 60UL * 60UL * 1000UL;
uint8_t httpConsecutiveFailCount = 0;
bool httpBackoffActive = false;
uint32_t httpBackoffUntilMs = 0;
char currentCriticalStage[32] = "boot";
RTC_DATA_ATTR char previousResetStage[32] = "boot";
RTC_DATA_ATTR bool previousResetStageValid = false;

// Display State
volatile DisplayState displayState = DISP_NORMAL;
volatile uint32_t displayStateStartTime = 0;
uint32_t lastOledActivity = 0;

const uint8_t BOOT_ITEM_COUNT = 5;
const char *BOOT_ITEM_LABELS[BOOT_ITEM_COUNT] = {
    "OLED", "SD", "RTC", "PLANTOWER", "CALEFACTOR"};
char bootItemStatus[BOOT_ITEM_COUNT][6] = {
    "WAIT", "WAIT", "WAIT", "WAIT", "WAIT"};

// Modem Sync
uint8_t rtcModemSyncCount = 0;
uint32_t lastModemSyncAttempt = 0;
const uint32_t MODEM_SYNC_INTERVAL_MS = 600000;
const uint8_t MAX_MODEM_SYNC_COUNT = 3;
bool rtcNetSyncPending = false;
uint32_t rtcNextProbeMs = 0;

// AT Command Struct
struct AtSession {
  bool active = false;
  String expect1;
  String expect2;
  String resp;
  uint32_t deadline = 0;
} at;

// Buffer for PMS
static uint8_t pmsBuf[64];
static size_t pmsHead = 0;
static uint32_t lastPmsSeen = 0;

// Battery Averaging
const float alpha = 0.8;
float batteryVoltageAverage = 0;
static uint32_t lastBatSample = 0;
static float batSampleSum = 0;
static int batSampleCount = 0;
const int NUM_SAMPLES = 30;
const uint32_t BAT_SAMPLE_INTERVAL_MS = 5;

// Variables needed for GPS diag
uint32_t lastNmeaSeenMs = 0;
uint32_t gnssFixFirstMs = 0;
bool gnssFixReported = false;
uint8_t gsaFixType = 0;
uint8_t gsaSatsUsed = 0;
float gsaPdop = NAN, gsaHdop2 = NAN, gsaVdop = NAN;
uint16_t gsvSatsInView = 0;
float gsvSnrAvg = 0, gsvSnrMax = 0;
uint32_t gsvLastMs = 0;
float gsvSnrAcc = 0;
int gsvSnrCnt = 0;
uint32_t lastNmeaMs = 0;
uint32_t lastGgaMs = 0;
uint32_t lastFixMs = 0;
uint16_t nmeaCount1s = 0;
uint16_t nmeaRate = 0;
uint32_t nmeaRefMs = 0;
uint8_t fixQLast = 0;
bool ttffPrinted = false;
uint32_t gnssStartMs = 0;
bool haveFix = false;

// SD Definition constants
const int SD_SCLK = 14, SD_MISO = 2, SD_MOSI = 15, SD_CS = 13;

// -------------------- Prototypes --------------------
void loadConfig();
void saveConfig();
void applyLEDConfig();
void writeErrorLogHeader();
String generateCSVFileName();
void writeCSVHeader();
void drawAnimation();
void startWifiApServer();
void stopWifiApServer();
void renderDisplay();
void oledStatus(const String &l1, const String &l2 = "", const String &l3 = "", const String &l4 = "");
String buildCurrentMeasurementValues();
bool saveMeasurementToSD(const String &values, const String &connectionStatus);
void checkRebootReason();
void readPMS();
void pollHeaterSerial();
void heaterRequestFirmwareVersion();
void updatePmLed(float pm25);
void gnssBringUp();
void gnssDiagTick();
void gnssDebugPollAsync();
void gnssWatchdog();
bool atTick(bool &done, bool &ok);
bool atRun(const String &cmd, const String &expect1 = "OK", const String &expect2 = "ERROR", uint32_t timeout_ms = 8000);
bool sendAtSync(const String &cmd, String &resp, uint32_t timeout_ms = 4000);
bool httpGet_webhook(const String &url);
bool detectAndEnableXtra();
bool downloadXtraOnce();
void parseNMEA(const String &line);
void saveFailedTransmission(const String &url, const String &errorType);
bool sendCurrentMeasurement();
void handleButtonLogic();
void showMessage(const char *msg);
void processSerialCommand();
void logError(const String &type, const String &ctx, const String &msg);
void IRAM_ATTR isr_btn2();
bool i2cDevicePresent(uint8_t address);
void bootStatus(const String &stage, const String &detail, bool ok);
void renderBootWindow();
void refreshSignalQuality();
extern void ui_btn1_click();
extern void ui_btn2_click();
String missingUrlValue();
String safeFloatStr(float v);
String safeUIntStr(uint32_t v);
String safeIntStr(int v);
String safeNumberStr(const String &s);
String safeGpsStr(const String &s);
String safeSatsStr(const String &s);

// -------------------- Debug Helpers --------------------
void feedWdt() { if (wdtStarted) esp_task_wdt_reset(); }
void startWdtOnce() {
  if (!wdtStarted) {
    esp_task_wdt_init(WDT_TIMEOUT, true);
    esp_task_wdt_add(NULL);
    wdtStarted = true;
    Serial.println("[WDT] Started");
  }
}
void setStage(const char *stage) {
  if (stage == nullptr) {
    return;
  }
  strncpy(currentCriticalStage, stage, sizeof(currentCriticalStage) - 1);
  currentCriticalStage[sizeof(currentCriticalStage) - 1] = '\0';
  strncpy(previousResetStage, currentCriticalStage, sizeof(previousResetStage) - 1);
  previousResetStage[sizeof(previousResetStage) - 1] = '\0';
  previousResetStageValid = true;
  lastStage = currentCriticalStage;
}

void printHeartbeat() {
  static uint32_t lastHeartbeatMs = 0;
  if (millis() - lastHeartbeatMs < HEARTBEAT_PERIOD_MS) return;
  lastHeartbeatMs = millis();
  Serial.printf("[HB] ms=%lu stage=%s heap=%u sd=%d stream=%d batV=%.3f\n", 
                millis(), lastStage, ESP.getFreeHeap(), SDOK, streaming, batV);
}

void configureI2CBus() {
  setStage("i2c.begin");
  Wire.begin();
  Wire.setTimeOut(50);
  Wire.setClock(I2C_CLOCK_HZ);
  delay(50);
  Serial.printf("[I2C] Clock: %u Hz\n", I2C_CLOCK_HZ);
}

bool i2cDevicePresent(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

void enforceStationStartupConfig() {
  bool changed = false;

  if (!config.sdAutoMount) { config.sdAutoMount = true; changed = true; }
  if (config.sdSavePeriod != 180000UL) { config.sdSavePeriod = 180000UL; changed = true; }
  if (config.httpSendPeriod != 300000UL) { config.httpSendPeriod = 300000UL; changed = true; }
  if (!config.autostart) { config.autostart = true; changed = true; }
  if (config.autostartWaitGps) { config.autostartWaitGps = false; changed = true; }

  if (changed) {
    saveConfig();
    Serial.println("[CONFIG] Station startup enforced: autostart ON, HTTP 5min, SD 3min");
  }
}

// -------------------- OLED Helper --------------------
void oledStatus(const String &l1, const String &l2, const String &l3, const String &l4) {
  if (!oledOK) return;
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tf);
  u8g2.setCursor(0, 12); u8g2.print(l1);
  u8g2.setCursor(0, 26); u8g2.print(l2);
  u8g2.setCursor(0, 40); u8g2.print(l3);
  u8g2.setCursor(0, 54); u8g2.print(l4);
  u8g2.sendBuffer();
}

int bootIndexForStage(const String &stage) {
  if (stage == "OLED") return 0;
  if (stage == "SD") return 1;
  if (stage == "RTC") return 2;
  if (stage == "PLANTOWER") return 3;
  if (stage == "CALEFACTOR") return 4;
  return -1;
}

void renderBootWindow() {
  if (!oledOK) return;
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tf);
  u8g2.drawStr(0, 7, "REVISION INICIO");
  u8g2.drawHLine(0, 9, 128);
  u8g2.setFont(u8g2_font_4x6_tf);
  for (uint8_t i = 0; i < BOOT_ITEM_COUNT; ++i) {
    int y = 16 + (i * 6);
    u8g2.setCursor(0, y);
    u8g2.print(BOOT_ITEM_LABELS[i]);
    if (i == 4 && heaterVersionKnown) {
      u8g2.setCursor(92, y);
      u8g2.print(heaterFirmwareVersion);
    } else {
      u8g2.setCursor(96, y);
      u8g2.print(bootItemStatus[i]);
    }
  }
  u8g2.sendBuffer();
}

void bootStatus(const String &stage, const String &detail, bool ok) {
  const char *state = detail.startsWith("probando") ? "..." : (ok ? "OK" : "FAIL");
  Serial.printf("[BOOTCHK] %-12s %-4s %s\n", stage.c_str(), state, detail.c_str());

  int idx = bootIndexForStage(stage);
  if (idx >= 0) {
    strncpy(bootItemStatus[idx], state, sizeof(bootItemStatus[idx]) - 1);
    bootItemStatus[idx][sizeof(bootItemStatus[idx]) - 1] = '\0';
    renderBootWindow();
  }
}

// -------------------- AT Helpers --------------------
void atBegin(const String &cmd, const String &expect1, const String &expect2, uint32_t timeout_ms) {
  modem.stream.print("AT");
  modem.stream.println(cmd);
  at.active = true;
  at.expect1 = expect1;
  at.expect2 = expect2;
  at.resp = "";
  at.deadline = millis() + timeout_ms;
}

bool atTick(bool &done, bool &ok) {
  uint32_t t0 = millis();
  while (SerialAT.available()) {
    if ((millis() - t0) >= 20) { done = false; ok = false; return false; }
    String line = SerialAT.readStringUntil('\n');
    line.trim();
    if (line.isEmpty()) continue;
    if (line.charAt(0) == '$') { parseNMEA(line); continue; }
    if (!at.active) continue;
    if (at.resp.length() < 2048) { at.resp += line; at.resp += "\n"; }
    if (at.expect1.length() && line.indexOf(at.expect1) >= 0) { done = true; ok = true; at.active = false; return true; }
    if (at.expect2.length() && line.indexOf(at.expect2) >= 0) { done = true; ok = (at.expect2 == "OK"); at.active = false; return true; }
  }
  if (at.active && millis() > at.deadline) { done = true; ok = false; at.active = false; return true; }
  done = false; ok = false; return false;
}

bool atRun(const String &cmd, const String &expect1, const String &expect2, uint32_t timeout_ms) {
  atBegin(cmd, expect1, expect2, timeout_ms);
  bool done = false, ok = false;
  uint32_t hardDeadline = millis() + timeout_ms + 1000;
  while (!done && millis() < hardDeadline) { feedWdt(); if (atTick(done, ok)) break; delay(1); }
  at.active = false;
  return ok;
}

bool sendAtSync(const String &cmd, String &resp, uint32_t timeout_ms) {
  atBegin(cmd, "OK", "ERROR", timeout_ms);
  bool done = false, ok = false;
  uint32_t hardDeadline = millis() + timeout_ms + 1000;
  while (!done && millis() < hardDeadline) { feedWdt(); if (atTick(done, ok)) break; delay(1); }
  resp = at.resp;
  at.active = false;
  return ok;
}

void updateNetworkInfo() {
  String resp;
  if (sendAtSync("+COPS?", resp, 3000)) {
    int idx = resp.indexOf("+COPS:");
    if (idx >= 0) {
      int start = resp.indexOf('"', idx);
      int end = resp.indexOf('"', start + 1);
      if (start >= 0 && end > start) networkOperator = resp.substring(start + 1, end);
    }
  }
  if (sendAtSync("+CREG?", resp, 2000)) {
    if (resp.indexOf(",1") >= 0 || resp.indexOf(",5") >= 0) registrationStatus = "Registered";
    else registrationStatus = "NotRegistered";
  }
}

void refreshSignalQuality() {
  int newCsq = 99;
  for (uint8_t attempt = 0; attempt < 3 && newCsq == 99; ++attempt) {
    newCsq = modem.getSignalQuality();
    if (newCsq == 99) {
      feedWdt();
      delay(300);
    }
  }
  csq = newCsq;
  signalQuality = (csq >= 0 && csq <= 31) ? String(csq) : String("N/A");
  Serial.printf("[MODEM] Signal Quality: %s CSQ\n", signalQuality.c_str());
}

// -------------------- Telemetry Tx --------------------
String getIdsSensores(const String& deviceId) {
  if (deviceId == "1") return "1144,1144,1144,1144,1144,1145,1146,1146,1146,1146,1146,1200,1200,1200,1200,1200";
  if (deviceId == "2") return "1151,1151,1151,1151,1151,1152,1153,1153,1153,1153,1153,1201,1201,1201,1201,1201";
  if (deviceId == "3") return "1158,1158,1158,1158,1158,1159,1160,1160,1160,1160,1160,1202,1202,1202,1202,1202";
  if (deviceId == "4") return "1165,1165,1165,1165,1165,1166,1167,1167,1167,1167,1167,1203,1203,1203,1203,1203";
  return "";
}

String getStationName(const String& deviceId) {
  if (deviceId == "1") return "HIRI-AUCA-1";
  if (deviceId == "2") return "HIRI-AUCA-2";
  if (deviceId == "3") return "HIRI-AUCA-3";
  if (deviceId == "4") return "HIRI-AUCA-4";
  return "HIRI-AUCA-INVALIDA";
}

String buildCurrentMeasurementValues() {
  bool heaterDataValid = heaterConnected && heaterLastDataMs != 0;
  return
      safeGpsStr(gpsLat) + "," + safeGpsStr(gpsLon) + "," +
      safeIntStr(csq) + "," + safeNumberStr(gpsSpeedKmh) + "," +
      safeSatsStr(satellitesStr) + "," + safeFloatStr(batV) + "," +
      safeFloatStr(pmsTempC) + "," + safeFloatStr(pmsHum) + "," +
      safeUIntStr(PM1) + "," + safeUIntStr(PM25) + "," +
      safeUIntStr(PM10) + "," +
      (heaterDataValid ? safeFloatStr(heaterShtTemp) : missingUrlValue()) + "," +
      (heaterDataValid ? safeFloatStr(heaterShtHum) : missingUrlValue()) + "," +
      (heaterDataValid ? String(heaterRelayOn ? 1 : 0) : missingUrlValue()) + "," +
      (heaterDataValid ? safeFloatStr(heaterBmeTemp) : missingUrlValue()) + "," +
      (heaterDataValid ? safeFloatStr(heaterBmeHum) : missingUrlValue());
}

bool sendCurrentMeasurement() {
  setStage("sendCurrentMeasurement.build");
  String idsSensores = getIdsSensores(String(DEVICE_ID_STR));
  if (idsSensores == "") {
    Serial.println("[HTTP][ERR] DEVICE_ID_STR debe estar entre 1 y 4");
    return false;
  }

  String valores = buildCurrentMeasurementValues();
  lastHttpMeasurementValues = valores;

  String fullUrl = String(API_BASE) + "?idsSensores=" + idsSensores + "&idsVariables=" + GLOBAL_IDS_VARIABLES + "&valores=" + valores;
  Serial.println("[HTTP] GET " + fullUrl);

  setStage("sendCurrentMeasurement.httpGet");
  bool httpOk = httpGet_webhook(fullUrl);

  if (httpOk) {
    sendCounter++;
    prefs.begin("system", false); prefs.putUInt("sendCnt", sendCounter); prefs.end();
    return true;
  }
  saveFailedTransmission(fullUrl, "HTTP_FAIL");
  return false;
}

// -------------------- Sensor Refresh --------------------
void refreshSensors2s() {
  if (rtcOK) rtcTempC = rtc.getTemperature();
}

bool waitForModemAT() {
  uint32_t t0 = millis();
  uint32_t attempt = 0;
  while (millis() - t0 < MODEM_TESTAT_TOTAL_MS) {
    feedWdt(); attempt++;
    if (modem.testAT(MODEM_TESTAT_RETRY_MS)) return true;
    oledStatus("MODEM", "Retry", String(attempt));
    digitalWrite(MODEM_PWRKEY, HIGH); delay(300); digitalWrite(MODEM_PWRKEY, LOW); delay(700);
  }
  return false;
}

void IRAM_ATTR isr_btn2() {
  uint32_t now = millis();
  if (now - lastDebounceTime2 > BTN2_DEBOUNCE_MS) {
    lastDebounceTime2 = now;
    btn2ClickFlag = true;
  }
}

void handleButtonLogic() {
  static bool lastRawBtn1State = HIGH;
  static bool stableBtn1State = HIGH;
  bool currentBtn1State = digitalRead(BUTTON_PIN_1);

  if (currentBtn1State != lastRawBtn1State) {
    lastDebounceTime1 = millis();
  }
  lastRawBtn1State = currentBtn1State;

  if ((millis() - lastDebounceTime1) > BTN1_DEBOUNCE_MS) {
    if (currentBtn1State != stableBtn1State) {
      stableBtn1State = currentBtn1State;
      if (stableBtn1State == LOW) {
        ui_btn1_click();
      }
    }
  }

  if (btn2ClickFlag) {
    btn2ClickFlag = false;
    ui_btn2_click();
  }
}

// -------------------- SETUP --------------------
void setup() {
 pinMode(I2C_POWER_PIN, OUTPUT);
digitalWrite(I2C_POWER_PIN,HIGH);
delay(300);
 Serial.begin(115200);
  startWdtOnce(); setStage("setup.start");
delay(300);
  configureI2CBus();
  pixels.begin(); pixels.setPixelColor(0, pixels.Color(0, 50, 100)); pixels.show();
  delay(300);
  Serial.println("\n[BOOT] FirmwarePro " + VERSION);
  checkRebootReason();

  prefs.begin("system", false);
  sendCounter = prefs.getUInt("sendCnt", 0); csvFileName = prefs.getString("csvFile", "");
  wasStreamingBeforeBoot = prefs.getBool("streaming", false);
  prefs.end();

  loadConfig();
  enforceStationStartupConfig();
  applyLEDConfig();
  
  spiSD.begin(SD_SCLK, SD_MISO, SD_MOSI, SD_CS);
  SDOK = SD.begin(SD_CS, spiSD);
  if (SDOK) {
    csvFileName = generateCSVFileName();
    writeCSVHeader();
    prefs.begin("system", false); prefs.putString("csvFile", csvFileName); prefs.end();
  }
  logFilePath = String("/errors_h") + String(DEVICE_ID_STR) + String(".csv");
  failedTxPath = String("/failed_h") + String(DEVICE_ID_STR) + String(".csv");
  AP_SSID_STR = getStationName(String(DEVICE_ID_STR));

  oledOK = i2cDevicePresent(0x3C) || i2cDevicePresent(0x3D);
  if (oledOK) {
    u8g2.begin();
    u8g2.setDisplayRotation(config.rotateDisplay ? U8G2_R0 : U8G2_R2);
  }
  lastOledActivity = millis();

  // Animation
  if (oledOK) {
    while (logoXOffset < LOGO_FINAL_X || hiriXOffset > HIRI_FINAL_X || proYOffset > PRO_FINAL_Y) {
      feedWdt();
      if (logoXOffset < LOGO_FINAL_X) logoXOffset += 4;
      if (hiriXOffset > HIRI_FINAL_X) hiriXOffset -= 4;
      if (proYOffset > PRO_FINAL_Y) proYOffset -= 1;
      drawAnimation(); delay(20);
    }
  } else {
    Serial.println("[OLED] Animation skipped: OLED not detected");
  }
  bootStatus("OLED", oledOK ? "SSD1306 detectado" : "sin respuesta I2C", oledOK);

  pms.begin(9600);
  bootStatus("PLANTOWER", "UART lista", true);
  Serial2.begin(9600, SERIAL_8N1, Serial2RX_PIN, Serial2TX_PIN);
  // VERSION es opcional: se consulta sin esperar respuesta para mantener
  // compatibilidad con calefactores que usan firmware antiguo.
  bootStatus("CALEFACTOR", "UART lista", true);
  heaterRequestFirmwareVersion();

  if (rtc.begin()) { rtcOK = true; }
  bootStatus("RTC", rtcOK ? "DS3231 detectado" : "DS3231 no responde", rtcOK);
  
  
  bootStatus("SD", SDOK ? "SD disponible" : "SD no montada", SDOK);

  if (oledOK) {
    renderBootWindow();
    uint32_t reviewStartedMs = millis();
    bool versionWasShown = heaterVersionKnown;
    while (millis() - reviewStartedMs < BOOT_REVIEW_HOLD_MS) {
      pollHeaterSerial();
      if (!versionWasShown && heaterVersionKnown) {
        versionWasShown = true;
        renderBootWindow();
      }
      feedWdt();
      delay(10);
    }
  }

  pinMode(BUTTON_PIN_1, INPUT_PULLUP);
  if (BUTTON_PIN_2 >= 0) {
    pinMode(BUTTON_PIN_2, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(BUTTON_PIN_2), isr_btn2, RISING);
  }
  bootStatus("BTN", String("B1=") + BUTTON_PIN_1, true);

  setStage("modem.boot");
  SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
  pinMode(MODEM_PWRKEY, OUTPUT); pinMode(MODEM_FLIGHT, OUTPUT); digitalWrite(MODEM_FLIGHT, HIGH);
  
  oledStatus("MODEM", "Starting...");
  if (waitForModemAT()) {
    atRun("+CEDRXS=0", "OK", "ERROR", 1500);
    atRun("+CPSMS=0", "OK", "ERROR", 1500);
    oledStatus("NET", "Attach...");
    if (modem.waitForNetwork(60000)) {
      if (modem.gprsConnect(apn, gprsUser, gprsPass)) oledStatus("NET", "OK");
    }
    refreshSignalQuality();
    updateNetworkInfo();
    detectAndEnableXtra();
  }
  
  gnssBringUp();
  if (config.autostart) {
    streaming = true;
    loggingEnabled = SDOK;
    lastHttpSend = millis() - config.httpSendPeriod;
    lastSdSave = millis();
    Serial.println("[BOOT] Autostart enabled: immediate HTTP, SD every 3 min");
  }
  debugScreenIndex = 0;
  lastDebugRotationMs = millis();
  displayState = DISP_NORMAL;
  setStage("setup.done");
}

// -------------------- LOOP --------------------
void loop() {
  feedWdt(); setStage("loop.start"); printHeartbeat();
  handleButtonLogic();

  if (wifiModeActive) {
    dnsServer.processNextRequest(); server.handleClient();
    if (haveFix) gnssWatchdog();
    return;
  }

  gnssWatchdog(); gnssDiagTick(); gnssDebugPollAsync();

  static uint32_t lastSensorUpdateMs = 0;
  if (millis() - lastSensorUpdateMs >= 2000) {
    lastSensorUpdateMs = millis();
    refreshSensors2s();
  }

  static uint32_t lastSignalUpdateMs = 0;
  if (millis() - lastSignalUpdateMs >= 30000UL) {
    lastSignalUpdateMs = millis();
    refreshSignalQuality();
  }

  readPMS();
  static uint32_t lastLedUpdateMs = 0;
  if (millis() - lastLedUpdateMs >= 300) { lastLedUpdateMs = millis(); updatePmLed((float)PM25); }

  pollHeaterSerial();

  static uint32_t lastAtTick = 0;
  if (millis() - lastAtTick >= 50) {
    lastAtTick = millis(); bool d, o; (void)atTick(d, o);
  }

  if (millis() - lastBatSample >= BAT_SAMPLE_INTERVAL_MS) {
    lastBatSample = millis(); batSampleSum += analogRead(BAT_PIN); batSampleCount++;
    if (batSampleCount >= NUM_SAMPLES) {
      batV = (batSampleSum / NUM_SAMPLES / 4095.0f) * 3.3f * 2.0f * 1.15f;
      batSampleSum = 0; batSampleCount = 0;
    }
  }

  static uint32_t lastDisplayUpdate = 0;
  if (millis() - lastDisplayUpdate > 60) { lastDisplayUpdate = millis(); renderDisplay(); }

  if (loggingEnabled && (millis() - lastSdSave >= config.sdSavePeriod)) {
    lastSdSave = millis();
    String connectionStatus =
        !hasHttpAttempted ? "SIN_INTENTO" :
        (lastHttpOk ? "ULTIMO_HTTP_OK" : "ULTIMO_HTTP_FALLO");
    lastSdOk =
        saveMeasurementToSD(buildCurrentMeasurementValues(), connectionStatus);
    lastTxPayloadSdOk = lastSdOk;
    lastSdActivityMs = millis();
  }

  if (streaming && (millis() - lastHttpSend >= config.httpSendPeriod)) {
    lastHttpSend = millis(); Serial.println("[HTTP] Send");
    lastHttpOk = sendCurrentMeasurement();
    hasHttpAttempted = true;
    lastHttpActivityMs = millis();
  }

  processSerialCommand();
}
