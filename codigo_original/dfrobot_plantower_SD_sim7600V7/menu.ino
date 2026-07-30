
void sendDataToServer() {
  Serial.println("Sending data to server");
  urlData.reserve(200);
  urlData = serverName + ID + "&temp=" + tempe + "&hum=" + humi + "&pm1=" + PMS1 + "&pm25=" + datomin10 + "," + datomin9 + "," + datomin8 + "," + datomin7 + "," + datomin6 + "," + datomin5 + "," + datomin4 + "," + datomin3 + "," + datomin2 + "," + datomin1 + "&pm10=" + PMS10 + "&fh=" + FMHDSB;
  Serial.println("URL Data: " + urlData);
  sendDataToServer(urlData);
  Serial.println("Datos enviados al servidor: " + urlData);
  pixels.setPixelColor(0, pixels.Color(255, 0, 255));
  pixels.show();
  delay(100);
}

void showSignalAndOperator() {
  esp_task_wdt_reset();
  Serial.print("operador: ");
  Serial.println(oper);
  Serial.print("señal: ");
  Serial.println(csq);
  Serial.print("Red: ");
  Serial.println(techString);
  // Mostrar los valores en la pantalla, cada uno separado por 12 píxeles
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.setCursor(0, 12);  // Comenzar en y = 12
  u8g2.print("operador:  ");
  u8g2.setCursor(0, 24);  // Comenzar en y = 12
  u8g2.print(oper);
  u8g2.setCursor(0, 36);  // y = 24
  u8g2.print("SEN/AL: ");
  u8g2.setCursor(0, 48);  // Comenzar en y = 12
  u8g2.print(csq);
  u8g2.setCursor(0, 60);  // y = 36
  u8g2.print("Red: ");
  u8g2.print(techString);
  u8g2.sendBuffer();
  delay(5000);
}

void showSensorInfo() {
  esp_task_wdt_reset();
  // Implementar la función para mostrar información del sensor aquí
  Serial.println("Mostrando información del sensor");
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
  Serial.println(ultimoRun);  // Display formaldehyde concentration in mg/m³
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

  u8g2.drawStr(0, 72, ("IP: " + myIPG).c_str());
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
  u8g2.print(" Bv: ");
  u8g2.println(bat);
  u8g2.sendBuffer();
  delay(5000);
}

// Función para manejar el inicio de una pulsación larga
void longPressStartFunction() {
  Serial.println("Botón pulsado largo");
  if (menu) {
    switch (indice) {
      case 0:
        Serial.println("Activando rtcupdate");
        rtcupdate();
        break;
      case 1:
        rgb = !rgb;
        if (rgb) {
          // Verde
          pixels.setPixelColor(0, pixels.Color(0, 255, 0));
          pixels.show();
        } else {
          pixels.setPixelColor(0, pixels.Color(0, 0, 0));
          pixels.show();
        }
        Serial.println(rgb ? "RGB Activo" : "RGB Inactivo");
        break;
      case 2:
        Serial.println("Enviando datos al servidor");
        sendDataToServer();
        pixels.setPixelColor(0, pixels.Color(0, 255, 255));
        pixels.show();
        delay(100);
        break;
      case 3:
        Serial.println("Mostrando señal y operador");
        showSignalAndOperator();
        break;
      case 4:
        Serial.println("Mostrando información del sensor");
        showSensorInfo();
        break;
      case 5:
        Serial.println("Reiniciar ESP");
        deactivateSystem();
        delay(500);
        ESP.restart();
        break;
      case 6:
        Serial.println("Desactivando menú");
        indice = -1;
        menu = false;
        break;
    }
  } else {
    Serial.println("Activando menú");
    menu = true;
  }

  if (lastIndex != indice) {
    Serial.println(menu ? "Menú Activo" : "Menú Inactivo");
    Serial.println("Índice Actual: " + String(indice));
    lastIndex = indice;  // Actualizar el último índice
  }
}

void clickFunction() {
  Serial.println("Clicked!");
  Serial.println("Current Index before increment: " + String(indice));
  indice = indice + 1;
  if (indice > 6) indice = 0;

  // if (indice > 6 && !menu) {
  //   indice = 0;
  // }
  // if (indice > 6 && menu) {
  //   indice = 0;
  // }
  if (indice == 4 && !menu) {
    rotate = true;
    Serial.println("Rotation activated");
  } else {
    rotate = false;
  }
  if (lastIndex != indice) {
    Serial.println("Current Index after increment: " + String(indice));
    lastIndex = indice;  // Actualizar el último índice
  }
}

void drawMenu() {
  esp_task_wdt_reset();
  // Acción del botón
  btn.tick();
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_fub11_tf);
  int menuHeight = 12;
  int numOptions = 7;
  int menuTotalHeight = numOptions * menuHeight;
  int menuY = (SCREEN_HEIGHT - menuTotalHeight) / 2;

  u8g2.setDrawColor(1);
  u8g2.drawBox(0, menuY + menuHeight * indice, SCREEN_WIDTH, menuHeight);

  String options[] = { "rtc update", "apagar led", "mandar mensaje", "S/n y operador", "info sensor", "RESET", "volver" };
  for (int i = 0; i < numOptions; i++) {
    int textX = 2;  // Alinea el texto a la izquierda
    int textY = menuY + menuHeight * i + menuHeight - 2;
    u8g2.setDrawColor(i == indice ? 0 : 1);
    u8g2.drawStr(textX, textY, options[i].c_str());
    if (lastIndex != indice) {
      Serial.println("Option " + String(i) + ": " + options[i] + (i == indice ? " (Selected)" : ""));
    }
  }

  u8g2.sendBuffer();
  if (lastIndex != indice) {
    Serial.println("Menu drawn with current index: " + String(indice));
    lastIndex = indice;  // Actualizar el último índice
  }
}