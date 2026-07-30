#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include "MAX6675.h"
#include <DHT.h>
#include <DHT_U.h>
#include <Ticker.h>
#include <SoftwareSerial.h>
#define externo_TX D4                            //
#define externo_RX D3                            //D3
SoftwareSerial externo(externo_RX, externo_TX);  // RX, TX
const int restartInterval = 4 * 60 * 60 * 1000;  // 4 horas en milisegundos
Ticker ticker;

#define DHTPIN D7      // Digital pin connected to the DHT sensor D7 original
#define DHTTYPE DHT22  // DHT 22 (AM2302)
DHT_Unified dht(DHTPIN, DHTTYPE);
#define SEALEVELPRESSURE_HPA (1013.25)
#define relay D0
const int dataPin = D6;    //SO
const int clockPin = D5;   //SCK
const int selectPin = D8;  //CS D6 originalmente D8
String numberString;

MAX6675 thermoCouple(selectPin, dataPin, clockPin);
uint32_t start, stop;

Adafruit_BME280 bme;  // I2C

unsigned long delayTime;
float tempThermo;
float tempBMP;
float humBMP;
float dhttemp;
float dhthum;
float tempsens = 16;
int maxcontrol = 70;
int maxemergencia = maxcontrol + 10;
int minimo = 15;             //////////////////////////////////////////////////////////////////////////////////////////atencion!!
bool emergencyMode = false;  // Estado de emergencia, inicialmente falso
bool statusBME = true;
unsigned long lastFeedTime;
const unsigned long watchdogTimeout = 30000;  // 30 segundos

void setup() {
  // Reinicia y alimenta el watchdog
  ESP.wdtDisable();
  ESP.wdtEnable(watchdogTimeout);
  Serial.begin(115200);
  externo.begin(9600);
  SPI.begin();
  pinMode(relay, OUTPUT);
  thermoCouple.begin();
  thermoCouple.setSPIspeed(4000000);

  Serial.println(F("BME280 test"));
  if (!bme.begin(0x76)) {
    Serial.println(F("Could not find a valid BME280 sensor, check wiring!"));
    statusBME = false;
  }

  dht.begin();
  sensor_t sensor;
  dht.temperature().getSensor(&sensor);
  dht.humidity().getSensor(&sensor);
  ticker.attach(restartInterval, restartESP);
}

unsigned long lastTime = 0;          // Almacena la última vez que la función fue ejecutada
const unsigned long interval = 280;  // Intervalo de tiempo para ejecuciones repetidas (300 ms)

void loop() {
  // Alimenta el watchdog
  ESP.wdtFeed();
  lastFeedTime = millis();

  // Lee y verifica datos si están disponibles
  if (externo.available() > 0) {
    String receivedData = externo.readStringUntil('\n');
    if (verifyChecksum(receivedData)) {
      // Serial.println("Checksum correcto, datos recibidos: " + receivedData);
      float number = numberString.toFloat();
      // Dividir el número por 10
      tempsens = number / 10.0;
      //externo.print(String(tempsens) + ",");
      Serial.println(tempsens);
      printValues();

    } else {
      Serial.println("Error de checksum.");
    }
  }

  if (millis() - lastTime >= interval) {
    lastTime = millis();  // Actualiza el último tiempo registrado
    int status = thermoCouple.read();
    tempThermo = thermoCouple.getTemperature();
    if (!statusBME) {
      tempBMP = 999;
      humBMP = 999;
    } else {
      tempBMP = bme.readTemperature();
      humBMP = bme.readHumidity();
    }
    sensors_event_t event;
    dht.temperature().getEvent(&event);
    dhttemp = event.temperature;
    dht.humidity().getEvent(&event);
    dhthum = event.relative_humidity;
    printValues();
  }


  if (tempBMP < minimo or tempsens < minimo or dhttemp < minimo) {
    digitalWrite(relay, HIGH);
    // Serial.println("relay on");
  } else {
    digitalWrite(relay, LOW);
    //Serial.println("relay off");
  }

  // Código existente para manejar condiciones mínimas

  if (tempThermo >= maxcontrol || tempThermo == 0) {
    digitalWrite(relay, LOW);
    if (tempThermo >= maxemergencia) {  // Si la temperatura alcanza el límite crítico + 10
      emergencyMode = true;             // Activa el modo de emergencia
    }
  }

  // Bloquea el código si está en modo de emergencia y la temperatura no ha caído lo suficiente
  while (emergencyMode) {
    Serial.println("parada de emergencia sobre: " + String(maxemergencia));
    int status = thermoCouple.read();
    tempThermo = thermoCouple.getTemperature();  // Actualiza la temperatura en modo emergencia
    Serial.println("temp: " + String(tempThermo));
    externo.println(String(tempsens) + "," + String(dhttemp) + "," + String(tempBMP) + "," + String(dhthum) + "," + String(humBMP) + "," + String(tempThermo));
    //externo.println('e');
    if (tempThermo < maxemergencia) {  // Verifica si la temperatura ha caído 10 grados por debajo del máximo
      emergencyMode = false;           // Desactiva el modo de emergencia
    }
    delay(300);  // Retardo para reducir la frecuencia de las lecturas de temperatura
  }
}


void printValues() {
  Serial.println(String(tempsens) + "," + String(dhttemp) + "," + String(tempBMP) + "," + String(dhthum) + "," + String(humBMP) + "," + String(tempThermo));
  externo.println(String(tempsens) + "," + String(dhttemp) + "," + String(tempBMP) + "," + String(dhthum) + "," + String(humBMP) + "," + String(tempThermo));
  Serial.flush();
}

bool verifyChecksum(String data) {
  // Eliminar cualquier salto de línea o retorno de carro residual
  data.trim();

  if (data.length() < 3) {
    Serial.println("Data too short to contain a checksum.");
    return false;  // Asegura longitud mínima
  }

  // Extracción del número y del checksum
  numberString = data.substring(0, data.length() - 2);
  String checksumString = data.substring(data.length() - 2);

  // // Depuración: Imprime los componentes separados
  Serial.print("Number String: ");
  Serial.println(numberString);
  Serial.print("Checksum String: ");
  Serial.println(checksumString);

  // Cálculo del checksum
  int calculatedChecksum = 0;
  for (int i = 0; i < numberString.length(); i++) {
    calculatedChecksum += numberString[i];
  }
  calculatedChecksum %= 256;

  // Conversión del checksum recibido
  int receivedChecksum = strtol(checksumString.c_str(), NULL, 16);

  // // Depuración: Imprime los checksums calculados y recibidos
  Serial.print("Calculated Checksum: ");
  Serial.println(calculatedChecksum);
  Serial.print("Received Checksum: ");
  Serial.println(receivedChecksum);

  // Verificación del checksum
  return calculatedChecksum == receivedChecksum;
}
void restartESP() {
  Serial.println("Reiniciando ESP8266...");
  ESP.restart();
}