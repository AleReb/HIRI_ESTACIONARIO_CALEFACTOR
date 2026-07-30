/*
To retrieve the contents of the SD card, visit http://esp32sd.local/list?dir=/
  The 'dir' parameter needs to be passed to the PrintDirectory function via an HTTP GET request.
*/

void handleFileDownload() {
  status = false;
  File file = SD.open(ARCHIVO);
  if (!file) {
    server.send(404, "text/plain", "File not found");
    return;
  }
  size_t fileSize = file.size();
  float fileSizeMB = fileSize / (1024.0 * 1024.0);  // Tamaño del archivo en MB
  file.close();
  String html = "<html><head><title>Descargar Archivo</title>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; display: flex; justify-content: center; align-items: center; height: 100vh; margin: 0; background-color: #f0f0f0; position: relative; }";
  html += ".container { background-color: #fff; padding: 20px; border-radius: 8px; box-shadow: 0 0 10px rgba(0, 0, 0, 0.1); max-width: 400px; width: 100%; text-align: center; }";
  html += "h1 { font-size: 24px; margin-bottom: 20px; }";
  html += "p { margin: 8px 0; font-size: 16px; }";
  html += ".buttons { display: flex; justify-content: center; gap: 8px; margin-top: 20px; }";
  html += "button { padding: 10px 20px; border: none; border-radius: 5px; background-color: #000; color: #fff; cursor: pointer; font-size: 16px; }";
  html += "button:hover { background-color: #333; }";
  html += ".logo { position: absolute; top: 10px; left: 10px; }";
  html += "</style></head><body>";
  html += "<div class='logo'>" + String(svgLogo) + "</div>";
  html += "<div class='container'>";
  html += "<h1>Descargar Datos</h1>";
  html += "<p>File Name: " + ARCHIVO + "</p>";
  html += "<p>File Size: " + String(fileSizeMB, 2) + " MB</p>";
  html += "<div class='buttons'>";
  html += "<a href='" + ARCHIVO + "?download=true'><button>DESCARGAR""</button></a>";
  html += "<button onclick=\"location.href='/estatus.html'\">ESTADO DEL SENSOR</button>";
  html += "</div>";
  html += "</div></body></html>";
  server.send(200, "text/html", html);
}


void returnOK() {
  server.send(200, "text/plain", "");
}

void returnFail(String msg) {
  server.send(500, "text/plain", msg + "\r\n");
}

bool loadFromSdCard(String path) {
  status = false;
  String dataType = "text/plain";
  // if (path.endsWith("/")) {
  //   path += "index.htm";
  //   status = false;
  // }

  if (path.endsWith(".src")) {
    path = path.substring(0, path.lastIndexOf("."));
  } else if (path.endsWith(".htm")) {
    dataType = "text/html";
  } else if (path.endsWith(".css")) {
    dataType = "text/css";
  } else if (path.endsWith(".js")) {
    dataType = "application/javascript";
  } else if (path.endsWith(".png")) {
    dataType = "image/png";
  } else if (path.endsWith(".gif")) {
    dataType = "image/gif";
  } else if (path.endsWith(".jpg")) {
    dataType = "image/jpeg";
  } else if (path.endsWith(".ico")) {
    dataType = "image/x-icon";
  } else if (path.endsWith(".xml")) {
    dataType = "text/xml";
  } else if (path.endsWith(".pdf")) {
    dataType = "application/pdf";
  } else if (path.endsWith(".zip")) {
    dataType = "application/zip";
  } else if (path.endsWith(".csv")) {
    dataType = "text/csv";
  }
  File dataFile = SD.open(path.c_str());
  if (dataFile.isDirectory()) {
    path += "/index.htm";
    dataType = "text/html";
    dataFile = SD.open(path.c_str());
  }
  if (!dataFile) {
    return false;
  }

  if (server.hasArg("download")) {
    dataType = "application/octet-stream";
  }

  if (server.streamFile(dataFile, dataType) != dataFile.size()) {
    Serial.println("Sent less data than expected!");
  }

  dataFile.close();
  return true;
}

void handleFileUpload() {
  if (server.uri() != "/edit") {
    return;
  }
  HTTPUpload &upload = server.upload();
  static size_t totalUploadSize = 0;
  static size_t currentUploadSize = 0;

  if (upload.status == UPLOAD_FILE_START) {
    if (SD.exists((char *)upload.filename.c_str())) {
      SD.remove((char *)upload.filename.c_str());
    }
    uploadFile = SD.open(upload.filename.c_str(), FILE_WRITE);
    totalUploadSize = upload.totalSize;
    currentUploadSize = 0;

    Serial.print("Upload: START, filename: ");
    Serial.println(upload.filename);
    u8g2.clearBuffer();
    u8g2.drawStr(0, 10, "Upload: START");
    u8g2.drawStr(0, 30, upload.filename.c_str());
    u8g2.sendBuffer();
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (uploadFile) {
      uploadFile.write(upload.buf, upload.currentSize);
      currentUploadSize += upload.currentSize;
      Serial.print("Upload: WRITE, Bytes: ");
      Serial.println(upload.currentSize);
      u8g2.clearBuffer();
      u8g2.drawStr(0, 10, "Uploading...");
      u8g2.drawStr(0, 30, ("Uploaded: " + String(currentUploadSize)).c_str());
      u8g2.drawStr(0, 50, ("Total: " + String(totalUploadSize)).c_str());
      u8g2.sendBuffer();
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (uploadFile) {
      uploadFile.close();
    }
    Serial.print("Upload: END, Size: ");
    Serial.println(upload.totalSize);
    u8g2.clearBuffer();
    u8g2.drawStr(0, 10, "Upload: END");
    u8g2.drawStr(0, 30, ("Total: " + String(totalUploadSize)).c_str());
    u8g2.sendBuffer();
    delay(3000);
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.drawStr(0, 10, "System Activated");
    u8g2.drawStr(0, 30, ("SSID: " + ssid).c_str());
    u8g2.drawStr(0, 50, ("IP: " + myIPG).c_str());
    u8g2.drawStr(0, 70, ("PASS: " + password).c_str());
    u8g2.drawStr(0, 85, (ver).c_str());
    u8g2.sendBuffer();
  }
}

void deleteRecursive(String path) {
  File file = SD.open((char *)path.c_str());
  if (!file.isDirectory()) {
    file.close();
    SD.remove((char *)path.c_str());
    return;
  }
  file.rewindDirectory();
  while (true) {
    File entry = file.openNextFile();
    if (!entry) {
      break;
    }
    String entryPath = path + "/" + entry.name();
    if (entry.isDirectory()) {
      entry.close();
      deleteRecursive(entryPath);
    } else {
      entry.close();
      SD.remove((char *)entryPath.c_str());
    }
    yield();
  }
  SD.rmdir((char *)path.c_str());
  file.close();
}

void handleDelete() {
  if (server.args() == 0) {
    return returnFail("BAD ARGS");
  }
  String path = server.arg(0);
  if (path == "/" || !SD.exists((char *)path.c_str())) {
    returnFail("BAD PATH");
    return;
  }
  deleteRecursive(path);
  returnOK();
}

void handleCreate() {
  if (server.args() == 0) {
    return returnFail("BAD ARGS");
  }
  String path = server.arg(0);
  if (path == "/" || SD.exists((char *)path.c_str())) {
    returnFail("BAD PATH");
    return;
  }

  if (path.indexOf('.') > 0) {
    File file = SD.open((char *)path.c_str(), FILE_WRITE);
    if (file) {
      file.write(0);
      file.close();
    }
  } else {
    SD.mkdir((char *)path.c_str());
  }
  returnOK();
}

void handleRename() {
  if (server.args() < 2) {
    return returnFail("BAD ARGS");
  }
  String oldName = server.arg(0);
  String newName = server.arg(1);
  if (!SD.exists((char *)oldName.c_str())) {
    returnFail("File does not exist");
    return;
  }
  if (SD.exists((char *)newName.c_str())) {
    returnFail("New name already exists");
    return;
  }
  if (SD.rename(oldName.c_str(), newName.c_str())) {
    returnOK();
  } else {
    returnFail("Rename failed");
  }
}

void printDirectory() {
  if (!server.hasArg("dir")) {
    return returnFail("BAD ARGS");
  }
  String path = server.arg("dir");
  if (path != "/" && !SD.exists((char *)path.c_str())) {
    return returnFail("BAD PATH");
  }
  File dir = SD.open((char *)path.c_str());
  path = String();
  if (!dir.isDirectory()) {
    dir.close();
    return returnFail("NOT DIR");
  }
  dir.rewindDirectory();
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/json", "");
  WiFiClient client = server.client();

  server.sendContent("[");
  for (int cnt = 0; true; ++cnt) {
    File entry = dir.openNextFile();
    if (!entry) {
      break;
    }

    String output;
    if (cnt > 0) {
      output = ',';
    }
    output += "{\"type\":\"";
    output += (entry.isDirectory()) ? "dir" : "file";
    output += "\",\"name\":\"";
    output += entry.path();
    output += "\",\"size\":\"";
    output += entry.size();
    output += "\"}";
    server.sendContent(output);
    entry.close();
  }
  server.sendContent("]");
  dir.close();
}

void handleNotFound() {
  if (hasSD && loadFromSdCard(server.uri())) {
    status = false;
    return;
  }
  String message = "SDCARD Not Detected o bad path\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " NAME:" + server.argName(i) + "\n VALUE:" + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
  Serial.print(message);
}

void performUpdate(Stream &updateSource, size_t updateSize) {
  if (Update.begin(updateSize)) {
    size_t written = Update.writeStream(updateSource);
    if (written == updateSize) {
      Serial.println("Written : " + String(written) + " successfully");
    } else {
      Serial.println("Written only : " + String(written) + "/" + String(updateSize) + ". Retry?");
    }
    if (Update.end()) {
      Serial.println("OTA done!");
      if (Update.isFinished()) {
        Serial.println("Update successfully completed. Rebooting.");
        u8g2.clearBuffer();
        u8g2.drawStr(0, 10, "Update completed!");
        u8g2.sendBuffer();
        delay(2000);

        if (SD.rename("/update.bin", "/update.bak")) {
          Serial.println("Firmware renamed successfully!");
          u8g2.clearBuffer();
          u8g2.drawStr(0, 10, "Firmware renamed");
          u8g2.sendBuffer();
        } else {
          Serial.println("Firmware rename error!");
          u8g2.clearBuffer();
          u8g2.drawStr(0, 10, "Rename error!");
          u8g2.sendBuffer();
          Serial.println("Removiendo update...");
          SD.remove("/update.bin");
        }

        ESP.restart();
      } else {
        Serial.println("Update not finished? Something went wrong!");
        u8g2.clearBuffer();
        u8g2.drawStr(0, 10, "Update not finished!");
        u8g2.sendBuffer();
      }
    } else {
      Serial.println("Error Occurred. Error #: " + String(Update.getError()));
      u8g2.clearBuffer();
      u8g2.drawStr(0, 10, ("Update Error #: " + String(Update.getError())).c_str());
      u8g2.sendBuffer();
    }
  } else {
    Serial.println("Not enough space to begin OTA");
    u8g2.clearBuffer();
    u8g2.drawStr(0, 10, "Not enough space");
    u8g2.sendBuffer();
  }
}

void updateFromFS(fs::FS &fs) {
  File updateBin = fs.open("/update.bin");
  if (updateBin) {
    if (updateBin.isDirectory()) {
      Serial.println("Error, update.bin is not a file");
      updateBin.close();
      return;
    }
    size_t updateSize = updateBin.size();
    if (updateSize > 0) {
      Serial.println("Try to start update");
      performUpdate(updateBin, updateSize);
    } else {
      Serial.println("Error, file is empty");
      u8g2.clearBuffer();
      u8g2.drawStr(0, 10, "File is empty");
      u8g2.sendBuffer();
    }

    updateBin.close();

    if (SD.rename("/update.bin", "/update.bak")) {
      Serial.println("Firmware renamed successfully!");
      u8g2.clearBuffer();
      u8g2.drawStr(0, 10, "Firmware renamed");
      u8g2.sendBuffer();
    } else {
      Serial.println("Firmware rename error!");
      u8g2.clearBuffer();
      u8g2.drawStr(0, 10, "Rename error!");
      // whe finished remove the binary from sd card to indicate end of the process
      Serial.println("Removiendo update...");
      SD.remove("/update.bin");
      u8g2.sendBuffer();
    }
  } else {
    Serial.println("Could not load update.bin from sd root");
    u8g2.clearBuffer();
    u8g2.drawStr(0, 10, "Firmware not found!");
    u8g2.sendBuffer();
  }
}

void activateSystem() {
  Serial.println("Activating system...");
  // Configura el AP con el SSID basado en la dirección MAC
  String macAddress = WiFi.macAddress();
  macAddress.replace(":", "");  // Elimina los dos puntos de la dirección MAC
  ssid = "MP" + ID + "-" + macAddress;
  password = macAddress;  // Establece la contraseña como la dirección MAC
  WiFi.softAP(ssid.c_str(), password.c_str());
  IPAddress myIP = WiFi.softAPIP();
  myIPG = myIP.toString();
  Serial.print("AP IP address: ");
  Serial.println(myIP);
  Serial.print("SSID: ");
  Serial.println(ssid);
  Serial.println("PASS: " + password);
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(0, 10, "System Activated");
  u8g2.drawStr(0, 30, ("SSID: " + ssid).c_str());
  u8g2.drawStr(0, 50, ("IP: " + myIPG).c_str());
  u8g2.drawStr(0, 70, ("PASS: " + password).c_str());
  u8g2.drawStr(0, 85, (ver).c_str());
  u8g2.sendBuffer();
  if (MDNS.begin("pms32")) {
    MDNS.addService("http", "tcp", 80);
    Serial.println("MDNS responder started");
    Serial.println("You can now connect to http://pms32.local");
  }


  // nueva funcion para ver en tiempo real los datos
  server.on("/estatus.html", HTTP_GET, handleStatus);
  server.on("/data", HTTP_GET, handleData);
  //funciones rtc y mandar mensaje
  server.on("/sendData", HTTP_GET, handleSendData);
  server.on("/rtcupdate", HTTP_GET, handleRtcUpdate);
  //funciones de SD
  server.on("/list", HTTP_GET, printDirectory);
  server.on("/edit", HTTP_DELETE, handleDelete);
  server.on("/edit", HTTP_PUT, handleCreate);
  server.on(
    "/edit", HTTP_POST, []() {
      returnOK();
    },
    handleFileUpload);
  server.on("/updateFirmware", HTTP_GET, []() {
    server.send(200, "text/html", "<html><head><title>Actualizando</title></head><body><h1>actualizando update.bin</h1><p>Actualizando espere a que reinicie... si no vuelva</p><button onclick=\"location.href='/estatus.html'\">Back to Status</button></body></html>");
    //server.send(200, "text/plain", "Update initiated");
    updateFromFS(SD);
  });
  server.on("/", handleFileDownload);
  server.on("/descarga", handleFileDownload);
  server.on("/rename", HTTP_POST, handleRename);
  server.onNotFound(handleNotFound);
  server.on("/reset", HTTP_GET, handleReset);  // Ruta para manejar el reinicio
  server.begin();
  Serial.println("HTTP server started");

  spiSD.begin(SD_SCLK, SD_MISO, SD_MOSI, SD_CS_PIN);
  if (!SD.begin(SD_CS_PIN, spiSD)) {
    Serial.println("Fallo al inicializar la tarjeta SD");
    hasSD = false;
    u8g2.setCursor(0, 100);  // y = 96
    u8g2.print("SD NO");
    u8g2.sendBuffer();
  } else {
    hasSD = true;
    Serial.println("Tarjeta SD inicializada correctamente");
    u8g2.setCursor(0, 100);  // y = 9
    u8g2.print("SD OK");
    u8g2.sendBuffer();
    if (SD.exists(ARCHIVO) == false) {
      Serial.println("Archivo base no existe...");
      delay(500);
      Serial.println("creando archivos");
      String tosave = "FECHA,HORA,TEMPERATURA,HUMEDAD,PM1,PM2_5,PM10,FORMAL,DHT22TEMP,DHT22HUM,BMP280TEMP,BMP280HUM,TERMOCUPLA \n";
      writeFile(SD, ARCHIVO.c_str(), tosave.c_str());
    } else {
      Serial.println("dato base es:" + ARCHIVO);
    }
  }
  if (SD.exists("/update.bin") == true) {
    Serial.println("archivo para actualizar... actualizando");
    updateFromFS(SD);
  } else {
    Serial.println("sin update");
  }
  systemActive = true;
}

void deactivateSystem() {
  Serial.println("Deactivating system...");
  server.close();
  SD.end();
  digitalWrite(SD_MOSI, LOW);
  digitalWrite(SD_SCLK, LOW);
  digitalWrite(SD_CS_PIN, LOW);
  delay(100);  // Espera un poco para asegurarte de que los pines estén en estado bajo
  MDNS.end();
  WiFi.softAPdisconnect(true);
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(0, 10, "System Deactivated");
  u8g2.sendBuffer();
  systemActive = false;
}

void handleSendData() {
  // Lógica para enviar datos al servidor
  sendDataToServer();
  // Responder con el estado de la operación
  String html = "<html><head><title>Send Data Response</title>";
  html += "<meta http-equiv='refresh' content='3;url=/sendData'>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; display: flex; justify-content: center; align-items: center; height: 100vh; margin: 0; background-color: #f0f0f0; position: relative; }";
  html += ".container { background-color: #fff; padding: 20px; border-radius: 8px; box-shadow: 0 0 10px rgba(0, 0, 0, 0.1); max-width: 400px; width: 100%; text-align: center; }";
  html += "h1 { font-size: 24px; margin-bottom: 20px; }";
  html += "p { margin: 8px 0; font-size: 16px; }";
  html += "button { padding: 10px 20px; border: none; border-radius: 5px; background-color: #000; color: #fff; cursor: pointer; font-size: 16px; margin-top: 20px; }";
  html += "button:hover { background-color: #333; }";
  html += ".logo { position: absolute; top: 10px; left: 10px; }";
  html += "</style></head><body>";
  html += "<div class='logo'>" + String(svgLogo) + "</div>";
  html += "<div class='container'>";
  html += "<h1>Send Data Response</h1>";
  if (operationSuccess) {
    html += "<p>Data sent successfully!</p>";
  } else {
    html += "<p>Failed to send data. Please wait 3 seconds and refresh the page.</p>";
  }
  html += "<button onclick=\"location.href='/estatus.html'\">Back to Status</button>";
  html += "</div></body></html>";
  server.send(200, "text/html", html);
}

void handleRtcUpdate() {
  // Lógica para actualizar RTC
  rtcupdate();

  // Redirige a la página de respuesta
  String html = "<html><head><title>RTC Update Response</title>";
  html += "<meta http-equiv='refresh' content='3;url=/rtcupdate'>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; display: flex; justify-content: center; align-items: center; height: 100vh; margin: 0; background-color: #f0f0f0; position: relative; }";
  html += ".container { background-color: #fff; padding: 20px; border-radius: 8px; box-shadow: 0 0 10px rgba(0, 0, 0, 0.1); max-width: 400px; width: 100%; text-align: center; }";
  html += "h1 { font-size: 24px; margin-bottom: 20px; }";
  html += "p { margin: 8px 0; font-size: 16px; }";
  html += "button { padding: 10px 20px; border: none; border-radius: 5px; background-color: #000; color: #fff; cursor: pointer; font-size: 16px; margin-top: 20px; }";
  html += "button:hover { background-color: #333; }";
  html += ".logo { position: absolute; top: 10px; left: 10px; }";
  html += "</style></head><body>";
  html += "<div class='logo'>" + String(svgLogo) + "</div>";
  html += "<div class='container'>";
  html += "<h1>RTC Update Response</h1>";
  if (operationSuccess) {
    html += "<p>RTC updated successfully!</p>";
  } else {
    html += "<p>Failed to update RTC. Please wait 3 seconds and refresh the page.</p>";
  }
  html += "<button onclick=\"location.href='/estatus.html'\">Back to Status</button>";
  html += "</div></body></html>";
  server.send(200, "text/html", html);
}

// Función para manejar el reinicio de la ESP32
void handleReset() {
  server.send(200, "text/plain", "Resetting ESP32...");
  delay(1000);
  ESP.restart();
}

void handleStatus() {
  // connectToNetwork();
  status = true;
  String html = "<html><head><title>Status</title>";
  html += "<script>";
  html += "function fetchData() {";
  html += "  var xhr = new XMLHttpRequest();";
  html += "  xhr.onreadystatechange = function() {";
  html += "    if (xhr.readyState == 4 && xhr.status == 200) {";
  html += "      var data = JSON.parse(xhr.responseText);";
  html += "      document.getElementById('fecha').innerHTML = data.fecha;";
  html += "      document.getElementById('hora').innerHTML = data.hora;";
  html += "      document.getElementById('tempe').innerHTML = data.tempe + ' &deg;C';";
  html += "      document.getElementById('humi').innerHTML = data.humi + ' %';";
  html += "      document.getElementById('PMS1').innerHTML = data.PMS1 + ' µg/m³';";
  html += "      document.getElementById('PMS2_5').innerHTML = data.PMS2_5 + ' µg/m³';";
  html += "      document.getElementById('PMS10').innerHTML = data.PMS10 + ' µg/m³';";
  html += "      document.getElementById('FMHDSB').innerHTML = data.FMHDSB + ' ppm';";
  html += "      document.getElementById('firmwareVersion').innerHTML = data.firmwareVersion;";
  html += "      document.getElementById('deviceID').innerHTML = data.deviceID;";
  html += "      document.getElementById('operador').innerHTML = data.operador;";
  html += "      document.getElementById('senial').innerHTML = data.senial;";
  html += "      document.getElementById('red').innerHTML = data.red;";
  html += "      if (data.ext) {";
  html += "        document.getElementById('dhtTemp').innerHTML = data.dhtTemp + ' &deg;C';";
  html += "        document.getElementById('dhtHum').innerHTML = data.dhtHum + ' %';";
  html += "        document.getElementById('tempBMP').innerHTML = data.tempBMP + ' &deg;C';";
  html += "        document.getElementById('humBMP').innerHTML = data.humBMP + ' %';";
  html += "        document.getElementById('tempThermo').innerHTML = data.tempThermo + ' &deg;C';";
  html += "        document.getElementById('extendedData').style.display = '';";
  html += "      } else {";
  html += "        document.getElementById('extendedData').style.display = 'none';";
  html += "      }";
  html += "      document.getElementById('sdStatus').innerHTML = data.hasSD ? 'Presente' : 'No presente';";
  html += "      document.getElementById('buttonContainer').innerHTML = data.hasSD ? '<button onclick=\"location.href=\\'index.htm\\'\">Go to Index SD</button>' : '<button onclick=\"resetESP()\">Reset ESP32</button>';";
  html += "      document.getElementById('downloadButton').innerHTML = '<button onclick=\"location.href=\\'/descarga\\'\">Download File</button>';";
  html += "    }";
  html += "  };";
  html += "  xhr.open('GET', '/data', true);";
  html += "  xhr.send();";
  html += "}";
  html += "function resetESP() {";
  html += "  var xhr = new XMLHttpRequest();";
  html += "  xhr.open('GET', '/reset', true);";
  html += "  xhr.send();";
  html += "}";
  html += "setInterval(fetchData, 5000);";  // Actualiza cada 5 segundos
  html += "</script>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; margin: 0; padding: 0; background-color: #f0f0f0; }";
  html += ".grid-container { display: grid; grid-template-columns: repeat(auto-fit, minmax(250px, 1fr)); gap: 20px; padding: 20px; }";
  html += ".card { background-color: #fff; padding: 20px; border-radius: 8px; box-shadow: 0 0 10px rgba(0, 0, 0, 0.1); }";
  html += ".card h2 { font-size: 18px; margin-bottom: 15px; }";
  html += ".card p { margin: 8px 0; font-size: 14px; }";
  html += ".button-container { display: flex; gap: 8px; }";
  html += "button { padding: 10px 20px; border: none; border-radius: 5px; background-color: #000; color: #fff; cursor: pointer; font-size: 14px; }";
  html += "button:hover { background-color: #333; }";
  html += ".logo { position: absolute; top: 10px; left: 10px; }";
  html += "</style></head><body onload='fetchData()'>";
  html += "<div class='logo'><svg width=\"35\" height=\"35\" viewBox=\"0 0 35 35\" fill=\"none\" xmlns=\"http://www.w3.org/2000/svg\"><path d=\"M8.75299 17.4999C8.77991 18.5207 8.31777 19.7478 7.47201 20.5936C6.62625 21.4394 5.39911 21.9015 4.37837 21.8746C3.35762 21.9015 2.13047 21.4394 1.28471 20.5936C0.438949 19.7478 -0.0231826 18.5207 0.00373815 17.4999C-0.0231826 16.4792 0.438949 15.2522 1.28471 14.4064C2.13047 13.5606 3.35762 13.0984 4.37837 13.1253C5.39911 13.0984 6.62625 13.5606 7.47201 14.4064C8.31777 15.2522 8.77991 16.4792 8.75299 17.4999Z\" fill=\"#27273F\"/><path d=\"M22.4691 15.887C22.0586 16.8853 22.0586 18.117 22.4691 19.113C23.7815 22.7249 27.5504 25.7781 31.3552 26.3098C32.874 26.5117 34.3771 27.8174 34.7899 29.2936C35.3081 30.7361 34.837 32.6699 33.7153 33.7153C32.6699 34.837 30.7383 35.3081 29.2936 34.7899C27.8174 34.3771 26.5117 32.874 26.3098 31.3552C25.7781 27.5504 22.7249 23.7815 19.113 22.4692C18.1147 22.0586 16.8831 22.0586 15.887 22.4692C12.2751 23.7815 9.22186 27.5504 8.69017 31.3552C8.48826 32.874 7.18261 34.3771 5.70645 34.7899C4.26395 35.3081 2.33013 34.837 1.28471 33.7153C0.163009 32.6699 -0.3081 30.7383 0.210125 29.2936C0.622911 27.8174 2.12599 26.5117 3.64477 26.3098C7.44957 25.7781 11.2185 22.7249 12.5309 19.113C12.9414 18.1147 12.9414 16.883 12.5309 15.887C11.2185 12.2751 7.44957 9.2218 3.64477 8.69012C2.12599 8.48821 0.622911 7.1826 0.210125 5.70645C-0.3081 4.26394 0.163009 2.33014 1.28471 1.28472C2.33013 0.16302 4.2617 -0.308102 5.70645 0.210123C7.18261 0.622908 8.48826 2.126 8.69017 3.64478C9.22186 7.44958 12.2751 11.2185 15.887 12.5308C16.8853 12.9414 18.1169 12.9414 19.113 12.5308C22.7249 11.2185 25.7781 7.44958 26.3098 3.64478C26.5117 2.126 27.8174 0.622908 29.2936 0.210123C30.7361 -0.308102 32.6699 0.16302 33.7153 1.28472C34.837 2.33014 35.3081 4.2617 34.7899 5.70645C34.3771 7.1826 32.874 8.48821 31.3552 8.69012C27.5504 9.2218 23.7815 12.2751 22.4691 15.887Z\" fill=\"#27273F\"/><path d=\"M21.8746 4.37836C21.9016 5.39911 21.4394 6.62626 20.5937 7.47202C19.7479 8.31778 18.5208 8.77991 17.5 8.75299C16.4793 8.77991 15.2521 8.31778 14.4064 7.47202C13.5606 6.62626 13.0985 5.39911 13.1254 4.37836C13.0985 3.35762 13.5606 2.13047 14.4064 1.28471C15.2521 0.438948 16.4793 -0.0231826 17.5 0.00373815C18.5208 -0.0231826 19.7479 0.438948 20.5937 1.28471C21.4394 2.13047 21.9016 3.35762 21.8746 4.37836Z\" fill=\"#27273F\"/><path d=\"M21.8746 30.6238C21.9016 31.6446 21.4394 32.8717 20.5937 33.7175C19.7479 34.5632 18.5208 35.0254 17.5 34.9984C16.4793 35.0254 15.2521 34.5632 14.4064 33.7175C13.5606 32.8717 13.0985 31.6446 13.1254 30.6238C13.0985 29.6031 13.5606 28.376 14.4064 27.5303C15.2521 26.6845 16.4793 26.2223 17.5 26.2492C18.5208 26.2223 19.7479 26.6845 20.5937 27.5303C21.4394 28.376 21.9016 29.6031 21.8746 30.6238Z\" fill=\"#27273F\"/><path d=\"M34.9963 17.4999C35.0232 18.5207 34.5611 19.7478 33.7153 20.5936C32.8695 21.4394 31.6424 21.9015 30.6217 21.8746C29.6009 21.9015 28.3738 21.4394 27.528 20.5936C26.6822 19.7478 26.2201 18.5207 26.247 17.4999C26.2201 16.4792 26.6822 15.2522 27.528 14.4064C28.3738 13.5606 29.6009 13.0984 30.6217 13.1253C31.6424 13.0984 32.8695 13.5606 33.7153 14.4064C34.5611 15.2522 35.0232 16.4792 34.9963 17.4999Z\" fill=\"#27273F\"/></svg></div>";
  html += "<div class='grid-container'>";
  html += "<div class='card'>";
  html += "<h2>Dispositivo</h2>";
  html += "<p>Firmware Version: <span id='firmwareVersion'></span></p>";
  html += "<p>Device ID: <span id='deviceID'></span></p>";
  html += "<p>SD Card: <span id='sdStatus'></span></p>";
 // html += "<p>File: /data24.csv</p>";
 // html += "<p>Size: 40.3 MB</p>";
  html += "<div class='button-container'><button onclick=\"location.href='/index.htm'\">Contenido SD</button></div>";
  html += "</div>";

  html += "<div class='card'>";
  html += "<h2>Conectividad</h2>";
  html += "<p>Date: <span id='fecha'></span></p>";
  html += "<p>Time: <span id='hora'></span></p>";
  html += "<p>Operator: <span id='operador'></span></p>";
  html += "<p>Signal: <span id='senial'></span></p>";
  html += "<p>Network: <span id='red'></span></p>";
  html += "<div class='button-container'><button onclick=\"location.href='/rtcupdate'\">RTC</button><button onclick=\"location.href='/sendData'\">Servidor</button></div>";
  html += "</div>";

  html += "<div class='card'>";
  html += "<h2>Sistema de Calefacción</h2>";
  html += "<p>DHT22 Temperature: <span id='dhtTemp'></span></p>";
  html += "<p>DHT22 Humidity: <span id='dhtHum'></span></p>";
  html += "<p>BMP280 Temperature: <span id='tempBMP'></span></p>";
  html += "<p>BMP280 Humidity: <span id='humBMP'></span></p>";
  html += "<p>Thermocouple Temperature: <span id='tempThermo'></span></p>";
  html += "<div id='extendedData' style='display:none;'></div>";
  html += "</div>";

  html += "<div class='card'>";
  html += "<h2>Sensor PM2.5</h2>";
  html += "<p>Temperature: <span id='tempe'></span></p>";
  html += "<p>Humidity: <span id='humi'></span></p>";
  html += "<p>PM1.0: <span id='PMS1'></span></p>";
  html += "<p>PM2.5: <span id='PMS2_5'></span></p>";
  html += "<p>PM10: <span id='PMS10'></span></p>";
  html += "<p>Formaldehyde: <span id='FMHDSB'></span></p>";
  html += "<div class='button-container'><button onclick=\"location.href='/descarga'\">Descargar datos SD</button></div>";
  html += "</div>";

  html += "</div></body></html>";
  server.send(200, "text/html", html);
}

void handleData() {
  String data = "{";
  data += "\"fecha\":\"" + fecha + "\",";
  data += "\"hora\":\"" + hora + "\",";
  data += "\"tempe\":" + String(tempe) + ",";
  data += "\"humi\":" + String(humi) + ",";
  data += "\"PMS1\":" + String(PMS1) + ",";
  data += "\"PMS2_5\":" + String(PMS2_5) + ",";
  data += "\"PMS10\":" + String(PMS10) + ",";
  data += "\"FMHDSB\":" + String(FMHDSB) + ",";
  data += "\"operador\":\"" + oper + "\",";
  data += "\"senial\":\"" + String(csq) + "\",";
  data += "\"red\":\"" + techString + "\",";
  data += "\"firmwareVersion\":\"" + ver + "\",";
  data += "\"deviceID\":\"" + ID + "\",";
  if (ext) {
    data += "\"dhtTemp\":" + String(dhtTemp) + ",";
    data += "\"dhtHum\":" + String(dhtHum) + ",";
    data += "\"tempBMP\":" + String(tempBMP) + ",";
    data += "\"humBMP\":" + String(humBMP) + ",";
    data += "\"tempThermo\":" + String(tempThermo) + ",";
    data += "\"ext\":true,";
  } else {
    data += "\"ext\":false,";
  }
  data += "\"hasSD\":" + String(hasSD ? "true" : "false");
  data += "}";
  server.send(200, "application/json", data);
}