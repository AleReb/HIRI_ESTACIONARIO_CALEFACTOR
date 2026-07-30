/*!
 * @file  SEN0233.ino
 * @brief Air Quality Monitor (PM 2.5, HCHO, Temperature & Humidity)
 * @n Get the module here: https://www.dfrobot.com/product-1612.html
 * @n This example is to detect formaldehyde, PM2.5, temperature and humidity in the environment.
 * @copyright  Copyright (c) 2010 DFRobot Co.Ltd (http://www.dfrobot.com)
 * @license  The MIT License (MIT)
 * @author  [lijun](ju.li@dfrobot.com)
 * @version  V2.0
 * @date  19/01/2024
 Modificado por alejandro rebolledo para ver el formalheidro, que segun el datashet esta en un dato antes de la temperatura
 revisar los tiempos de respuestra con waitresponce, versus waitforresponce \
 cambiado el codigo para trabajar con servidor mysql, sin seguridad! considerar generar algun tipo de key o encriptacion
 el esp32 esta en la version 2.0.11
 */
#define TINY_GSM_MODEM_SIM7600
#define TINY_GSM_RX_BUFFER 1024  // Set RX buffer to 1Kb
// See all AT commands, if wanted
#define DUMP_AT_COMMANDS
#define SerialAT Serial1

#include <Arduino.h>
//////////////////////////////
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>  // Incluir la biblioteca para mDNS
#include <Update.h>
//////////////////////////////////////////7
#include <TinyGsmClient.h>
#include <SoftwareSerial.h>
#include <U8g2lib.h>
// Configuración de la pantalla OLED
U8G2_SH1107_SEEED_128X128_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
#include <OneButton.h>
#include <Wire.h>
#include <RTClib.h>
#include <Adafruit_NeoPixel.h>
#include "SD.h"
#include "SPI.h"
#include "FS.h"

///watchdog
#include <Ticker.h>
#include "esp_task_wdt.h"            //watchdog
const int watchdogTimeout = 300000;  // 5 minutos en milisegundos

// Configuración del RTC
RTC_DS3231 rtc;

//////////////////////////////////////////////////////////////////////////////
//pantalla animacion
#include "tipo.h"  // Asumiendo que este archivo existe y es correcto

int indice = 0;
#define SCREEN_WIDTH 128   // OLED display width, in pixels
#define SCREEN_HEIGHT 128  // OLED display height, in pixels
#define MAX_PARTICLES 60   // Máximo número de partículas que el sistema puede manejar

float particle_x[MAX_PARTICLES];
float particle_y[MAX_PARTICLES];
int particle_speed_x[MAX_PARTICLES];
int particle_speed_y[MAX_PARTICLES];
float particle_size[MAX_PARTICLES];
int init_spread = 55;  // Rango inicial para la dispersión de partículas
float bat;
float pm25_level = 0.0;  // Nivel de PM2.5 actual
/////////////////////////////////////////////////////////////////////////////////////////////////////

#define uS_TO_S_FACTOR 1000000ULL /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP 30          /* Time ESP32 will go to sleep (in seconds) */
#define UART_BAUD 115200
#define MODEM_TX 27
#define MODEM_RX 26
#define pms_TX 5       //18   // version antenrior 23
#define pms_RX 18      //5   // version anterior 19
#define externo_TX 23  // version anterior 19
#define externo_RX 19  // versionm    23
#define MODEM_PWRKEY 4
#define MODEM_DTR 32
#define MODEM_RI 33
#define MODEM_FLIGHT 25
#define MODEM_STATUS 34

const char apn[] = "gigsky-02";  //nuevas sim
const char gprsUser[] = "";
const char gprsPass[] = "";

String serverName = "https://southamerica-west1-fic-aysen-412113.cloudfunctions.net/nuevoDatoMultiple?idSensor=";
String contentType = "url";
#ifdef DUMP_AT_COMMANDS  // if enabled it requires the streamDebugger lib
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, Serial);
TinyGsm modem(debugger);
#else
TinyGsm modem(SerialAT);
#endif

////////////////////////////////////////////////////////////////////////
// Configuración de la tarjeta SD
#define SD_MISO 2
#define SD_MOSI 15
#define SD_SCLK 14
#define SD_CS_PIN 13

SPIClass spiSD(HSPI);  // HSPI

//seriales
SoftwareSerial pms5(pms_TX, pms_RX);             // RX, TX
SoftwareSerial externo(externo_RX, externo_TX);  // RX, TX
// variables para leer el pms5003 con formalhidro
char col;
unsigned int PMSa1 = 0, PMSa2_5 = 0, PMSa10 = 0, FMHDSa = 0, TPSa = 0, HDSa = 0, PMSb1 = 0, PMSb2_5 = 0, PMSb10 = 0, FMHDSb = 0, TPSb = 0, HDSb = 0;
unsigned int PMS1 = 0, PMS2_5 = 0, PMS10 = 0, TPS = 0, HDS = 0, CR1 = 0, CR2 = 0;
unsigned char bufferRTT[32] = {};  //serial receive data
char tempStr[15];
float FMHDSB = 0;
// Configuración del NeoPixel
#define PIN 12  //  12 pin led interno
#define NUMPIXELS 1
Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

// Configuración del botón
#define BUTTON_PIN 39  //  39

OneButton btn = OneButton(
  BUTTON_PIN,  // Input pin for the button
  false,       // Button is active high
  false        // Disable internal pull-up resistor
);
volatile bool buttonFlag = false;  // Flag para la ISR
void IRAM_ATTR checkTicks() {
  buttonFlag = true;  // Establece el flag para manejar en el loop
}

#define batPin 35   //pin intnerno para lectura de bateria
#define extReset 0  //cmanejo externo del pin  de reset para calefactor

float tempe;
float humi;
String fecha;
String hora;
String fechaDias;
bool rtcOK = true;
bool ext = false;
bool calefactorOn = false;
bool menu = false;
bool rOKfs = false;
int ultimoRun = 0;
String ID = "37";        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////// este es el sensor de pruebas 22
String ver = "V:0.9.9";  /////////////////////////////////////////////////////
String ARCHIVO = "/data" + ID + ".csv";
String data;
String receivedString;
bool rotate = false;
bool rgb = true;

float dhtTemp;     // Temperatura del sensor DHT
float tempBMP;     // Temperatura del sensor BMP
float dhtHum;      // Humedad del sensor DHT
float humBMP;      // Humedad del sensor BMP
float tempThermo;  // Temperatura de un termómetro, podría ser termopar u otro

//float datomin1, datomin2, datomin3, datomin4, datomin5;
//float datomin6, datomin7, datomin8, datomin9, datomin10;
int datomin1, datomin2, datomin3, datomin4, datomin5;
int datomin6, datomin7, datomin8, datomin9, datomin10;

String ssid;
String password;  // Password for AP
String myIPG;
WebServer server(80);

static bool hasSD = false;
static bool status = false;  // variable para
File uploadFile;
bool systemActive = false;
bool clientConnected = false;   // Variable para indicar si hay una conexión activa
bool operationSuccess = false;  // variable para saber si recibi ok del servidor
////////////////////////////////////////////////////////////// manejo asincronico de las respuestas del modem
int attempt;
const int MAX_RETRIES = 3;
int MAX_RETRIESSERIAL;
const unsigned long RETRY_DELAY_MS = 10000;  // 10 segundos entre reintentos
String responseBuffer;                       // Búfer global para almacenar la respuesta
bool awaitingData = false;                   // Indica si se espera leer datos del módem
int dataLength = 0;                          // Longitud de los datos a leer
//////////////////////////////////////////////////////////////////promedios para recuperar
const int numReadings = 20;        // Número de lecturas por minuto (20 lecturas en 60 segundos, una cada 3 segundos) originalmente 20
float readings[numReadings];       // Array para almacenar las lecturas individuales
float minuteAverages[10];          // Array para almacenar los promedios de cada minuto
int readIndex = 0;                 // Índice actual para la lectura
unsigned long lastUpdateProm = 0;  // Tiempo de la última actualización
int minuteIndex = 0;               // Índice para los promedios por minuto
String urlData;
/////////////////////////////////////// timers
unsigned long lastUpdateTimecheck = 0;          // Almacena la última vez que la variable fue actualizada
bool isUpdated = true;                          // Booleano que indica si la variable ha sido actualizada recientemente
int monitoredVariable = 0;                      // Esta es la variable que estamos monitoreando serial
unsigned long ultimoGuardado = 0;               // Variable para llevar un registro del último guardado y conexion
const unsigned long intervaloGuardado = 10000;  // Intervalo de guardado 10s
unsigned long checkpms = 0;                     // Variable para llevar EL PMS
const unsigned long intercheckpms = 3000;       // Intervalo LECTURA PMS ESTABILIDAD (3 segundos en milisegundos)
unsigned long showSerial = 0;                   // Variable para llevar un registro del último guardado
const unsigned long interShow = 600000;         // Intervalo de mandado de datos 10 minutos 600000 este show de serial tambien manda mensajes;
// Variable global para almacenar la última vez que se cambió la pantalla
unsigned long lastUpdateTime = 0;
// Intervalo de actualización (5 segundos * 1000 milisegundos) pantalla
const unsigned long updateInterval = 5000;
//// ////////////sistema para suavizar el uso del boton;
unsigned long lastPauseMillis = 0;               // Para almacenar el último valor de millis relacionado con la pausa
const unsigned long pauseIntervalMillis = 3000;  // Intervalo de 3 segundos (3000 ms)
// Clase para gestionar el parpadeo del LED
class LedBlinker {
public:
  LedBlinker(uint16_t ledIndex, uint32_t color, float interval)
    : ledIndex(ledIndex), color(color), interval(interval), state(false) {}
  void begin() {
    // Usa attach_ms para adjuntar una función que acepte argumentos
    ticker.attach_ms(static_cast<int>(interval * 1000), LedBlinker::toggleWrapper, this);
  }
  void stop() {
    ticker.detach();
    // Apaga el LED
    pixels.setPixelColor(ledIndex, 0, 0, 0);  // Establece el color a negro (apagado)
    pixels.show();
  }
private:
  uint16_t ledIndex;
  uint32_t color;
  float interval;
  bool state;
  Ticker ticker;
  void toggle() {
    if (state) {
      // Apaga el LED
      pixels.setPixelColor(ledIndex, 0, 0, 0);  // Establece el color a negro (apagado)
    } else {
      // Enciende el LED con el color especificado
      pixels.setPixelColor(ledIndex, color);
    }
    state = !state;  // Cambia el estado del LED
    pixels.show();   // Actualiza el strip de LEDs
  }
  static void toggleWrapper(LedBlinker* instance) {
    instance->toggle();
  }
};

LedBlinker blinker1(0, pixels.Color(0, 20, 255), 1.0);  // Parpadeo cada 1 segundo con color azul
LedBlinker blinker2(0, pixels.Color(0, 255, 0), 0.5);   // Parpadeo cada 0.5 segundos con color verde

////////////////////////////////////////////////////////////////////////////////////////////////////////
void setup() {
  blinker1.begin();
  // Inicializar todos los pines del SPI en estado bajo
  pinMode(SD_MISO, INPUT_PULLUP);
  pinMode(SD_MOSI, OUTPUT);
  pinMode(SD_SCLK, OUTPUT);
  pinMode(SD_CS_PIN, OUTPUT);
  digitalWrite(SD_MOSI, LOW);
  digitalWrite(SD_SCLK, LOW);
  digitalWrite(SD_CS_PIN, LOW);
  // Inicializa el watchdog timer con un tiempo de espera de 5 minutos
  esp_task_wdt_init(watchdogTimeout, true);
  Serial.begin(115200);
  pms5.begin(9600);
  externo.begin(9600);
  u8g2.begin();
  pixels.begin();
  // u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_logisoso16_tr);  // Fuente más grande para PM
  u8g2.setCursor(0, 18);                  // y = 96
  u8g2.print("4G PMS " + ver);
  u8g2.sendBuffer();
  // pinMode(BUTTON_PIN, INPUT);
  if (!rtc.begin()) {
    Serial.println("No se encontró el RTC");
    rtcOK = false;
    u8g2.setCursor(0, 38);  // y = 96
    u8g2.print("RTC NO");
    u8g2.sendBuffer();
  } else {
    u8g2.setCursor(0, 38);  // y = 96
    u8g2.print("RTC OK");
    u8g2.sendBuffer();
  }
  u8g2.setCursor(70, 38);  // y = 96
  u8g2.print("I:" + String(ID));
  u8g2.sendBuffer();
  SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
  Serial.println("\nStart");
  /*
    MODEM_PWRKEY IO:4 The power-on signal of the modulator must be given to it,
    otherwise the modulator will not reply when the command is sent
  */
  pinMode(MODEM_PWRKEY, OUTPUT);
  digitalWrite(MODEM_PWRKEY, HIGH);
  delay(300);  //Need delay
  digitalWrite(MODEM_PWRKEY, LOW);
  /*
    MODEM_FLIGHT IO:25 Modulator flight mode control,
    need to enable modulator, this pin must be set to high
  */
  pinMode(MODEM_FLIGHT, OUTPUT);
  digitalWrite(MODEM_FLIGHT, HIGH);
  delay(500);
  u8g2.setCursor(0, 54);  // y = 96
  u8g2.print("modem rutine");
  u8g2.setDrawColor(0);  // Establece el color de dibujo a negro
  u8g2.drawXBMP(30, 70, logoc_width, logoc_height, logoc_bits);
  u8g2.setDrawColor(1);  // Establece el color de dibujo a blanco
  u8g2.sendBuffer();
  modem.restart();
  if (!modem.init()) {
    Serial.println("Error al inicializar módem");
    u8g2.setCursor(0, 66);  // y = 96
    u8g2.print("ERROR MODEM");
    u8g2.sendBuffer();
  }
  if (!connectToNetwork()) {
    Serial.println("Error al conectar a la red y a GPRS.");
    u8g2.setCursor(0, 84);  // y = 96
    u8g2.print("SIN RED");
    u8g2.sendBuffer();
  }
  for (int i = 0; i < numReadings; i++) {
    readings[i] = 0;
  }
  for (int i = 0; i < 10; i++) {
    minuteAverages[i] = 0;
  }
  // Otras configuraciones externo
  u8g2.clearBuffer();
  pinMode(extReset, OUTPUT);
  // digitalWrite(extReset, LOW);
  // delay(300);  //Need delay
  digitalWrite(extReset, HIGH);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), checkTicks, CHANGE);
  btn.setPressTicks(3000);  // Tiempo en milisegundos para considerar una pulsación larga (1000 ms)
  btn.attachLongPressStart(longPressStartFunction);
  blinker1.stop();
}

//////////////////////////////////////////////////////////////////////////////////loop
//
//////////////////////////////////////////////////////////////////////////////////////

bool primerloop = true;
int lastIndex = -1;  // Inicializar con un valor que no sea un índice válido
bool ispause = false;
void loop() {
  esp_task_wdt_reset();
  btn.tick();            // Maneja el estado del botón
  readSerialResponse();  //modo nuevo
  if (buttonFlag) {
    btn.tick();  // Acción del botón
    Serial.println("entre en el flag");
    ispause = true;
    clickFunction();
    delay(200);
    lastPauseMillis = millis();
    buttonFlag = false;  // Reinicia el flag
  }

  if (primerloop == true) {
    if (millis() - checkpms >= intercheckpms) {
      // Actualizar el registro del último guardado
      checkpms = millis();
      while (pms5.available() > 0)  //Check whether there is any serial data
      {
        for (int i = 0; i < 32; i++) {
          col = pms5.read();
          bufferRTT[i] = (char)col;
          delay(2);
        }
        pms5.flush();
        CR1 = (bufferRTT[30] << 8) + bufferRTT[31];
        CR2 = 0;
        for (int i = 0; i < 30; i++)
          CR2 += bufferRTT[i];
        if (CR1 == CR2)  //Check
        {
          PMSa1 = bufferRTT[10];        //Read PM1 high 8-bit data
          PMSb1 = bufferRTT[11];        //Read PM1 low 8-bit data
          PMS1 = (PMSa1 << 8) + PMSb1;  //PM1 data

          PMSa2_5 = bufferRTT[12];            //Read PM2.5 high 8-bit data
          PMSb2_5 = bufferRTT[13];            //Read PM2.5 low 8-bit data
          PMS2_5 = (PMSa2_5 << 8) + PMSb2_5;  //PM2.5 data

          PMSa10 = bufferRTT[14];          //Read PM10 high 8-bit data
          PMSb10 = bufferRTT[15];          //Read PM10 low 8-bit data
          PMS10 = (PMSa10 << 8) + PMSb10;  //PM10 data

          TPSa = bufferRTT[24];      //Read temperature high 8-bit data
          TPSb = bufferRTT[25];      //Read temperature low 8-bit data
          TPS = (TPSa << 8) + TPSb;  //Temperature data

          HDSa = bufferRTT[26];      //Read humidity high 8-bit data
          HDSb = bufferRTT[27];      //Read humidity low 8-bit data
          HDS = (HDSa << 8) + HDSb;  //Humidity data

          unsigned int FMHDSa = bufferRTT[22];  // Read formaldehyde high 8-bit data
          unsigned int FMHDSb = bufferRTT[23];  // Read formaldehyde low 8-bit data
          FMHDSB = (FMHDSa << 8) + FMHDSb;      // Combine high and low bits
          FMHDSB = FMHDSB / 1000.0;
        }
      }

      tempe = TPS / 10.0;
      humi = HDS / 10.0;
      if (ext == true and MAX_RETRIESSERIAL < 10) {
        sendDataWithChecksum(TPS);
        Serial.println(String(TPS));
      }
    }
    u8g2.clearBuffer();
    activateSystem();
    u8g2.setCursor(0, 40);  //
    u8g2.print("activando wifi...");
    u8g2.sendBuffer();
    primerloop = false;
  } else {
    // Verifica si hay algún cliente conectado
    clientConnected = (WiFi.softAPgetStationNum() > 0);
  }
  if (clientConnected) {
    pixels.setPixelColor(0, pixels.Color(0, 255, 255));
    pixels.show();
    server.handleClient();
  }
  if (clientConnected and status == false) {
    // Verde calipso
    pixels.setPixelColor(0, pixels.Color(0, 255, 255));
    pixels.show();
    server.handleClient();
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.drawStr(0, 10, "System Activated");
    u8g2.drawStr(0, 30, ("" + ssid).c_str());
    u8g2.drawStr(0, 50, ("IP: " + myIPG).c_str());
    u8g2.drawStr(0, 70, ("PASS: " + password).c_str());
    u8g2.drawStr(0, 85, (ver).c_str());
    if (hasSD = true) {
      u8g2.setCursor(0, 100);  // y = 96
      u8g2.print("SD OK");
    } else {
      u8g2.setCursor(0, 100);  // y = 96
      u8g2.print("SD FAIL");
    }
    u8g2.drawStr(0, 110, "Client Connected");
    u8g2.drawStr(0, 120, "http://pms32.local");
    u8g2.sendBuffer();
  }

  if (!clientConnected or status == true) {
    if (menu) {
      drawMenu();
    }
    if (ispause == false) {
      bat = ((float)analogRead(batPin) / 4095 * 3.3) * 2 * 1.04;  //voltaje relativo de la bateria
      if (millis() - checkpms >= intercheckpms) {
        // Actualizar el registro del último guardado
        checkpms = millis();
        while (pms5.available() > 0)  //Check whether there is any serial data
        {
          for (int i = 0; i < 32; i++) {
            col = pms5.read();
            bufferRTT[i] = (char)col;
            delay(2);
          }
          pms5.flush();
          CR1 = (bufferRTT[30] << 8) + bufferRTT[31];
          CR2 = 0;
          for (int i = 0; i < 30; i++)
            CR2 += bufferRTT[i];
          if (CR1 == CR2)  //Check
          {
            PMSa1 = bufferRTT[10];        //Read PM1 high 8-bit data
            PMSb1 = bufferRTT[11];        //Read PM1 low 8-bit data
            PMS1 = (PMSa1 << 8) + PMSb1;  //PM1 data

            PMSa2_5 = bufferRTT[12];            //Read PM2.5 high 8-bit data
            PMSb2_5 = bufferRTT[13];            //Read PM2.5 low 8-bit data
            PMS2_5 = (PMSa2_5 << 8) + PMSb2_5;  //PM2.5 data

            PMSa10 = bufferRTT[14];          //Read PM10 high 8-bit data
            PMSb10 = bufferRTT[15];          //Read PM10 low 8-bit data
            PMS10 = (PMSa10 << 8) + PMSb10;  //PM10 data

            TPSa = bufferRTT[24];      //Read temperature high 8-bit data
            TPSb = bufferRTT[25];      //Read temperature low 8-bit data
            TPS = (TPSa << 8) + TPSb;  //Temperature data

            HDSa = bufferRTT[26];      //Read humidity high 8-bit data
            HDSb = bufferRTT[27];      //Read humidity low 8-bit data
            HDS = (HDSa << 8) + HDSb;  //Humidity data

            unsigned int FMHDSa = bufferRTT[22];  // Read formaldehyde high 8-bit data
            unsigned int FMHDSb = bufferRTT[23];  // Read formaldehyde low 8-bit data
            FMHDSB = (FMHDSa << 8) + FMHDSb;      // Combine high and low bits
            FMHDSB = FMHDSB / 1000.0;
          }
        }

        tempe = TPS / 10.0;
        humi = HDS / 10.0;
        if (ext == true and MAX_RETRIESSERIAL < 10) {
          sendDataWithChecksum(TPS);
          Serial.println(String(TPS));
        }
      }
      // Actualizar cada 3 segundos
      if (millis() - lastUpdateProm >= 3000) {
        lastUpdateProm = millis();
        // Leer la variable (por ejemplo, un sensor analógico)
        float newValue = PMS2_5;  // Reemplaza PMS2_5 con la lectura de tu sensor
        readings[readIndex] = newValue;
        readIndex = (readIndex + 1) % numReadings;

        // Si se ha completado una vuelta del array
        if (readIndex == 0) {
          float sum = 0;
          for (int i = 0; i < numReadings; i++) {
            sum += readings[i];
          }
          float average = sum / numReadings;
          minuteAverages[minuteIndex] = average;
          Serial.print("Promedio del minuto ");
          Serial.print(minuteIndex + 1);
          Serial.print(": ");
          Serial.println(average);
          Serial.println("promedios del 10 al 1:" + String(datomin10) + "," + String(datomin9) + "," + String(datomin8) + "," + String(datomin7) + "," + String(datomin6) + "," + String(datomin5) + "," + String(datomin4) + "," + String(datomin3) + "," + String(datomin2) + "," + String(datomin1));
          // Almacenar el promedio en las variables específicas
          switch (minuteIndex) {
            case 0: datomin1 = average; break;
            case 1: datomin2 = average; break;
            case 2: datomin3 = average; break;
            case 3: datomin4 = average; break;
            case 4: datomin5 = average; break;
            case 5: datomin6 = average; break;
            case 6: datomin7 = average; break;
            case 7: datomin8 = average; break;
            case 8: datomin9 = average; break;
            case 9: datomin10 = average; break;
          }

          minuteIndex++;
          if (minuteIndex >= 10) {
            minuteIndex = 0;  // Reiniciar el índice si se han registrado 10 promedios
            // Aquí puedes decidir qué hacer cuando se llenan los 10 promedios
          }
        }
      }

      //esta funcion manda el dato
      if (millis() - showSerial >= interShow) {  //esta es la funcion que manda el dato
        showSerial = millis();
        Serial.println("-----------------------pms--------------------------");
        Serial.print("Temp : ");
        sprintf(tempStr, "%d%d.%d", TPS / 100, (TPS / 10) % 10, TPS % 10);
        Serial.print(tempStr);
        Serial.println(" C");  //Display temperature
        Serial.print("RH   : ");
        sprintf(tempStr, "%d%d.%d", HDS / 100, (HDS / 10) % 10, HDS % 10);
        Serial.print(tempStr);  //Display humidity
        Serial.println(" %");   //%
        Serial.print("PM1.0: ");
        Serial.print(PMS1);
        Serial.println(" ug/m3");  //Display PM1.0 unit  ug/m³
        Serial.print("PM2.5: ");
        Serial.print(PMS2_5);
        Serial.println(" ug/m3");  //Display PM2.5 unit  ug/m³
        Serial.print("PM 10: ");
        Serial.print(PMS10);
        Serial.println(" ug/m3");  //Display PM 10 unit  ug/m³
        Serial.print("Formaldehyde: ");
        Serial.print(FMHDSB);
        Serial.println(" mg/m3");  // Display formaldehyde concentration in mg/m³
        Serial.print(hasSD);
        Serial.print(" SD");
        Serial.println(ultimoRun);
        //// revisando conexion
        if (!modem.isGprsConnected()) {
          Serial.println("GPRS desconectado. Intentando reconectar...");
          if (!connectToNetwork()) {
            Serial.println("No se pudo reconectar....");
            ESP.restart();
          }
        }
        pixels.setPixelColor(0, pixels.Color(255, 255, 255));
        pixels.show();
        delay(100);
        urlData.reserve(200);  // Ajusta este número según la longitud esperada de la URL
        if (ext == true) {
          urlData = serverName + ID + "&temp=" + String(dhtTemp) + "&hum=" + String(dhtHum) + "&pm1=" + PMS1 + "&pm25=" + datomin10 + "," + datomin9 + "," + datomin8 + "," + datomin7 + "," + datomin6 + "," + datomin5 + "," + datomin4 + "," + datomin3 + "," + datomin2 + "," + datomin1 + "&pm10=" + PMS10 + "&fh=" + FMHDSB;
        } else {
          urlData = serverName + ID + "&temp=" + tempe + "&hum=" + humi + "&pm1=" + PMS1 + "&pm25=" + datomin10 + "," + datomin9 + "," + datomin8 + "," + datomin7 + "," + datomin6 + "," + datomin5 + "," + datomin4 + "," + datomin3 + "," + datomin2 + "," + datomin1 + "&pm10=" + PMS10 + "&fh=" + FMHDSB;
        }
        Serial.println("pasamos a mandar");
        sendDataToServer(urlData);  // Envía los datos
        u8g2.println("S");          // Enviar búfer a la pantalla
        u8g2.sendBuffer();
      }

      if (rtcOK == true) {
        // Leer y mostrar la fecha y hora del RTC
        DateTime now = rtc.now();
        fecha = getFecha(now);
        hora = getHora(now);
        fechaDias = getDiaMes(now);
      }
      //// manejo de datos externos
      if (tempThermo > tempBMP + 2) {
        calefactorOn = true;
      }
      if (externo.available()) {
        // Lee el string desde el Software Serial hasta encontrar un salto de línea
        receivedString = externo.readStringUntil('\n');
        receivedString.trim();  // Esto elimina espacios en blanco y caracteres de control
        Serial.print("Recibido: ");
        Serial.println(receivedString);
        processData(receivedString);
        ext = true;
        MAX_RETRIESSERIAL = 0;
        indice = 3;
        updateVariable(tempThermo);
      }

      // Verifica si han pasado 3 segundos desde la última actualización
      if (millis() - lastUpdateTimecheck >= 3000) {  // 3000 milisegundos = 3 segundos
        isUpdated = false;
        if (MAX_RETRIESSERIAL < 10) {
          MAX_RETRIESSERIAL = MAX_RETRIESSERIAL + 1;
          Serial.println(MAX_RETRIESSERIAL);
        }
      }
      if (!isUpdated and MAX_RETRIESSERIAL == 4) {
        Serial.println("wemos no conectado.");
        digitalWrite(extReset, LOW);
        delay(300);  //Need delay
        digitalWrite(extReset, HIGH);
      }
      if (!isUpdated and MAX_RETRIESSERIAL == 8) {
        Serial.println("wemos no conectado.");
        digitalWrite(extReset, LOW);
        delay(300);  //Need delay
        digitalWrite(extReset, HIGH);
      }

      // Hacer algo basado en el estado de isUpdated
      if (!isUpdated and MAX_RETRIESSERIAL < 10) {
        Serial.println("wemos no conectado.");
        dhtTemp = 0;
        tempBMP = 0;
        dhtHum = 0;
        humBMP = 0;
        tempThermo = 0;
        ext = false;
      }

      ////////////////////////////////////
      tempe = TPS / 10.0;
      humi = HDS / 10.0;

      if (millis() - ultimoGuardado >= intervaloGuardado) {
        // Actualizar el registro del último guardado
        ultimoGuardado = millis();
        u8g2.setCursor(60, 120);  // y = 96
        if (hasSD == true) {
          // Obtener el color original
          uint32_t originalColor = pixels.getPixelColor(0);

          // Descomponer en componentes RGB
          uint8_t originalRed = (uint8_t)(originalColor >> 16);
          uint8_t originalGreen = (uint8_t)(originalColor >> 8);
          uint8_t originalBlue = (uint8_t)originalColor;

          // Calcular color atenuado
          uint8_t dimmedRed = originalRed * 0.5;
          uint8_t dimmedGreen = originalGreen * 0.5;
          uint8_t dimmedBlue = originalBlue * 0.5;

          // Apagar gradualmente
          for (float fadeFactor = 1.0; fadeFactor >= 0.3; fadeFactor -= 0.1) {
            // Calcular color atenuado
            uint8_t dimmedRed = originalRed * fadeFactor;
            uint8_t dimmedGreen = originalGreen * fadeFactor;
            uint8_t dimmedBlue = originalBlue * fadeFactor;

            // Establecer el color atenuado
            pixels.setPixelColor(0, pixels.Color(dimmedRed, dimmedGreen, dimmedBlue));
            pixels.show();
            delay(50);  // Ajuste de tiempo para que la atenuación dure 0.5 segundos
          }

          // Prender gradualmente
          for (float fadeFactor = 0.0; fadeFactor <= 1.0; fadeFactor += 0.1) {
            // Calcular color atenuado
            uint8_t dimmedRed = originalRed * fadeFactor;
            uint8_t dimmedGreen = originalGreen * fadeFactor;
            uint8_t dimmedBlue = originalBlue * fadeFactor;

            // Establecer el color atenuado
            pixels.setPixelColor(0, pixels.Color(dimmedRed, dimmedGreen, dimmedBlue));
            pixels.show();
            delay(50);  // Ajuste de tiempo para que el encendido dure 0.5 segundos
          }

          // Restaurar el color original
          pixels.setPixelColor(0, originalColor);
          pixels.show();
          u8g2.print("*");
          u8g2.sendBuffer();
          // Formatea tus datos en una cadena
          if (ext == true) {
            data = String(fecha) + "," + String(hora) + "," + String(tempe) + "," + String(humi) + "," + String(PMS1) + "," + String(PMS2_5) + "," + String(PMS10) + "," + String(FMHDSB) + "," + String(dhtTemp) + "," + String(dhtHum) + "," + String(tempBMP) + "," + String(humBMP) + "," + String(tempThermo) + "\n";
          } else {
            data = String(fecha) + "," + String(hora) + "," + String(tempe) + "," + String(humi) + "," + String(PMS1) + "," + String(PMS2_5) + "," + String(PMS10) + "," + String(FMHDSB) + "\n";
          }
          if (hasSD) {
            // Guarda los datos en el archivo
            appendFile(SD, ARCHIVO.c_str(), data.c_str());
          }
        }
      }
      if (rgb == true) {
        if (PMS2_5 <= 10) {
          // Verde
          pixels.setPixelColor(0, pixels.Color(0, 255, 0));
          pixels.show();
        } else if (PMS2_5 <= 50) {
          // Interpolar entre verde y rojo a través de amarillo y naranja
          int green = map(PMS2_5, 10, 50, 255, 0);
          int red = map(PMS2_5, 10, 50, 0, 255);
          pixels.setPixelColor(0, pixels.Color(red, green, 0));
          pixels.show();
        } else {
          // Rojo
          pixels.setPixelColor(0, pixels.Color(255, 0, 0));
          pixels.show();
        }
      } else {
        pixels.setPixelColor(0, pixels.Color(0, 0, 0));
        pixels.show();
      }
      if (bat > 2) {
        u8g2.setCursor(80, 100);  //
        u8g2.print("Bv ");
        u8g2.print(bat);
        u8g2.sendBuffer();
      }
    }
    ////////////////////////////////////////////////////// visualizacion
    if (indice == 0 and menu == false) {
      int pm25_int = (int)PMS2_5;  // Usando casteo, o puedes usar también static_cast<int>(pm25_level) para más claridad
      pm25_level = PMS2_5;
      imprimirPM("Calidad del Aire", String(pm25_int));
    }
    if (indice == 1 and menu == false) {
      // Acción del botón
      u8g2.clearBuffer();          // clear the internal memory
      int tempe_int = (int)tempe;  // Usando casteo, o puedes usar también static_cast<int>(pm25_level) para más claridad
      u8g2.setDrawColor(0);        // Establece el color de dibujo a negro
      u8g2.drawXBMP(10, 40, termo_width, termo_height, termo_bits);
      u8g2.setDrawColor(1);  // Establece el color de dibujo a blanco
      imprimirPantalla("TEMPERATURA", String(tempe_int));
    }
    if (indice == 2 and menu == false) {
      u8g2.clearBuffer();        // clear the internal memory
      int humi_int = (int)humi;  // Usando casteo, o puedes usar también static_cast<int>(pm25_level) para más claridad
      u8g2.setDrawColor(0);      // Establece el color de dibujo a negro
      u8g2.drawXBMP(10, 40, drop_width, drop_height, drop_bits);
      u8g2.setDrawColor(1);  // Establece el color de dibujo a blanco
      imprimirPantalla("HUMEDAD", String(humi_int));
    }

    if (indice == 3 and menu == false) {
      u8g2.clearBuffer();
      u8g2.setFont(u8g2_font_ncenB08_tr);
      // Mostrar los valores en la pantalla, cada uno separado por 12 píxeles
      u8g2.setCursor(0, 12);  // Comenzar en y = 12
      u8g2.print("Temp: ");
      u8g2.print(tempe);
      u8g2.println(" C");

      u8g2.setCursor(0, 24);  // y = 24
      u8g2.print("RH: ");
      u8g2.print(humi);
      u8g2.println(" %");

      u8g2.setCursor(0, 36);  // y = 36
      u8g2.print("PM1.0: ");
      u8g2.print(PMS1);
      u8g2.println(" ug/m3");

      u8g2.setCursor(0, 48);  // y = 48
      u8g2.print("PM2.5: ");
      u8g2.print(PMS2_5);
      u8g2.println(" ug/m3");

      u8g2.setCursor(0, 60);  // y = 60
      u8g2.print("PM10: ");
      u8g2.print(PMS10);
      u8g2.println(" ug/m3");

      u8g2.setCursor(0, 72);  // y = 72
      u8g2.print("HCHO: ");
      u8g2.print(FMHDSB);
      u8g2.println(" mg/m3");
      if (ext == false) {
        u8g2.setCursor(0, 84);  // y = 84
        u8g2.print("Fecha: ");
        u8g2.println(fecha);
        u8g2.setCursor(0, 96);  // y = 96
        u8g2.print("Hora: ");
        u8g2.println(hora);
        u8g2.setCursor(0, 108);  // y = 96
        u8g2.print("Temprtc: ");
        u8g2.println(rtc.getTemperature());  // Enviar búfer a la pantalla
      } else {
        u8g2.setCursor(0, 84);  // y = 84
        u8g2.print("DHT: ");
        u8g2.print(dhtTemp);
        u8g2.print(" DHH: ");
        u8g2.println(dhtHum);
        u8g2.setCursor(0, 96);  // y = 96
        u8g2.print("BMT: ");
        u8g2.print(tempBMP);
        u8g2.print(" BMH: ");
        u8g2.println(humBMP);
        u8g2.setCursor(0, 108);  //
        u8g2.print("Tcup: ");
        u8g2.print(tempThermo);
        u8g2.print(" Cal: ");
        u8g2.println(calefactorOn);
      }
      u8g2.setCursor(110, 108);  //
      u8g2.print("R:");
      u8g2.print(attempt);
      u8g2.setCursor(0, 120);  //
      u8g2.print("SD:");
      u8g2.print(hasSD);  // Enviar búfer a la pantalla
      u8g2.print(" ID:");
      u8g2.print(ID);
      u8g2.print(" V: ");
      u8g2.println(ver);
      u8g2.sendBuffer();
    }

    if (indice == 4 and menu == false) {
      u8g2.clearBuffer();
      u8g2.setCursor(0, 12);  // Comenzar en y = 12
      u8g2.print("ROTAR");
      u8g2.sendBuffer();
    }

    if (indice == 6 and menu == false) {
      u8g2.setPowerSave(1);
    } else {
      u8g2.setPowerSave(0);
    }
    updateIndex();
    // Al final de tu loop, llama a esta función para indicar que el sistema está funcionando correctamente
    esp_task_wdt_reset();
  }
  if (millis() - lastPauseMillis >= pauseIntervalMillis) {
    lastPauseMillis = millis();
    if (ispause == true) {
      ispause = false;
      Serial.println("3 segundos han pasado, isPaused establecido en false");
    }
  }
}

///////////////////////////////////////////////// fin del loop
String getFecha(DateTime now) {
  char fecha[11];
  sprintf(fecha, "%02d/%02d/%04d", now.day(), now.month(), now.year());
  return String(fecha);
}

String getDiaMes(DateTime now) {
  char fecha[11];
  sprintf(fecha, "%02d/%02d", now.day(), now.month());
  return String(fecha);
}
String getHora(DateTime now) {
  char hora[9];
  sprintf(hora, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
  return String(hora);
}

void drawFooter() {
  // Establece la fuente para el texto del footer
  u8g2.setFont(u8g2_font_fub11_tf);
  // Texto para el footer
  String footerText = hora + " " + fechaDias;
  // Calcula la posición y el tamaño del texto
  int textWidth = u8g2.getStrWidth(footerText.c_str());
  int textHeight = u8g2.getMaxCharHeight();
  // Altura total del rectángulo del footer (añade un pequeño margen)
  int rectHeight = textHeight + 4;
  // Coordenadas y tamaño del rectángulo
  int rectX = 0;
  int rectY = SCREEN_HEIGHT - rectHeight;  // Coloca el rectángulo en la parte inferior de la pantalla
  int rectWidth = SCREEN_WIDTH;            // El ancho total de la pantalla
  // Dibuja el rectángulo del footer
  u8g2.setDrawColor(1);  // Color blanco
  u8g2.drawBox(rectX, rectY, rectWidth, rectHeight);
  // Posiciona el texto dentro del rectángulo
  int textX = (SCREEN_WIDTH - textWidth) / 2;             // Centra el texto horizontalmente
  int textY = rectY + (rectHeight + textHeight) / 2 - 4;  // Ajusta la posición vertical
  // Dibuja el texto del footer
  u8g2.setDrawColor(0);  // Cambia el color a negro para el texto
  u8g2.drawStr(textX, textY, footerText.c_str());

  // Si hasSD es false, dibuja un cuadrado pequeño con el texto "SD NO"
  if (!hasSD) {
    int squareSize = 20;
    int squareX = rectX;               // Coloca el cuadrado a la izquierda del rectángulo del footer
    int squareY = rectY - squareSize;  // Coloca el cuadrado encima del rectángulo del footer
    // Dibuja el cuadrado
    u8g2.setDrawColor(1);  // Cambia el color a blanco para el cuadrado
    u8g2.drawBox(squareX, squareY, squareSize + 8, squareSize);
    // Dibuja el texto "SD NO" dentro del cuadrado
    u8g2.setDrawColor(0);                // Cambia el color a negro para el texto
    u8g2.setFont(u8g2_font_ncenB08_tr);  // Usa una fuente más pequeña para el texto dentro del cuadrado
    int textSquareWidth = u8g2.getStrWidth("SD NO");
    int textSquareX = squareX + (squareSize - textSquareWidth) / 2;
    int textSquareY = squareY + squareSize / 2;  // Ajusta la posición vertical
    u8g2.drawStr(textSquareX, textSquareY, "SD NO");
  }

  // Envía el buffer al display
  u8g2.sendBuffer();
  u8g2.setDrawColor(1);  // Establece el color de dibujo a blanco
}



int calculateActiveParticles(float pm25_level) {
  // La cantidad de partículas activas es proporcional al nivel de PM2.5, con un máximo de MAX_PARTICLES
  int activeParticles = (int)(pm25_level / 50.0 * MAX_PARTICLES);
  return constrain(activeParticles, 1, MAX_PARTICLES);  // Asegúrate de que siempre hay al menos una partícula
}

void inicializarParticulas() {
  for (int i = 0; i < MAX_PARTICLES; i++) {
    reinitializeParticle(i);
  }
}

void dibujarParticulas() {
  int numActiveParticles = calculateActiveParticles(pm25_level);  // Calcula el número de partículas activas basado en PM2.5
  u8g2.setDrawColor(1);                                           // Color blanco para las partículas
  for (int i = 0; i < numActiveParticles; i++) {
    particle_x[i] += particle_speed_x[i] / 10.0;
    particle_y[i] += particle_speed_y[i] / 10.0;
    particle_size[i] *= 0.95;  // Reducción gradual del tamaño
    u8g2.drawDisc(particle_x[i], particle_y[i], particle_size[i]);
    if (particle_x[i] > 128 || particle_x[i] < 0 || particle_y[i] > 128 || particle_y[i] < 0 || particle_size[i] < 0.5) {
      reinitializeParticle(i);
    }
  }
}

void reinitializeParticle(int i) {
  particle_x[i] = random(64 - init_spread, 64 + init_spread);
  particle_y[i] = random(64 - init_spread, 64 + init_spread);
  particle_speed_x[i] = random(-5, 6) * 3;
  particle_speed_y[i] = random(-5, 6) * 3;
  particle_size[i] = calculateInitialParticleSize();
}

float calculateInitialParticleSize() {
  // Escala el tamaño inicial de la partícula según el nivel de PM2.5
  float baseSize = 3;  // Tamaño base cuando PM2.5 es 0
  float maxSize = 5;   // Tamaño máximo cuando PM2.5 es 50 o más
  float sizeFactor = (pm25_level / 40) * (maxSize - baseSize) + baseSize;
  return sizeFactor;
}


void imprimirPM(String titulo, String medida) {
  u8g2.clearBuffer();    // Limpia el buffer de dibujo de U8G2
  u8g2.setDrawColor(1);  // Color blanco
  // Establece la fuente para el título
  u8g2.setFont(u8g2_font_pixzillav1_tr);
  int posX = ((SCREEN_WIDTH - u8g2.getStrWidth(titulo.c_str())) / 2);
  u8g2.drawStr(posX, 24 - 10, titulo.c_str());  // Dibuja el título en el centro, desplazado 10 píxeles hacia arriba
  // Dibuja las partículas flotantes del fondo
  dibujarParticulas();
  // Dibuja el círculo negro detrás del dato
  u8g2.setDrawColor(0);            // Establece el color de dibujo a negro
  u8g2.drawDisc(64, 72 - 10, 32);  // Dibuja el círculo grande, desplazado 10 píxeles hacia arriba

  // Dibuja el valor al centro
  u8g2.setDrawColor(1);                                            // Establece el color de dibujo a blanco
  u8g2.setFont(MissingPlanet72);                                   // Establece la fuente
  byte string_width = u8g2.getStrWidth(medida.c_str());            // Calcula el ancho del string
  u8g2.drawStr(64 - (string_width / 2), 90 - 10, medida.c_str());  // Dibuja el valor de medida centrado y desplazado hacia arriba
  drawFooter();
  // Envía el contenido del buffer al display
  u8g2.sendBuffer();
}

void imprimirPantalla(String titulo, String medida) {
  int posX;
  int posY;
  //hay que transformar de String a const char, mediante el metodo "c_str"
  //titulo
  u8g2.setFont(u8g2_font_pixzillav1_tr);                           // choose a suitable font
  posX = ((SCREEN_WIDTH - u8g2.getStrWidth(titulo.c_str())) / 2);  //centrado horizontal
  u8g2.drawStr(posX, 24 - 10, titulo.c_str());                     // write something to the internal memory
  //medicion
  u8g2.setFont(MissingPlanet72);                                        // choose a suitable font
  posX = ((SCREEN_WIDTH - u8g2.getStrWidth(medida.c_str())) / 2);       //centrado horizontal
  posY = ((SCREEN_HEIGHT + u8g2.getAscent() - u8g2.getDescent()) / 2);  //centrado vertical
  byte string_width = u8g2.getStrWidth(medida.c_str());
  u8g2.drawStr(64 - (string_width / 2), 90 - 10, medida.c_str());  // write something to the internal memory
  drawFooter();
  u8g2.sendBuffer();  // transfer internal memory to the display
}

// Función para actualizar el índice y cambiar la pantalla
void updateIndex() {
  if (millis() - lastUpdateTime >= updateInterval) {
    lastUpdateTime = millis();
    // Incrementar el índice solo si rotate está activado
    if (rotate == true) {
      indice++;
      if (indice > 2) indice = 0;  // Asegúrate de que el índice se reinicie correctamente
    }
  }
}

void sendDataWithChecksum(int number) {
  String data = String(number);  // Convierte el número a una cadena para fácil manipulación
  int checksum = 0;

  // Calcula el checksum sumando los valores ASCII de cada caracter del número
  for (int i = 0; i < data.length(); i++) {
    checksum += data[i];
  }
  checksum %= 256;  // Reduce el checksum con un módulo 256

  char hexChecksum[3];                     // Buffer para almacenar el checksum en formato hexadecimal
  sprintf(hexChecksum, "%02X", checksum);  // Convierte el checksum a hexadecimal

  // Envía los datos por el puerto serial externo
  externo.print(data);
  externo.println(hexChecksum);  // Envía el checksum en una nueva línea
  // // Imprime los datos enviados en el monitor serial del IDE para depuración
  // Serial.println("Datos enviados:");
  // Serial.print(data);
  // Serial.println(hexChecksum);  // Asegúrate de usar println para el checksum para que esté en una nueva línea
}

void processData(String data) {
  float initialTemp;
  int numValues = 6;  // Número de valores esperados en la cadena
  int currentIndex = 0;
  int lastCommaIndex = -1;  // Inicializamos a -1 para manejar el primer segmento correctamente

  for (int i = 0; i < numValues; i++) {
    currentIndex = data.indexOf(',', lastCommaIndex + 1);  // Encuentra la próxima coma
    if (currentIndex == -1) {                              // Si no hay más comas, toma el final de la cadena
      currentIndex = data.length();
    }
    String value = data.substring(lastCommaIndex + 1, currentIndex);
    float floatValue = value.toFloat();  // Convierte el segmento a float

    // Asigna el valor flotante a la variable correspondiente
    switch (i) {
      case 0: initialTemp = floatValue; break;
      case 1: dhtTemp = floatValue; break;
      case 2: tempBMP = floatValue; break;
      case 3: dhtHum = floatValue; break;
      case 4: humBMP = floatValue; break;
      case 5: tempThermo = floatValue; break;
    }
    isUpdated = true;
    lastCommaIndex = currentIndex;  // Actualiza el índice de la última coma encontrada
  }
  // Imprime los valores combinados en el formato especificado, excluyendo el primer valor
  // Serial.println(String(dhtTemp) + "," + String(tempBMP) + "," + String(dhtHum) + "," + String(humBMP) + "," + String(tempThermo));
}

void updateVariable(int newValue) {
  monitoredVariable = newValue;
  lastUpdateTimecheck = millis();  // Reinicia el contador cada vez que la variable es actualizada
  isUpdated = true;                // Marca que la variable ha sido actualizada
}
