// Enlace serial HIRI <-> calefactor. Tramas ASCII, 9600 8N1:
// CALEF,1,HELLO,version*HH (tambien acepta el ID historico CAL)
// CALEF,1,VERSION,version*HH
// CALEF,1,DATA,extTemp,shtTemp,bmeTemp,shtHum,bmeHum,thermo,relay,byHum,byTemp,emergency*HH
// HIRI,1,HELLO*HH, HIRI,1,VERSION*HH, HIRI,1,TEMP,valor*HH y
// HIRI,1,CAL,segundos*HH

static String heaterRxLine;
static uint32_t heaterLastHelloMs = 0;
static uint32_t heaterLastTempTxMs = 0;
static uint32_t heaterLastVersionRequestMs = 0;
static uint32_t heaterVersionQueryStartedMs = 0;
static bool heaterVersionQueryActive = false;
static const uint32_t HEATER_HELLO_INTERVAL_MS = 5000;
static const uint32_t HEATER_TEMP_INTERVAL_MS = 2000;
static const uint32_t HEATER_VERSION_RETRY_MS = 30000;
static const uint32_t HEATER_VERSION_QUERY_WINDOW_MS = 3UL * 60UL * 1000UL;
static const uint32_t HEATER_STALE_MS = 5000;
static const uint16_t HEATER_MAX_TEST_SECONDS = 300;

bool heaterDeviceIdIsValid(const String &deviceId) {
  return deviceId == "CALEF" || deviceId == "CAL";
}

uint8_t heaterChecksum(const String &payload) {
  uint8_t sum = 0;
  for (size_t i = 0; i < payload.length(); ++i) sum += (uint8_t)payload[i];
  return sum;
}

bool heaterFrameIsValid(const String &frame, String &payload) {
  int star = frame.lastIndexOf('*');
  if (star <= 0 || star + 3 != frame.length()) return false;
  payload = frame.substring(0, star);
  char expected[3];
  snprintf(expected, sizeof(expected), "%02X", heaterChecksum(payload));
  String received = frame.substring(star + 1);
  received.toUpperCase();
  return received == expected;
}

void heaterSendFrame(const String &payload) {
  char checksum[4];
  snprintf(checksum, sizeof(checksum), "*%02X", heaterChecksum(payload));
  Serial2.print(payload);
  Serial2.println(checksum);
  Serial.println("[CALEFACTOR][TX] " + payload + checksum);
}

bool heaterRequestTimedTest(const String &secondsText) {
  if (secondsText.length() == 0) return false;

  for (size_t i = 0; i < secondsText.length(); ++i) {
    if (!isDigit(secondsText[i])) return false;
  }

  unsigned long seconds = secondsText.toInt();
  if (seconds == 0 || seconds > HEATER_MAX_TEST_SECONDS) return false;

  heaterSendFrame("HIRI,1,CAL," + String(seconds));
  Serial.printf("[CALEFACTOR] prueba solicitada: %lu s\n", seconds);
  return true;
}

bool heaterParseData(const String &payload) {
  // Se esperan el ID, version de protocolo, DATA y 10 valores posteriores.
  const uint8_t expectedFields = 13;
  String fields[13];
  int begin = 0;
  for (uint8_t i = 0; i < expectedFields; ++i) {
    int comma = payload.indexOf(',', begin);
    if (comma < 0) {
      fields[i] = payload.substring(begin);
      begin = payload.length();
    } else {
      fields[i] = payload.substring(begin, comma);
      begin = comma + 1;
    }
  }
  if (!heaterDeviceIdIsValid(fields[0]) || fields[1] != "1" ||
      fields[2] != "DATA") {
    return false;
  }

  heaterExternalTemp = fields[3].toFloat();
  heaterShtTemp = fields[4].toFloat();
  heaterBmeTemp = fields[5].toFloat();
  heaterShtHum = fields[6].toFloat();
  heaterBmeHum = fields[7].toFloat();
  heaterThermoTemp = fields[8].toFloat();
  heaterRelayOn = fields[9].toInt() != 0;
  heaterByHumidity = fields[10].toInt() != 0;
  heaterByTemperature = fields[11].toInt() != 0;
  heaterEmergencyMode = fields[12].toInt() != 0;
  heaterLastDataMs = millis();
  heaterConnected = true;
  Serial.printf("[CALEFACTOR][RX] SHT=%.1fC/%.1f%% BME=%.1fC/%.1f%% TERM=%.1fC REL=%d H=%d T=%d E=%d\n",
                heaterShtTemp, heaterShtHum, heaterBmeTemp, heaterBmeHum,
                heaterThermoTemp, heaterRelayOn, heaterByHumidity,
                heaterByTemperature, heaterEmergencyMode);
  return true;
}

void heaterProcessFrame(String frame) {
  frame.trim();
  if (frame.length() == 0) return;
  Serial.println("[CALEFACTOR][RX RAW] " + frame);
  String payload;
  if (!heaterFrameIsValid(frame, payload)) {
    Serial.println("[CALEFACTOR] trama/checksum invalido");
    return;
  }
  int firstComma = payload.indexOf(',');
  String deviceId = firstComma > 0 ? payload.substring(0, firstComma) : "";
  String message = firstComma > 0 ? payload.substring(firstComma + 1) : "";

  bool isHello = message == "1,HELLO" || message.startsWith("1,HELLO,");
  if (heaterDeviceIdIsValid(deviceId) && isHello) {
    heaterConnected = true;
    String helloVersion = message.startsWith("1,HELLO,")
                              ? message.substring(String("1,HELLO,").length())
                              : "";
    if (helloVersion.length() > 0) {
      heaterFirmwareVersion = helloVersion;
      heaterVersionKnown = true;
      heaterVersionQueryActive = false;
    }
    Serial.println("[CALEFACTOR] conectado: " + payload);
  } else if (heaterDeviceIdIsValid(deviceId) &&
             message.startsWith("1,VERSION,")) {
    String receivedVersion =
        message.substring(String("1,VERSION,").length());
    if (receivedVersion.length() == 0) {
      Serial.println("[CALEFACTOR] respuesta VERSION vacia");
      return;
    }
    heaterFirmwareVersion = receivedVersion;
    heaterVersionKnown = true;
    heaterVersionQueryActive = false;
    heaterConnected = true;
    Serial.println("[CALEFACTOR] firmware: " + heaterFirmwareVersion);
  } else if (heaterParseData(payload)) {
    // Los valores ya fueron impresos por heaterParseData().
  } else {
    Serial.println("[CALEFACTOR] trama valida, pero ID/tipo no reconocido");
  }
}

void heaterReadAvailableFrames() {
  while (Serial2.available() > 0) {
    char c = (char)Serial2.read();
    if (c == '\n') {
      heaterProcessFrame(heaterRxLine);
      heaterRxLine = "";
    } else if (c != '\r') {
      if (heaterRxLine.length() < 180) heaterRxLine += c;
      else heaterRxLine = ""; // descarta una trama demasiado larga
    }
  }
}

void heaterRequestFirmwareVersion() {
  uint32_t now = millis();
  if (!heaterVersionQueryActive) {
    heaterVersionQueryActive = true;
    heaterVersionQueryStartedMs = now;
  }
  heaterSendFrame("HIRI,1,VERSION");
  heaterLastVersionRequestMs = now;
}

void pollHeaterSerial() {
  heaterReadAvailableFrames();

  uint32_t now = millis();
  if (now - heaterLastHelloMs >= HEATER_HELLO_INTERVAL_MS) {
    heaterSendFrame("HIRI,1,HELLO");
    heaterLastHelloMs = now;
  }
  if (!heaterVersionKnown && heaterVersionQueryActive) {
    if (now - heaterVersionQueryStartedMs >= HEATER_VERSION_QUERY_WINDOW_MS) {
      heaterVersionQueryActive = false;
      Serial.println("[CALEFACTOR] VERSION no disponible; se detienen consultas");
    } else if (now - heaterLastVersionRequestMs >= HEATER_VERSION_RETRY_MS) {
      heaterRequestFirmwareVersion();
    }
  }
  if (now - heaterLastTempTxMs >= HEATER_TEMP_INTERVAL_MS &&
      !isnan(heaterShtTemp)) {
    heaterSendFrame("HIRI,1,TEMP," + String(heaterShtTemp, 1));
    heaterLastTempTxMs = now;
  }
  if (heaterLastDataMs != 0 && now - heaterLastDataMs > HEATER_STALE_MS) {
    if (heaterConnected) Serial.println("[CALEFACTOR] timeout: sin DATA por 5 s");
    heaterConnected = false;
  }
}
