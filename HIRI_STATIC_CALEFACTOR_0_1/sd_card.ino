// -------------------- SD helpers --------------------
#include "config.h"
extern SystemConfig config;
extern float pmsTempC, pmsHum, rtcTempC, batV;
extern float heaterShtTemp, heaterShtHum, heaterBmeTemp, heaterBmeHum;
extern bool heaterRelayOn;
extern uint16_t PM1, PM25, PM10;
extern String gpsDate, gpsLat, gpsLon, gpsAlt, gpsSpeedKmh, satellitesStr, hdopStr, rebootReason;
extern bool rtcOK, SDOK, loggingEnabled, xtraLastOk;
extern uint32_t sdSaveCounter, sendCounter;
extern String csvFileName, logFilePath;
extern uint8_t lastDayLogged;

bool splitMeasurementValues(const String &values, String fields[16]) {
  int start = 0;
  for (uint8_t i = 0; i < 16; ++i) {
    int comma = values.indexOf(',', start);
    if (i < 15) {
      if (comma < 0) return false;
      fields[i] = values.substring(start, comma);
      start = comma + 1;
    } else {
      if (comma >= 0) return false;
      fields[i] = values.substring(start);
    }
  }
  return true;
}

// Guarda los mismos 16 valores disponibles para HTTP, reordenados para lectura humana.
bool saveMeasurementToSD(const String &values, const String &connectionStatus) {
  if (!SDOK) return false;

  DateTime now = rtcOK ? rtc.now() : DateTime(2000, 1, 1, 0, 0, 0);
  String expectedFileName = generateCSVFileName();
  if (csvFileName != expectedFileName) {
    csvFileName = expectedFileName;
    writeCSVHeader();
    prefs.begin("system", false);
    prefs.putString("csvFile", csvFileName);
    prefs.end();
  }

  String fields[16];
  if (!splitMeasurementValues(values, fields)) {
    Serial.println("[SD][ERR] El payload HTTP no contiene exactamente 16 valores");
    return false;
  }

  File file = SD.open(csvFileName, FILE_APPEND);
  if (!file) {
    Serial.println("[SD][ERR] No se pudo abrir " + csvFileName);
    return false;
  }

  if (file.size() == 0) {
    file.println("timestamp,pm_1_0_ug_m3,pm_2_5_ug_m3,pm_10_ug_m3,"
                 "temp_plantower_c,hum_plantower_pct,"
                 "temp_interna_calefactor_bme_c,hum_interna_calefactor_bme_pct,"
                 "temp_externa_calefactor_sht40_c,hum_externa_calefactor_sht40_pct,"
                 "relay_calefactor,senal_csq,conexion_http,"
                 "latitud,longitud,velocidad_kmh,satelites,voltaje_v");
  }

  char timestamp[20];
  snprintf(timestamp, sizeof(timestamp), "%04d-%02d-%02d %02d:%02d:%02d",
           now.year(), now.month(), now.day(),
           now.hour(), now.minute(), now.second());

  // Payload HTTP:
  // 0 lat, 1 lon, 2 CSQ, 3 velocidad, 4 satélites, 5 voltaje,
  // 6/7 T/RH Plantower, 8/9/10 PM, 11/12 SHT40, 13 relé, 14/15 BME.
  String line = String(timestamp) + "," +
                fields[8] + "," + fields[9] + "," + fields[10] + "," +
                fields[6] + "," + fields[7] + "," +
                fields[14] + "," + fields[15] + "," +
                fields[11] + "," + fields[12] + "," + fields[13] + "," +
                fields[2] + "," + connectionStatus + "," +
                fields[0] + "," + fields[1] + "," + fields[3] + "," +
                fields[4] + "," + fields[5];
  file.println(line);
  file.close();

  sdSaveCounter++;
  lastSavedCSVLine = line;
  Serial.println("[SD][TX] Guardado payload HTTP: " + line);
  return true;
}

// Genera nombre diario de CSV usando prefijo de dispositivo + fecha RTC.
// Permite rotación por día y continuidad de trazabilidad en terreno.
String generateCSVFileName() {
  DateTime now = rtcOK ? rtc.now() : DateTime(2000, 1, 1, 0, 0, 0);
  char name[40];
  snprintf(name, sizeof(name), "/HIRI_AUCA_%s_%02d_%02d_%04d.csv",
           DEVICE_ID_STR, now.day(), now.month(), now.year());
  Serial.print("[SD] CSV filename: ");
  Serial.println(name);
  return String(name);
}

// Escribe cabecera CSV solo si el archivo está vacío.
// Estandariza columnas para procesamiento posterior en backend/IA.
void writeCSVHeader() {
  if (!SDOK)
    return;
  File f = SD.open(csvFileName, FILE_APPEND);
  if (f) {
    if (f.size() == 0) { // Only write header if file is empty
      f.println("timestamp,pm_1_0_ug_m3,pm_2_5_ug_m3,pm_10_ug_m3,"
                "temp_plantower_c,hum_plantower_pct,"
                "temp_interna_calefactor_bme_c,hum_interna_calefactor_bme_pct,"
                "temp_externa_calefactor_sht40_c,hum_externa_calefactor_sht40_pct,"
                "relay_calefactor,senal_csq,conexion_http,"
                "latitud,longitud,velocidad_kmh,satelites,voltaje_v");
      Serial.println("[SD] Wrote header to " + csvFileName);
    }
    f.close();
  } else {
    Serial.println("[SD][ERR] Failed to open " + csvFileName +
                   " to write header.");
  }
}

// Crea archivo de errores con esquema fijo si aún no existe.
// Facilita auditoría de fallos de red/módem en campo.
void writeErrorLogHeader() {
  if (!SDOK)
    return;
  if (SD.exists(logFilePath.c_str()))
    return; // Ya existe
  File f = SD.open(logFilePath.c_str(), FILE_WRITE);
  if (f) {
    f.println("timestamp,type,context,message");
    f.close();
    Serial.println("[SD] Error log header created");
  }
}

#if 0 // Formato CSV antiguo: reemplazado por saveMeasurementToSD().
// Variable global para almacenar la última línea guardada (para visualización
// en OLED)
extern String lastSavedCSVLine;

extern String currentNote;

// Guarda una muestra completa de telemetría en SD y rota por cambio de día.
// Actualiza contadores y expone última línea guardada para UI/debug.
bool saveCSVData() {
  if (!SDOK || !loggingEnabled)
    return false;
  DateTime now = rtcOK ? rtc.now() : DateTime(2000, 1, 1, 0, 0, 0);

  // Detectar cambio de día (muy eficiente: 1 ciclo CPU)
  if (now.day() != lastDayLogged) {
    Serial.println("[SD] Day changed, creating new file...");
    csvFileName = generateCSVFileName();
    writeCSVHeader();
    lastDayLogged = now.day();

    // Persist new filename
    prefs.begin("system", false);
    prefs.putString("csvFile", csvFileName);
    prefs.end();
  }

  char hhmmss[9];
  snprintf(hhmmss, sizeof(hhmmss), "%02d:%02d:%02d", now.hour(), now.minute(),
           now.second());
  String line = String(millis()) + "," + hhmmss + "," + gpsDate + "," + gpsLat +
                "," + gpsLon + "," + gpsAlt + "," + gpsSpeedKmh + "," +
                String(PM1) + "," + String(PM25) + "," + String(PM10) + "," +
                (isnan(pmsTempC) ? "0" : String(pmsTempC, 1)) + "," +
                (isnan(pmsHum) ? "0" : String(pmsHum, 1)) + "," +
                String(rtcTempC, 2) + "," + String(batV, 2) + "," +
                String(csq) + "," + satellitesStr + "," + hdopStr + "," +
                (xtraLastOk ? "1" : "0") + "," +
                (isnan(heaterShtTemp) ? "-1" : String(heaterShtTemp, 2)) + "," +
                (isnan(heaterShtHum) ? "-1" : String(heaterShtHum, 2)) + "," +
                String(heaterRelayOn ? 1 : 0) + "," +
                (isnan(heaterBmeTemp) ? "-1" : String(heaterBmeTemp, 2)) + "," +
                (isnan(heaterBmeHum) ? "-1" : String(heaterBmeHum, 2)) + "," +
                rebootReason + "," + currentNote;

  File f = SD.open(csvFileName, FILE_APPEND);
  if (f) {
    f.println(line);
    f.close();
    sdSaveCounter++;         // Incrementar contador de guardados exitosos en SD
    lastSavedCSVLine = line; // Guardar línea para visualización en OLED
    Serial.println(String("[SD] Saved line: ") + line);
    
    // Clear one-shot note after writing
    currentNote = ""; 
    
    return true;
  } else {
    Serial.println("[SD][ERR] Failed to open " + csvFileName +
                   " for appending.");
    return false;
  }
}
#endif

// -------------------- Failed Transmission Log (Debug only)
// -------------------- Esta función guarda las transmisiones HTTP fallidas para
// análisis posterior NO se reintenta la transmisión automáticamente para evitar
// desfase de datos Variables globales ajustables para pruebas:
// - failedTxPath: Ruta del archivo CSV (definido en .ino principal)
// Registra en SD las transmisiones HTTP fallidas con timestamp y URL.
// No reintenta en línea para evitar desfases y preservar ciclo de muestreo.
void saveFailedTransmission(const String &url, const String &errorType) {
  if (!SDOK)
    return; // SD no disponible

  DateTime now = rtc.now();
  char timestamp[20];
  snprintf(timestamp, sizeof(timestamp), "%04d-%02d-%02d %02d:%02d:%02d",
           now.year(), now.month(), now.day(), now.hour(), now.minute(),
           now.second());

  // Verificar si necesitamos crear el header (primera vez)
  bool needsHeader = !SD.exists(failedTxPath.c_str());

  File f = SD.open(failedTxPath.c_str(), FILE_APPEND);
  if (f) {
    // Escribir header si es la primera línea
    if (needsHeader) {
      f.println("timestamp,error_type,url");
      Serial.println("[FAILED_TX] Created header in " + failedTxPath);
    }

    // Escribir línea de fallo
    // Formato: timestamp,error_type,url (URL entre comillas por si tiene comas)
    String line = String(timestamp) + "," + errorType + ",\"" + url + "\"";
    f.println(line);
    f.close();

    Serial.println("[FAILED_TX] Logged: " + errorType);
  } else {
    Serial.println("[FAILED_TX][ERR] Could not open " + failedTxPath);
  }
}

