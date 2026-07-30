# Manual técnico — HIRI AUCA con calefactor

Referencia comprobada contra el código vigente el 30 de julio de 2026.

- Sketch principal: `HIRI_STATIC_CALEFACTOR_0_1.ino`
- Versión: `CAL V0.0.1`
- Equipo: `HIRI-AUCA-4`
- Plataforma: ESP32 Dev Module

## 1. Arquitectura

| Archivo | Responsabilidad vigente |
|---|---|
| `HIRI_STATIC_CALEFACTOR_0_1.ino` | Estados globales, `setup`, `loop`, arranque, telemetría y scheduler |
| `heater_serial.ino` | Protocolo serial con el calefactor |
| `PMSandSensorEXTRA.ino` | Lectura Plantower y LED de PM2.5 |
| `sd_card.ino` | CSV, errores y transmisiones fallidas |
| `http.ino` | Sesión HTTP mediante SIM7600 |
| `gps.ino` | GNSS, NMEA, diagnóstico y XTRA |
| `rtc.ino` | RTC y sincronización |
| `wifi.ino` | AP, DNS cautivo y gestor SD |
| `ui.ino` | OLED y botones |
| `config.h`, `config.ino` | Pines, preferencias y defaults |
| `serial_commands.ino` | Consola serial USB |
| `helpers.ino` | Valores seguros, reset y logging |
| `animacion.ino` | Animación inicial |

## 2. Pines

```text
SIM7600:    TX27 RX26 PWRKEY4 DTR32 FLIGHT25
Plantower:  RX18 TX5
Calefactor: RX19 TX23 (Serial2, 9600 8N1)
SD HSPI:    SCLK14 MISO2 MOSI15 CS13
Batería:    ADC35
Power:      GPIO33
NeoPixel:   GPIO12
BTN1:       GPIO39, activo LOW, requiere resistencia externa
BTN2:       -1, deshabilitado
```

OLED SSD1306 y RTC DS3231 comparten I2C. En esta variante no existe en el código
un `I2C_POWER_PIN` ni rutinas de inicialización para SHT/ENS/gas locales; esas
referencias pertenecían a variantes anteriores.

## 3. Secuencia de arranque

1. Inicia Serial USB y watchdog.
2. Configura I2C a 50 kHz.
3. Carga preferencias y fuerza la política operativa de estación.
4. Monta SD y crea el CSV diario.
5. Detecta OLED y ejecuta la animación.
6. Inicia Plantower y `Serial2`.
7. Envía una solicitud opcional de versión al calefactor.
8. Detecta RTC.
9. Muestra por 3 segundos la revisión OLED mientras continúa leyendo el enlace
   del calefactor.
10. Inicializa botones, SIM7600, red, XTRA y GNSS.
11. Activa autostart, logging SD y envío HTTP.

La ventana de arranque muestra `OLED`, `SD`, `RTC`, `PLANTOWER` y `CALEFACTOR`.
El estado del calefactor confirma la UART; no exige soporte de `VERSION`.

## 4. Protocolo del calefactor

Tramas ASCII con checksum aditivo de 8 bits:

```text
HIRI,1,HELLO*HH
HIRI,1,VERSION*HH
HIRI,1,TEMP,<valor>*HH
HIRI,1,CAL,<1-300>*HH

CALEF,1,HELLO,<version>*HH
CALEF,1,VERSION,<version>*HH
CALEF,1,DATA,<10 valores>*HH
```

HIRI acepta tanto `CALEF` como el ID histórico `CAL`. También acepta el
`HELLO` histórico sin campo de versión (`CAL,1,HELLO`).

`DATA` contiene, en orden: temperatura externa recibida, temperatura SHT40,
temperatura BME280, humedad SHT40, humedad BME280, termocupla, relé, activación
por humedad, activación por temperatura y emergencia.

La conexión se considera sin datos si pasan más de 5 segundos desde el último
`DATA`. HIRI envía temperatura al calefactor cada 2 segundos y `HELLO` cada 5
segundos.

`VERSION` es opcional y no bloqueante. Se consulta al inicio y cada 30 segundos
por hasta 3 minutos. Una respuesta recibida por `HELLO` o `VERSION` detiene los
reintentos.

## 5. Bucle principal

El `loop()`:

1. alimenta watchdog y procesa botones;
2. atiende WiFi/DNS/Web si ese modo está activo;
3. procesa GNSS;
4. actualiza sensores auxiliares y señal;
5. lee Plantower;
6. atiende el serial del calefactor;
7. procesa comandos AT;
8. muestrea batería;
9. actualiza OLED;
10. guarda SD según periodo;
11. transmite HTTP según periodo;
12. procesa comandos USB y reinicio programado.

## 6. Configuración persistente

Preferencias `config`:

```text
sdAuto, sdSavePer, httpPer, httpTO, oledOff, oledTO,
ledEn, ledBr, autoStart, autoGPS, autoGPSTO,
rotDisp, rebootH, gnssEn, gnssMode
```

En cada boot se fuerzan `sdAutoMount=true`, guardado a 180 s, HTTP a 300 s,
autostart activo y espera GPS desactivada. El reinicio programado por defecto es
cada 3 horas.

## 7. HTTP

Endpoint:

```text
http://api-sensores.cmasccp.cl/insertarMedicion
```

El payload contiene 16 valores:

```text
lat,lon,csq,velocidad,satelites,voltaje,
tempPMS,humPMS,pm1,pm25,pm10,
tempSHTcalefactor,humSHTcalefactor,rele,
tempBMEcalefactor,humBMEcalefactor
```

Los datos inválidos se convierten a `-1`. Para el ID 4 se usan los sensores
`1165`, `1166`, `1167` y `1203` según el mapeo de `getIdsSensores()`.

## 8. SD

Archivo diario:

```text
/HIRI_AUCA_4_DD_MM_AAAA.csv
```

Las 18 columnas registran fecha/hora, PM, T/RH Plantower, T/RH BME del
calefactor, T/RH SHT40 del calefactor, relé, CSQ, resultado HTTP, posición,
velocidad, satélites y voltaje.

Logs:

- `/errors_h4.csv`
- `/failed_h4.csv`

## 9. UI real

`renderDisplay()` muestra directamente `drawStationView()` y rota entre:

- Plantower;
- calefactor;
- sistema;
- GPS/hora, sólo cuando GNSS está habilitado.

BTN1 ejecuta envío HTTP manual, guardado del mismo payload y una prueba del
calefactor por 10 segundos. BTN2 avanzaría la vista, pero está deshabilitado por
`BUTTON_PIN_2=-1`.

Los arreglos de menú permanecen en el fuente, pero el flujo vigente de botones
y render retorna antes de utilizarlos. No se deben documentar como interfaz
operativa hasta reactivar esa navegación.

## 10. Comandos seriales relevantes

Baudrate USB: 115200.

```text
help, cal <1-300>
rtc, rtcsync, modemtime
counters, resetcnt, stats
sdinfo, sdlist, sdnew, sdclear
netinfo, csq, sysinfo, mem, reboot
start, stop, config, configsave, configreset
set sdauto ..., set sdsave ..., set httpsend ..., set httptimeout ...
set oledoff ..., set oledtime ..., set led ..., set ledbright ...
set autostart ..., set gnss ..., set autowaitgps ...
set autogpsto ..., set rotatedisplay ..., set reboot ..., set gnssmode ...
```

La lista exacta y validaciones están en `serial_commands.ino`; `help` imprime la
ayuda disponible en el firmware cargado.

## 11. Compilación y QA

```powershell
arduino-cli compile --fqbn esp32:esp32:esp32 HIRI_STATIC_CALEFACTOR_0_1
```

Validar en hardware: boot, OLED, RTC, SD, Plantower, calefactor nuevo y antiguo,
termocupla, relé, SIM7600, HTTP, CSV y reinicio programado.

## 12. Seguridad, autoría y licencia

Creación original: Alejandro Rebolledo (`arebolledo@udd.cl`).

Licencia: [CC BY-NC 4.0](../../LICENSE.md).

Descargo: [DISCLAIMER.md](../../DISCLAIMER.md).

La protección de software a 70/80 °C no reemplaza fusibles, límites térmicos
independientes, revisión eléctrica ni validación del integrador.
