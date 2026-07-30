/*
  Calefactor V2 - Firmware 1.0.0
  Creacion original: Alejandro Rebolledo <arebolledo@udd.cl>
  SPDX-License-Identifier: CC-BY-NC-4.0

  Cambios principales frente a Calefactor_bmp_dht22_serialcheck_v3:
  - Reemplaza DHT22 por SHT40.
  - Activa calefactor por humedad relativa alta: RH_ON = 65%, RH_OFF = 60%.
  - Mantiene proteccion por termocupla MAX6675.
  - Evita bloqueo permanente en emergencia.
  - Agrega estado de rele y banderas de control en la salida CSV.

  Salida CSV:
  tempsens,shtTemp,tempBME,shtHum,humBME,tempThermo,relayOn,heaterByHumidity,heaterByTemperature,emergencyMode
*/

#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <Adafruit_SHT4x.h>
#include "MAX6675.h"
#include <Ticker.h>
#include <SoftwareSerial.h>

#define externo_TX D4
#define externo_RX D3
SoftwareSerial externo(externo_RX, externo_TX);  // RX, TX

#define SEALEVELPRESSURE_HPA (1013.25)
#define relay D0

const int dataPin = D6;    // SO
const int clockPin = D5;   // SCK
const int selectPin = D8;  // CS

const unsigned long restartInterval = 4UL * 60UL * 60UL * 1000UL;  // 4 horas
const unsigned long watchdogTimeout = 30000UL;                     // 30 segundos
const unsigned long sensorInterval = 1000UL;                       // 1 segundo

const float tempMinima = 15.0;
const float maxcontrol = 70.0;
const float maxemergencia = maxcontrol + 10.0;

// La literatura reporta inicio del efecto higroscopico en PMS5003 cerca de 65-70% RH.
const float RH_ON = 65.0;
const float RH_OFF = 60.0;
const float RH_ALTA = 70.0;

Ticker ticker;
MAX6675 thermoCouple(selectPin, dataPin, clockPin);
Adafruit_BME280 bme;
Adafruit_SHT4x sht4 = Adafruit_SHT4x();

String numberString;

float tempThermo = NAN;
float tempBME = NAN;
float humBME = NAN;
float shtTemp = NAN;
float shtHum = NAN;
float tempsens = 16.0;

bool emergencyMode = false;
bool statusBME = true;
bool statusSHT40 = true;
bool heaterByHumidity = false;
bool heaterByTemperature = false;
bool heaterByManualTest = false;
bool relayOn = false;

unsigned long lastSensorRead = 0;
unsigned long manualHeatUntilMs = 0;

// Límite de una prueba remota; no reemplaza la protección de 70 C.
const uint16_t MAX_MANUAL_HEAT_SECONDS = 300;

const char *CAL_DEVICE_ID = "CALEF";
// Version semantica del firmware. Esta es la primera version completa.
// Cambiar solamente esta constante al publicar una nueva version.
const char *CAL_FIRMWARE_VERSION = "1.0.0";

uint8_t frameChecksum(const String &payload) {
  uint8_t sum = 0;
  for (size_t i = 0; i < payload.length(); ++i) sum += (uint8_t)payload[i];
  return sum;
}

void sendFrame(const String &payload) {
  char checksum[4];
  snprintf(checksum, sizeof(checksum), "*%02X", frameChecksum(payload));
  externo.print(payload);
  externo.println(checksum);
}

bool decodeFrame(const String &frame, String &payload) {
  int star = frame.lastIndexOf('*');
  if (star <= 0 || star + 3 != frame.length()) return false;
  payload = frame.substring(0, star);
  char expected[3];
  snprintf(expected, sizeof(expected), "%02X", frameChecksum(payload));
  String received = frame.substring(star + 1);
  received.toUpperCase();
  return received == expected;
}

void setup() {
  ESP.wdtDisable();
  ESP.wdtEnable(watchdogTimeout);

  Serial.begin(115200);
  externo.begin(9600);
  sendFrame(String(CAL_DEVICE_ID) + ",1,HELLO," + CAL_FIRMWARE_VERSION);
  Wire.begin();
  SPI.begin();

  pinMode(relay, OUTPUT);
  digitalWrite(relay, LOW);

  thermoCouple.begin();
  thermoCouple.setSPIspeed(4000000);

  Serial.printf("Calefactor V2 - Firmware %s - BME280 + SHT40 + MAX6675\n",
                CAL_FIRMWARE_VERSION);

  if (!bme.begin(0x76)) {
    Serial.println(F("No se encontro BME280 en 0x76. Se usara SHT40 como referencia principal."));
    statusBME = false;
  }

  if (!sht4.begin()) {
    Serial.println(F("No se encontro SHT40. Revisar conexion I2C."));
    statusSHT40 = false;
  } else {
    sht4.setPrecision(SHT4X_HIGH_PRECISION);
    sht4.setHeater(SHT4X_NO_HEATER);
  }

  ticker.attach_ms(restartInterval, restartESP);
}

void loop() {
  ESP.wdtFeed();

  readExternalTemperature();

  if (millis() - lastSensorRead >= sensorInterval) {
    lastSensorRead = millis();
    readSensors();
    updateControl();
    printValues();
  }
}

void readExternalTemperature() {
  if (externo.available() <= 0) {
    return;
  }

  String receivedData = externo.readStringUntil('\n');
  receivedData.trim();
  String payload;
  if (!decodeFrame(receivedData, payload)) {
    Serial.println(F("Trama HIRI invalida."));
    return;
  }
  if (payload == "HIRI,1,HELLO") {
    sendFrame(String(CAL_DEVICE_ID) + ",1,HELLO," + CAL_FIRMWARE_VERSION);
  } else if (payload == "HIRI,1,VERSION") {
    sendFrame(String(CAL_DEVICE_ID) + ",1,VERSION," + CAL_FIRMWARE_VERSION);
  } else if (payload.startsWith("HIRI,1,TEMP,")) {
    float temp = payload.substring(String("HIRI,1,TEMP,").length()).toFloat();
    if (isValidReading(temp)) tempsens = temp;
  } else if (payload.startsWith("HIRI,1,CAL,")) {
    startTimedManualHeating(payload.substring(String("HIRI,1,CAL,").length()));
  }
}

void startTimedManualHeating(const String &secondsText) {
  if (secondsText.length() == 0) {
    Serial.println(F("[CAL] Prueba rechazada: duracion vacia."));
    return;
  }

  for (size_t i = 0; i < secondsText.length(); ++i) {
    if (!isDigit(secondsText[i])) {
      Serial.println(F("[CAL] Prueba rechazada: duracion invalida."));
      return;
    }
  }

  unsigned long seconds = secondsText.toInt();
  bool thermoValid = isValidReading(tempThermo) && tempThermo > 0.0;
  if (seconds == 0 || seconds > MAX_MANUAL_HEAT_SECONDS) {
    Serial.println(F("[CAL] Prueba rechazada: duracion fuera de rango (1-300 s)."));
  } else if (!thermoValid || tempThermo >= maxcontrol || emergencyMode) {
    Serial.println(F("[CAL] Prueba rechazada: termocupla invalida, temperatura >= 70 C o emergencia."));
  } else {
    heaterByManualTest = true;
    manualHeatUntilMs = millis() + seconds * 1000UL;
    Serial.printf("[CAL] Prueba manual activada por %lu s (corte a %.1f C).\n", seconds, maxcontrol);
  }
}

void readSensors() {
  int status = thermoCouple.read();
  tempThermo = thermoCouple.getTemperature();

  if (statusBME) {
    tempBME = bme.readTemperature();
    humBME = bme.readHumidity();
    if (!isValidReading(tempBME) || !isValidReading(humBME)) {
      statusBME = false;
      tempBME = NAN;
      humBME = NAN;
    }
  }

  if (statusSHT40) {
    sensors_event_t humidity;
    sensors_event_t temp;
    if (sht4.getEvent(&humidity, &temp)) {
      shtTemp = temp.temperature;
      shtHum = humidity.relative_humidity;
    } else {
      statusSHT40 = false;
      shtTemp = NAN;
      shtHum = NAN;
    }
  }
}

void updateControl() {
  bool thermoValid = isValidReading(tempThermo) && tempThermo > 0.0;

  if (heaterByManualTest && (int32_t)(millis() - manualHeatUntilMs) >= 0) {
    heaterByManualTest = false;
    Serial.println(F("[CAL] Prueba manual finalizada por tiempo."));
  }

  if (!thermoValid || tempThermo >= maxcontrol) {
    setRelay(false);
    heaterByHumidity = false;
    heaterByTemperature = false;
    if (heaterByManualTest) {
      heaterByManualTest = false;
      Serial.println(F("[CAL] Prueba manual detenida por proteccion termica."));
    }
    if (thermoValid && tempThermo >= maxemergencia) {
      emergencyMode = true;
    }
    return;
  }

  if (emergencyMode) {
    setRelay(false);
    heaterByManualTest = false;
    if (tempThermo < maxcontrol) {
      emergencyMode = false;
    }
    return;
  }

  updateHumidityControl();
  updateTemperatureControl();

  setRelay(heaterByHumidity || heaterByTemperature || heaterByManualTest);
}

void updateHumidityControl() {
  float rhControl = getControlHumidity();

  if (!isValidReading(rhControl)) {
    heaterByHumidity = false;
    return;
  }

  if (rhControl >= RH_ON) {
    heaterByHumidity = true;
  } else if (rhControl <= RH_OFF) {
    heaterByHumidity = false;
  }
}

void updateTemperatureControl() {
  bool tempBajaExterna = isValidReading(tempsens) && tempsens < tempMinima;
  bool tempBajaSHT = isValidReading(shtTemp) && shtTemp < tempMinima;
  bool tempBajaBME = isValidReading(tempBME) && tempBME < tempMinima;

  heaterByTemperature = tempBajaExterna || tempBajaSHT || tempBajaBME;
}

float getControlHumidity() {
  if (isValidReading(shtHum)) {
    return shtHum;
  }
  if (isValidReading(humBME)) {
    return humBME;
  }
  return NAN;
}

bool isValidReading(float value) {
  return !isnan(value) && isfinite(value);
}

void setRelay(bool state) {
  relayOn = state;
  digitalWrite(relay, relayOn ? HIGH : LOW);
}

void printValues() {
  String line = String(CAL_DEVICE_ID) + ",1,DATA," +
                String(tempsens, 1) + "," +
                String(shtTemp, 1) + "," +
                String(tempBME, 1) + "," +
                String(shtHum, 1) + "," +
                String(humBME, 1) + "," +
                String(tempThermo, 1) + "," +
                String(relayOn ? 1 : 0) + "," +
                String(heaterByHumidity ? 1 : 0) + "," +
                String(heaterByTemperature ? 1 : 0) + "," +
                String(emergencyMode ? 1 : 0);

  Serial.println(line);
  sendFrame(line);
  Serial.flush();

  if (isValidReading(getControlHumidity()) && getControlHumidity() >= RH_ALTA) {
    Serial.println(F("Aviso: RH >= 70%, zona alta para sesgo higroscopico en PMS5003."));
  }
}

bool verifyChecksum(String data) {
  data.trim();

  if (data.length() < 3) {
    Serial.println(F("Data too short to contain a checksum."));
    return false;
  }

  numberString = data.substring(0, data.length() - 2);
  String checksumString = data.substring(data.length() - 2);

  int calculatedChecksum = 0;
  for (int i = 0; i < numberString.length(); i++) {
    calculatedChecksum += numberString[i];
  }
  calculatedChecksum %= 256;

  int receivedChecksum = strtol(checksumString.c_str(), NULL, 16);

  return calculatedChecksum == receivedChecksum;
}

void restartESP() {
  Serial.println(F("Reiniciando ESP8266..."));
  ESP.restart();
}
