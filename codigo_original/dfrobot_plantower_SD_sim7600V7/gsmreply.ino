//////////////////////////////// datos de señal y operador
String oper;
int csq;
String techString;

/////////////////////////////////////////////////////////////////////////////////////////////// Parámetros de reintento

bool connectToNetwork() {
  // Obtener y mostrar la información de la red
  oper = modem.getOperator();
  Serial.print("Operador: ");
  Serial.println(oper);
  // Obtener y mostrar la intensidad de la señal
  csq = modem.getSignalQuality();
  Serial.print("Intensidad de la señal: ");
  Serial.print(csq);
  Serial.println(" dBm");
  // Obtener y mostrar la tecnología de acceso (2G, 3G, 4G)
  getNetworkTechnology();

  for (attempt = 1; attempt <= MAX_RETRIES; ++attempt) {
    Serial.print("Intento de conexión ");
    Serial.print(attempt);
    Serial.print(" de ");
    Serial.println(MAX_RETRIES);

    if (modem.waitForNetwork()) {
      Serial.println("Red móvil conectada.");
      if (modem.gprsConnect(apn, gprsUser, gprsPass)) {
        Serial.println("Conexión GPRS exitosa.");
        u8g2.setCursor(0, 64);  // y = 96
        u8g2.print("GPRS exitosa");
        u8g2.sendBuffer();
        return true;
      } else {
        Serial.println("Falló la conexión GPRS.");
        u8g2.setCursor(0, 64);  // y = 96
        u8g2.print("GPRS FALLO");
        u8g2.sendBuffer();
      }
    } else {
      Serial.println("Falló la conexión a la red móvil.");
      u8g2.setCursor(0, 64);  // y = 96
      u8g2.print("GPRS FALLO");
      u8g2.sendBuffer();
    }

    Serial.print("Esperando ");
    Serial.print(RETRY_DELAY_MS);
    Serial.println(" ms antes de reintentar...");
    delay(RETRY_DELAY_MS);
  }

  Serial.println("No se pudo conectar después de ");
  Serial.print(MAX_RETRIES);
  Serial.println(" intentos.");

  return false;
}
/////////////////////////////////////////////////////////////////////
void readSerialResponse() {
  while (SerialAT.available()) {
    char c = SerialAT.read();
    responseBuffer += c;
    if (c == '\n' || c == '\r') {  // Manejar tanto '\n' como '\r'
      Serial.println("Serial Response: " + responseBuffer);
      if (responseBuffer.startsWith("+HTTPACTION:")) {
        handleHTTPAction(responseBuffer, awaitingData, dataLength);
      } else if (responseBuffer.startsWith("Content-Length:")) {
        int start = responseBuffer.indexOf("Content-Length:") + 15;
        int end = responseBuffer.indexOf('\r', start);
        dataLength = responseBuffer.substring(start, end).toInt();
        Serial.println("Handling response with  desde el readserial" + String(dataLength) + " bytes of data.");
        modem.sendAT("+HTTPREAD=0," + String(dataLength));
      } else if (responseBuffer.startsWith("+HTTPREAD:")) {
        // aqui iria alguna funcion?? handleHTTPRead(responseBuffer);
      } else if (isDigit(responseBuffer.charAt(0))) {  // Verificar si el primer carácter es un dígito
        Serial.println("Response starts with a number: " + responseBuffer);
        // Aquí puedes manejar la respuesta que comienza con un número
        DateTime now = rtc.now();
        int serverUnixTime = responseBuffer.toInt();
        Serial.println("Timestamp Unix del servidor: " + String(serverUnixTime));

        long localUnixTime = now.unixtime();
        Serial.print("Hora local (Unix): ");
        Serial.println(localUnixTime);
        // Comparar las horas
        pixels.setPixelColor(0, pixels.Color(255, 0, 255));
        pixels.show();
        delay(500);
        if (localUnixTime > serverUnixTime) {
          Serial.println("La hora local es menor que la hora del servidor.");
          DateTime dt = DateTime(serverUnixTime);
          rtc.adjust(dt);
          Serial.println("actualizando hora...");
          closeHttpSession();
        } else if (localUnixTime < serverUnixTime) {
          Serial.println("La hora local es menor que la hora del servidor.");
          DateTime dt = DateTime(serverUnixTime);
          rtc.adjust(dt);
          Serial.println("actualizando hora...");
          closeHttpSession();
        } else {
          Serial.println("La hora local es igual a la hora del servidor.");
          closeHttpSession();
        }
      } else if (responseBuffer.startsWith("OK")) {
        Serial.println("Received OK from server");
        operationSuccess = true;
        if (menu == true) {
           u8g2.setCursor(0, 120);  // y = 36
          u8g2.print("ok gsm ");
          u8g2.sendBuffer();
          pixels.setPixelColor(0, pixels.Color(255, 0, 255));
          pixels.show();
          delay(1000);
          pixels.setPixelColor(0, pixels.Color(0, 0, 0));
          pixels.show();
          delay(1000);
          pixels.setPixelColor(0, pixels.Color(255, 0, 255));
          pixels.show();
          delay(1000);
          closeHttpSession();
        }
      }
      responseBuffer = "";  // Limpiar el buffer después de procesar cada línea
    }
  }
}

void handleHTTPAction(const String &response, bool &awaitingData, int &dataLength) {
  Serial.println("Handling HTTPACTION response...");
  if (response.indexOf("200,") != -1) {
    int lengthIndex = response.lastIndexOf(',') + 1;
    dataLength = response.substring(lengthIndex).toInt();
    if (dataLength > 0) {
      Serial.println("HTTP GET Success, awaiting " + String(dataLength) + " bytes of data...");
      modem.sendAT("+HTTPREAD=0," + String(dataLength));
      awaitingData = true;
    }
  } else {
    Serial.println("HTTP GET Failed.");
    pixels.setPixelColor(0, pixels.Color(255, 0, 0));
    pixels.show();
    delay(2000);
  }
}

void rtcupdate() {
  // Iniciar la conexión HTTP
  modem.sendAT("+HTTPINIT");

  if (modem.waitResponse(5000) != 1) {
    Serial.println("HTTPINIT failed");
    //  closeHttpSession();
    //  return;
  }
  // Establecer el URL del servidor
  modem.sendAT("+HTTPPARA=\"URL\",\"https://southamerica-west1-fic-aysen-412113.cloudfunctions.net/unixTime\"");

  if (modem.waitResponse(50000) != 1) {
    Serial.println("HTTPPARA URL failed");
    //closeHttpSession();
    //return;
  }

  // Iniciar la acción HTTP GET
  modem.sendAT("+HTTPACTION=0");

  if (modem.waitResponse(50000) != 1) {
    Serial.println("HTTPACTION failed");
    //  closeHttpSession();
    //  return;
  }
  readSerialResponse();
}

void closeHttpSession() {
  modem.sendAT("+HTTPTERM");
  modem.waitResponse(30000L);
}

void sendDataToServer(String urlData) {
  bool success = false;
  int retryCount = 0;
  const int maxRetries = 1;  //originalmente eran 3

  Serial.println("Intentando enviar datos al servidor...");
  while (!success && retryCount < maxRetries) {
    modem.sendAT("+HTTPINIT");

    if (modem.waitResponse(50000) != 1) {
      Serial.println("HTTPINIT failed");
      retryCount++;
      closeHttpSession();
      continue;
    }

    String fullURL = urlData;
    modem.sendAT("+HTTPPARA=\"URL\",\"" + fullURL + "\"");


    if (modem.waitResponse(10000) != 1) {
      Serial.println("HTTPPARA URL failed");
      retryCount++;
      closeHttpSession();
      continue;
    }

    // No es necesario enviar datos JSON, así que se procede directamente a la acción HTTP
    modem.sendAT("+HTTPACTION=0");  // 0 para GET, 1 para POST esto se cambio para hacer el post y no el get sigo sin tener buen manejo de la respuesta

    if (modem.waitResponse(10000) != 1) {
      Serial.println("HTTPPARA URL failed");
      retryCount++;
      closeHttpSession();
      continue;
    }
    if (!success) {
      Serial.println("Failed to send data after retries.");
      closeHttpSession();  //////////////////importante revisar para consumo
    }
  }
  closeHttpSession();  //////////////////importante revisar para consumo
}

void resetModem() {
  // Enviar comando AT para reiniciar el módem
  modem.sendAT("+CRESET");
  u8g2.setCursor(0, 66);  // y = 96
  u8g2.print("MODEM RESET");
  u8g2.sendBuffer();
  delay(5000);  // Espera a que el módem se reinicie completamente
  // Reiniciar la conexión
  modem.init();
  u8g2.setCursor(0, 66);  // y = 96
  u8g2.print("MODEM INIT");
  u8g2.sendBuffer();
  if (modem.gprsConnect(apn, gprsUser, gprsPass)) {
    Serial.println("GPRS connection failed after modem reset");
    // Considera realizar acciones adicionales si la conexión falla
  }
}

void getNetworkTechnology() {
  SerialAT.println("AT+COPS?");
  delay(100);
  while (SerialAT.available()) {
    String response = SerialAT.readStringUntil('\n');
    response.trim();
    if (response.startsWith("+COPS:")) {
      Serial.print("Respuesta de COPS: ");
      Serial.println(response);

      // Analizar la respuesta para determinar la tecnología
      int tech = response.charAt(response.length() - 1) - '0';
      techString;
      switch (tech) {
        case 0:
          techString = "GSM";
          break;
        case 1:
          techString = "GSM Compact";
          break;
        case 2:
          techString = "UTRAN (3G)";
          break;
        case 3:
          techString = "GSM w/EGPRS (EDGE)";
          break;
        case 4:
          techString = "UTRAN w/HSDPA (HSPA)";
          break;
        case 5:
          techString = "UTRAN w/HSUPA (HSPA)";
          break;
        case 6:
          techString = "UTRAN w/HSDPA and HSUPA (HSPA+)";
          break;
        case 7:
          techString = "E-UTRAN (LTE)";
          break;
        default:
          techString = "Desconocido";
          break;
      }
      Serial.print("Tecnología de acceso: ");
      Serial.println(techString);
    }
  }
}
