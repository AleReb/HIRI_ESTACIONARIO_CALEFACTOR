# HIRI AUCA con calefactor para Plantower

Proyecto de firmware y hardware para una estación ambiental HIRI AUCA basada en
ESP32, un sensor Plantower y un calefactor controlado por ESP8266. El calefactor
mide temperatura y humedad, protege el elemento térmico mediante termocupla y
entrega sus datos a la estación por un enlace serial dedicado.

## Estado vigente

| Componente | Archivo principal | Plataforma | Versión |
|---|---|---|---|
| Calefactor | `Calefactor_V2_0_0/Calefactor_V2_0_0.ino` | ESP8266 / NodeMCU | Firmware `1.0.0` |
| Estación HIRI | `HIRI_STATIC_CALEFACTOR_0_1/HIRI_STATIC_CALEFACTOR_0_1.ino` | ESP32 Dev Module | `CAL V1.0.2` |
| PCB calefactor | `PCB/calefactor.sch` y `PCB/calefactor.brd` | Autodesk EAGLE 9.6.2 | Diseño fuente |
| PCB HIRI ESP32/SIM7600 | `PCB/pcbesp32sim7600/Board_760032v2.sch` y `PCB/pcbesp32sim7600/Board_760032v2.brd` | Autodesk EAGLE 9.6.2 | Diseño fuente |
| PCB resistivo | `PCB/RESIS-CALOR.brd` | Autodesk EAGLE 9.6.2 | Diseño fuente |

El firmware histórico con DHT22 se conserva sólo como referencia. El calefactor
vigente usa SHT40; no requiere DHT22.

## Manual técnico de consulta

- [Manual completo en Markdown](documentacion/manual_tecnico_hiri_calefactor_v2.md).
- [Manual completo en PDF](documentacion/manual_tecnico_hiri_calefactor_v2.pdf), con índice y diagramas.

El manual toma **HIRI_STATIC_CALEFACTOR_0_1 (CAL V1.0.2)** como base y explica
el módulo **Calefactor V2 (firmware 1.0.0)** conectado. Incluye componentes,
pines, función de cada archivo, arranque, sensores, pantalla, botón, consola USB,
protocolo serial, esquema HTTP y las 18 columnas del CSV.

| Consulta rápida | Comportamiento del código revisado |
|---|---|
| Guardado SD | Cada **3 minutos**; archivo diario con fecha/hora del RTC. |
| Servidor | Primer intento al entrar al loop; después cada **5 minutos**, con red disponible. Envía 16 valores por HTTP GET. |
| Botón B1 (GPIO39) | Intenta HTTP, guarda el mismo payload en SD y solicita **10 s** de calefacción. `CAL:CMD` confirma emisión, no encendido. |
| Pantalla | Rota cada **10 s** entre Plantower, calefactor y estado; añade GPS si GNSS está habilitado. B2 está deshabilitado. |
| Sin calefactor | Sigue midiendo/guardando/enviando; cinco campos del calefactor pasan a **-1** sin DATA válido. |
| Sin red | Tras tres fallos de conexión entra en modo SD, sin nuevos intentos automáticos; no reenvía el historial. |
| Control térmico | ESP8266 autónomo: humedad ≥65 % (libera ≤60 %), alguna referencia <15 °C o prueba; corte por termocupla inválida/≤0 °C o ≥70 °C, emergencia ≥80 °C. |
| Fecha/hora | CSV `AAAA-MM-DD HH:MM:SS`, archivo `DD_MM_AAAA`; sin RTC usa 2000-01-01. La URL no lleva timestamp. |

**Límites observados:** el reinicio HIRI configurado a 3 h y el apagado automático
OLED no tienen ejecución activa; los menús heredados no son accesibles por la
navegación normal. El calefactor sí reinicia cada 4 h. Las lecturas Plantower no
caducan automáticamente y sus campos T/RH requieren una variante compatible del
sensor. Al arrancar se fuerzan los períodos SD/HTTP y el autostart. El manual
detalla estas diferencias y las opciones que solo se guardan en configuración.

Se corrigieron comentarios contradictorios sin cambiar lógica ni valores
ejecutables. La [verificación documental](documentacion/verificacion_comentarios.json)
registra la comparación de tokens de los 15 archivos fuente revisados.
Para regenerar el PDF desde el Markdown, con ReportLab y fuentes DejaVu:

```bash
python3 documentacion/herramientas/generar_manual_pdf.py
```

## Funciones principales

### Calefactor

- SHT40 para temperatura y humedad.
- BME280 como medición interna/respaldo.
- MAX6675 y termocupla para protección térmica.
- Relé en `D0`.
- Control por humedad con histéresis: enciende desde `65 % RH` y libera a
  `60 % RH` o menos.
- Control por temperatura ambiental baja.
- Corte a `70 °C` y emergencia a `80 °C`.
- Prueba remota temporizada de 1 a 300 segundos, siempre subordinada a la
  protección térmica.
- Reinicio programado cada 4 horas.

### HIRI AUCA

- Plantower PMS por software serial.
- Enlace independiente a 9600 baudios con el calefactor.
- OLED SSD1306, RTC DS3231, tarjeta SD, batería y NeoPixel.
- SIM7600 para red celular, HTTP y GNSS configurable.
- Guardado SD cada 3 minutos y envío HTTP cada 5 minutos.
- Consulta opcional de versión del calefactor; admite ID `CALEF` o `CAL`
  con el protocolo de checksum actual, sin exigir respuesta de versión.

## Comunicación serial: puertos y funciones

El sistema utiliza puertos separados para la consola de mantenimiento, el
calefactor, el Plantower y el módem. Cada uno tiene su propio formato.

| Enlace | Puerto en el código | Velocidad | Función |
|---|---|---|---|
| PC ↔ HIRI | `Serial` del ESP32, mediante USB | 115200 baudios | Comandos de operación, configuración y diagnóstico. |
| HIRI ↔ calefactor | `Serial2` del ESP32 ↔ `externo` (`SoftwareSerial`) del ESP8266 | 9600 baudios, 8N1 | Identificación, versión, temperatura, prueba temporizada y telemetría. |
| Plantower → HIRI | `pms` (`SoftwareSerial`) | 9600 baudios | Recepción de tramas binarias del sensor. |
| HIRI ↔ SIM7600 | `SerialAT`, alias de `Serial1` | 115200 baudios, 8N1 | Comandos AT para red celular, HTTP, hora y GNSS. |
| PC ← calefactor | `Serial` del ESP8266, mediante USB | 115200 baudios | Lecturas y mensajes de diagnóstico del calefactor. |

### Consola USB de HIRI

Conectar el ESP32 al PC, seleccionar su puerto en el monitor serial y configurar
**115200 baudios** con terminación **nueva línea (`\n`)**. Enviar un comando
por línea, sin checksum. El firmware elimina espacios al principio y al final y
convierte el comando a minúsculas.

Ejemplo de consulta y prueba del calefactor; enviar cada línea por separado:

```text
help
sysinfo
config
cal 120
```

`cal 120` solicita una prueba de 120 segundos. También se admite `cal.120`.
La consola confirma el envío de la solicitud; esa confirmación no acredita que
el relé haya encendido. Las condiciones de aceptación se explican más abajo.

| Comando | Función |
|---|---|
| `help` o `?` | Muestra la ayuda; `cal` está implementado aunque no figura en esa lista. |
| `rtc` | Muestra fecha, hora, epoch y temperatura del RTC. |
| `rtcsync` | Intenta sincronizar el RTC con la hora del módem. |
| `modemtime` | Consulta la hora del módem sin ajustar el RTC. |
| `counters` | Muestra contadores de guardados SD y envíos HTTP exitosos. |
| `resetcnt` | Pone ambos contadores en cero y guarda el cambio en flash. |
| `stats` | Muestra estadísticas, batería, red, GPS y motivo de reinicio. |
| `sdinfo` | Muestra información de la tarjeta SD. |
| `sdlist` | Lista los archivos de la SD. |
| `sdnew` | Selecciona el archivo CSV generado para la fecha y escribe su cabecera; requiere SD disponible. |
| `sdclear` | Muestra la advertencia y el comando de confirmación del borrado. |
| `sdclear confirm` | Borra hasta 50 archivos no directorio de la raíz SD; es irreversible. |
| `netinfo` | Muestra operador, tecnología, registro y conexión de red/PDP. |
| `csq` | Consulta la calidad de señal al módem. |
| `sysinfo` | Muestra firmware, dispositivo, chip, uptime y valor configurado de reinicio; el loop no ejecuta ese reinicio periódico. |
| `mem` | Muestra memoria libre, mínima y mayor bloque disponible. |
| `reboot` | Reinicia el ESP32 tras una espera de 2 segundos. |
| `cal <segundos>` | Solicita al calefactor una prueba de entre 1 y 300 segundos. |
| `start` | Activa `streaming` si estaba detenido; en esa transición deja el registro SD desactivado. |
| `stop` | Detiene `streaming` y desactiva el registro SD si estaba transmitiendo. |
| `config` | Muestra toda la configuración. |
| `config sd`, `config http`, `config display`, `config power`, `config system` | Muestran una sección de la configuración. |
| `configsave` | Guarda la configuración actual en flash. |
| `configreset` | Restablece y guarda los valores predeterminados; requiere reinicio para aplicar todos los cambios. |

`start` y `stop` controlan el envío periódico y el registro según lo indicado;
la lectura de Plantower y la atención del enlace del calefactor continúan en el
bucle principal. `stop` no es una orden de apagado del relé.

### Configuración desde la consola

Los comandos `set` aceptados guardan automáticamente el cambio en flash.

| Comando | Valores admitidos y efecto |
|---|---|
| `set sdauto on/off` | Guarda opción; setup intenta montar SD siempre y fuerza `on` al arrancar. |
| `set sdsave <segundos>` | Periodo SD: `3`, `60`, `600` o `1200`. |
| `set httpsend <segundos>` | Periodo HTTP: `3`, `60`, `600` o `1200`. |
| `set httptimeout <segundos>` | Timeout HTTP de `5` a `30`. |
| `set oledoff on/off` | Guarda la opción; el apagado automático de OLED no se ejecuta en el flujo actual. |
| `set oledtime <segundos>` | Guarda timeout `60`, `120` o `180`; no hay apagado automático activo. |
| `set led on/off` | Aplica estado inmediato; el refresco PM puede volver a encender el NeoPixel. |
| `set ledbright <porcentaje>` | Brillo: `10`, `25`, `50` o `100`. |
| `set autostart on/off` | Guarda opción; el siguiente arranque fuerza `on`. |
| `set gnss on/off` | Activa o desactiva GNSS; requiere reinicio. |
| `set autowaitgps on/off` | Guarda opción; el arranque fuerza `off` y no espera GPS. |
| `set autogpsto <segundos>` | Guarda `60` a `900` s; no hay ruta de espera GPS activa. |
| `set rotatedisplay on/off` | `on` aplica `U8G2_R0`; `off`, `U8G2_R2`. |
| `set reboot <horas>` | Guarda `0` a `720` horas; el loop actual no ejecuta este reinicio programado. |
| `set gnssmode <modo>` | Acepta `1`, `3`, `5`, `7` o `15`; requiere reinicio. |

**Configuración impuesta al arrancar:** `enforceStationStartupConfig()` fuerza
montaje SD activado, guardado cada 180 segundos, envío HTTP cada 300 segundos,
inicio automático activado y espera GPS desactivada. Los cambios por consola
a esos parámetros se sobrescriben en el siguiente arranque. No se admiten
`set sdsave 180` ni `set httpsend 300`; reiniciar restaura esos períodos.
Los intervalos dependen del loop y pueden alargarse durante operaciones
bloqueantes de módem o consola.

### Cableado HIRI ↔ calefactor

Cruzar TX con RX y unir las masas de ambos equipos. Las señales UART son de
lógica de 3,3 V.

| ESP32 HIRI | ESP8266 / NodeMCU calefactor |
|---|---|
| TX2 GPIO23 | RX `D3` / GPIO0 |
| RX2 GPIO19 | TX `D4` / GPIO2 |
| GND | GND |

### Formato de las tramas

El enlace dedicado transmite texto ASCII a **9600 baudios, 8 bits, sin paridad
y 1 bit de parada (8N1)**. Cada trama ocupa una línea:

```text
ORIGEN,1,TIPO[,argumentos]*HH\r\n
```

- `ORIGEN`: `HIRI` para la estación y `CALEF` para el calefactor. HIRI también
  acepta el identificador histórico `CAL` en las respuestas.
- `1`: versión del protocolo, independiente de la versión del firmware.
- `TIPO`: `HELLO`, `VERSION`, `TEMP`, `CAL` o `DATA`.
- `HH`: suma de los bytes ASCII anteriores al asterisco, módulo 256,
  representada con dos dígitos hexadecimales. No incluye `*`, `HH` ni el fin
  de línea. No es un CRC ni un XOR.
- El emisor usa `println()` (`\r\n`). Los receptores procesan al recibir
  `\n` y eliminan o ignoran `\r`; aceptan checksum en mayúsculas o minúsculas.

Por ejemplo, la suma de `HIRI,1,HELLO` módulo 256 es `0x29`, por lo que la
trama completa es `HIRI,1,HELLO*29`, seguida del fin de línea.

### Mensajes y comportamiento

Los siguientes ejemplos incluyen checksums calculados para el texto mostrado.

| Mensaje | Dirección y función |
|---|---|
| `HIRI,1,HELLO*29` | HIRI → calefactor: solicita identificación cada 5 segundos. |
| `CALEF,1,HELLO,1.0.0*71` | Calefactor → HIRI: anuncia su firmware al arrancar y responde a `HELLO`. |
| `HIRI,1,VERSION*DB` | HIRI → calefactor: consulta la versión del firmware. |
| `CALEF,1,VERSION,1.0.0*23` | Calefactor → HIRI: responde la versión consultada. |
| `HIRI,1,TEMP,22.5*DE` | HIRI → calefactor: actualiza `tempsens`, la temperatura de referencia externa. |
| `HIRI,1,CAL,120*44` | HIRI → calefactor: solicita una prueba de calefacción de 120 segundos. |
| `CALEF,1,DATA,...*HH` | Calefactor → HIRI: transmite las lecturas y el estado de control aproximadamente cada segundo. Aquí `...` y `HH` son marcadores del formato. |

La consulta `VERSION` se envía al arrancar y se reintenta cada 30 segundos
durante un máximo de 3 minutos. Al conocer la versión mediante `HELLO` o
`VERSION`, se detienen los reintentos. La ausencia de respuesta no bloquea el
arranque, la adquisición ni el guardado; HIRI acepta también `HELLO` sin versión.

`TEMP` se envía cada 2 segundos cuando `heaterShtTemp` no es `NaN`. En el
firmware actual ese valor procede del SHT40 recibido en `DATA`: HIRI devuelve
esa lectura al calefactor como referencia externa, no la temperatura del
Plantower. Antes de recibir una actualización, `tempsens` vale `16.0 °C`.
El calefactor conserva el último valor recibido; no implementa caducidad de
esa referencia por pérdida de comunicación.

### Campos de telemetría `DATA`

La trama contiene 13 campos en total: identificador, versión de protocolo,
tipo y diez valores. Ejemplo completo en una sola línea:

```text
CALEF,1,DATA,22.5,22.5,23.0,55.0,54.0,40.0,0,0,0,0*18
```

| Posición después de `DATA` | Campo en el código | Significado |
|---|---|---|
| 1 | `tempsens` | Temperatura de referencia recibida de HIRI, en °C. |
| 2 | `shtTemp` | Temperatura SHT40, en °C. |
| 3 | `tempBME` | Temperatura BME280, en °C. |
| 4 | `shtHum` | Humedad relativa SHT40, en %. |
| 5 | `humBME` | Humedad relativa BME280, en %. |
| 6 | `tempThermo` | Temperatura de la termocupla MAX6675, en °C. |
| 7 | `relayOn` | Estado del relé: `1` encendido, `0` apagado. |
| 8 | `heaterByHumidity` | Demanda por humedad: `1` activa, `0` inactiva. |
| 9 | `heaterByTemperature` | Demanda por baja temperatura: `1` activa, `0` inactiva. |
| 10 | `emergencyMode` | Emergencia térmica: `1` activa, `0` inactiva. |

Las lecturas se transmiten con un decimal; una lectura no disponible puede
aparecer como `nan`. La prueba manual no tiene un campo propio: durante ella
el relé puede estar encendido con ambas banderas de demanda automática en cero.

### Prueba temporizada y protecciones

La duración de `CAL` debe ser un entero entre 1 y 300 segundos. El calefactor
rechaza la solicitud si la termocupla no tiene una lectura finita mayor que
0 °C, si está a 70 °C o más, o si existe emergencia activa. Una prueba aceptada
se cancela al vencer el tiempo, al perder una lectura válida de termocupla o
al alcanzar el corte de 70 °C. Una temperatura de 80 °C o más activa emergencia.

El control sigue ejecutándose en el calefactor. Al terminar la prueba, el relé
puede seguir encendido si existe demanda automática por humedad o temperatura.
El protocolo no define una respuesta `ACK`/`NACK` para `TEMP` o `CAL`, ni una
orden específica para cancelar la prueba. Los motivos de rechazo se imprimen
en la consola USB del ESP8266 a 115200 baudios.

### Diagnóstico del enlace

HIRI imprime `[CALEFACTOR][TX]`, `[CALEFACTOR][RX RAW]` y
`[CALEFACTOR][RX]` en su consola USB. Una trama con formato de checksum
incorrecto o suma no coincidente se descarta. Si han transcurrido más de
5 segundos desde el último `DATA` recibido, HIRI marca el calefactor como
desconectado. `HELLO` y `VERSION` pueden marcar conexión, pero no actualizan
la fecha del último `DATA`.

Si no llegan datos, revisar primero velocidad, cruce TX/RX y GND común.
Si aparecen errores de checksum, comprobar que se está usando el enlace de
9600 baudios con tramas completas y fin de línea. La salida USB del ESP8266
incluye mensajes de texto y una copia de `DATA` sin checksum; no tiene el mismo
formato que el enlace dedicado.

### Funciones del código que implementan la comunicación

| Archivo | Funciones y responsabilidad |
|---|---|
| [`heater_serial.ino`](HIRI_STATIC_CALEFACTOR_0_1/heater_serial.ino) | `heaterChecksum()`, `heaterFrameIsValid()` y `heaterSendFrame()`: cálculo, validación y envío de tramas. |
| [`heater_serial.ino`](HIRI_STATIC_CALEFACTOR_0_1/heater_serial.ino) | `heaterReadAvailableFrames()` reúne caracteres hasta `\n`; `heaterProcessFrame()` distingue los mensajes; `heaterParseData()` actualiza lecturas, banderas y tiempo del último dato. |
| [`heater_serial.ino`](HIRI_STATIC_CALEFACTOR_0_1/heater_serial.ino) | `pollHeaterSerial()` atiende recepción, consultas periódicas, envío de temperatura y timeout; `heaterRequestFirmwareVersion()` consulta versión; `heaterRequestTimedTest()` valida la duración y envía `CAL`. |
| [`serial_commands.ino`](HIRI_STATIC_CALEFACTOR_0_1/serial_commands.ino) | `processSerialCommand()` interpreta los comandos de la consola USB de HIRI. |
| [`Calefactor_V2_0_0.ino`](Calefactor_V2_0_0/Calefactor_V2_0_0.ino) | `frameChecksum()`, `decodeFrame()` y `sendFrame()` implementan el formato en el ESP8266; `readExternalTemperature()` procesa todos los mensajes entrantes, pese a su nombre. |
| [`Calefactor_V2_0_0.ino`](Calefactor_V2_0_0/Calefactor_V2_0_0.ino) | `startTimedManualHeating()` valida la prueba; `printValues()` emite telemetría; `updateControl()` aplica demandas y protecciones al relé. |

## Manual de consulta y ajuste de hora

La estación conserva fecha y hora en el **RTC DS3231**. El ajuste disponible
por consola copia la hora del módem SIM7600 al RTC. Conectar el ESP32 por USB,
abrir el monitor serial a **115200 baudios** y seleccionar **nueva línea** como
terminación. Enviar los comandos de este apartado uno por uno y esperar su
respuesta antes del siguiente.

### 1. Consultar la hora guardada

Enviar:

```text
rtc
```

La respuesta muestra fecha y hora en formato `AAAA-MM-DD HH:MM:SS`, epoch y
temperatura del RTC. Comprobar la fecha completa además de la hora. Este
comando solamente consulta; no modifica el reloj.

Si aparece `[RTC] Not available`, el firmware no detectó el DS3231. Revisar
alimentación y conexiones I2C antes de intentar sincronizar. Reiniciar después
de corregir la conexión para que el arranque vuelva a detectar el RTC.

### 2. Comprobar la hora de referencia del módem

Enviar:

```text
netinfo
modemtime
```

`netinfo` permite revisar el registro y la conexión celular. `modemtime`
consulta la hora del SIM7600 mediante `AT+CCLK?`, sin modificar el DS3231.
Comparar la fecha y hora mostradas con una referencia de hora conocida antes
de copiarlas al RTC. Que el módem responda no garantiza que su reloj esté
actualizado por la red.

Si aparece `[MODEM] ✗ Failed to get time`, esperar a que el módem esté operativo
y registrado en la red y repetir `modemtime`. El ajuste requiere que el módem
entregue una fecha que el firmware considere válida.

### 3. Sincronizar el RTC

Una vez comprobada la referencia del módem, enviar:

```text
rtcsync
```

El firmware solicita al módem la actualización automática de zona horaria
(`AT+CTZU=1` y `AT+CTZR=1`), consulta su reloj y calcula la diferencia con el
RTC. **El ajuste se realiza solamente si la diferencia absoluta supera
30 segundos**. Una diferencia de exactamente 30 segundos tampoco provoca ajuste.

| Mensaje | Interpretación y acción |
|---|---|
| `[RTC] Synchronized to modem clock` y `[RTC] ✓ Synced from modem` | El DS3231 se ajustó con la hora obtenida del módem. Continuar con la verificación. |
| `[RTC] Within threshold; no sync needed` | La diferencia es de 30 segundos o menos; se conserva la hora del RTC. La consola también imprime `✗ Sync failed` porque la función no hizo un ajuste, aunque en este caso no existe un fallo de consulta. |
| `[RTC] Modem time not ready; will retry in loop` | No se obtuvo una hora válida. En la versión actual hay que repetir `modemtime` y `rtcsync` manualmente: el bucle principal no ejecuta ese reintento anunciado. |
| `[RTC] Not available` | El RTC no fue detectado; resolver su conexión antes de continuar. |

El ajuste escribe directamente en el RTC; no requiere `configsave` ni un
reinicio para aplicarse.

### 4. Verificar el resultado

Enviar nuevamente:

```text
rtc
modemtime
```

Comprobar que ambas fechas coincidan y que la diferencia sea coherente con
los segundos transcurridos entre consultas. Repetir `rtc` unos segundos después
para confirmar que el reloj avanza. Los nuevos registros SD usarán la fecha y
hora del RTC; el ajuste no corrige los registros ya guardados.

### Zona horaria y cambios de horario

El firmware actual extrae año, mes, día, hora, minuto y segundo de la respuesta
del módem, pero **no aplica el sufijo de zona horaria de `AT+CCLK?`**; convierte
esos campos mediante `mktime()`. Por ello, la salida de `rtc` y `modemtime` no
debe interpretarse como UTC o como hora local de Chile sin comprobarla con una
referencia externa. No hay un comando de consola para seleccionar zona horaria,
aplicar un desplazamiento ni gestionar automáticamente el horario de verano.

Si se observa una diferencia de horas enteras, comprobar primero la referencia
del módem y la interpretación de zona horaria. Repetir `rtcsync` no resuelve
por sí solo ese problema.

### Alcance del ajuste en el firmware actual

- No existe un comando serial para escribir una fecha y hora arbitrarias, como
  `rtc set ...`. El procedimiento disponible es la sincronización con el módem.
- El arranque vigente detecta el RTC mediante `rtc.begin()`, pero no llama a
  `syncRtcSmart()`. No se debe asumir que sincroniza al encender ni que repone
  automáticamente la hora de compilación si el RTC pierde su respaldo.
- `rtc.ino` contiene una rutina de sincronización inteligente con límite de
  tres ajustes y una función `resetModemSyncCounter()`, pero no hay un comando
  serial que invoque ese reinicio de contador. `resetcnt` solamente reinicia
  los contadores SD/HTTP.
- El comando manual `rtcsync` no comprueba el límite de tres ajustes antes de
  intentar sincronizar. El contador mostrado no impide usar este procedimiento.
- Para conservar la hora al cortar la alimentación principal, comprobar el
  respaldo del módulo RTC. Después de una pérdida de hora, repetir la consulta,
  sincronización y verificación de este manual.

## Estructura

- `Calefactor_V2_0_0/`: firmware vigente del calefactor.
- `HIRI_STATIC_CALEFACTOR_0_1/`: firmware vigente de la estación.
- `PCB/`: esquemáticos y placas EAGLE del calefactor, del elemento resistivo y
  de la estación HIRI con ESP32/SIM7600; consultar su README antes de fabricar.
- `documentacion/`: manual vigente de HIRI y calefactor en Markdown/PDF, cambios y antecedentes.
- `codigo_original/`: respaldos históricos; no son el firmware recomendado.

## Compilación

```bash
arduino-cli compile --fqbn esp8266:esp8266:nodemcuv2 Calefactor_V2_0_0
arduino-cli compile --fqbn esp32:esp32:esp32 HIRI_STATIC_CALEFACTOR_0_1
```

Se requieren las librerías Arduino indicadas por los `#include` de cada
sketch. Estos comandos compilan; para cargar se debe seleccionar el puerto
real y ejecutar la operación de subida correspondiente.

## Documentación

- [Índice documental de HIRI y calefactor](documentacion/README.md)
- [Documentación de HIRI](HIRI_STATIC_CALEFACTOR_0_1/README.md)
- [PCB y advertencias de fabricación](PCB/README.md)
- [Licencia y atribución](LICENSE.md)
- [Descargo de responsabilidad](DISCLAIMER.md)

## Comentario final y propuestas de mejora

La revisión deja documentado el comportamiento de HIRI y del calefactor V2,
con sus diferencias entre configuración declarada y ejecución efectiva. El
siguiente paso propuesto es validar en banco los tiempos, las desconexiones,
la escritura SD y las protecciones antes de evaluar cambios en terreno.
Las mejoras de este apartado son **propuestas pendientes de implementar y
validar**; no describen prestaciones del firmware actual.

### Frecuencias de lectura, guardado y envío

Mantener separadas la frecuencia de adquisición, la de guardado y la de envío.
Actualmente el calefactor lee/controla cada 1 s, HIRI guarda en SD cada 180 s
y programa HTTP cada 300 s; Plantower se recibe según llegan sus tramas.
Se propone permitir períodos configurables que persistan tras reiniciar,
incluidos los valores actuales de 180 y 300 s, con consulta de los valores
realmente aplicados.

| Perfil propuesto | Guardado SD | Envío HTTP | Uso a evaluar |
|---|---|---|---|
| Estación actual | 180 s | 300 s | Referencia para comparar con los registros existentes. |
| Mayor detalle local | 60 s | 300 s | Conservar más resolución en SD sin aumentar los envíos periódicos. |
| Seguimiento más frecuente | 60 s | 60 s | Evaluar respuesta, consumo, cobertura y duración de las operaciones HTTP. |

Estos perfiles no cambiarían por sí solos el control local de 1 s del
calefactor. Antes de adoptarlos se propone medir el tiempo ocupado por el módem,
el volumen de datos y el crecimiento de archivos. También definir si cada
registro corresponde a una lectura instantánea o a un promedio de una ventana;
si se agregan muestras, registrar duración de ventana y cantidad de lecturas
válidas. El firmware actual guarda las últimas lecturas disponibles.

### Fecha legible, timestamp Unix y UTC

Se propone conservar dos representaciones del mismo instante de medición:
`timestamp_unix_s`, entero de segundos desde 1970-01-01 00:00:00 UTC, y
`timestamp_utc`, fecha legible en formato `AAAA-MM-DDTHH:MM:SSZ`. La `Z` indicaría
UTC. El timestamp Unix no debe incluir un desplazamiento horario local, y su
nombre debe indicar segundos para evitar confundirlo con milisegundos.

Ejemplo del formato propuesto, calculado para el mismo instante en ambos campos:

```csv
timestamp_unix_s,timestamp_utc,time_valid,time_source
1794916800,2026-11-17T12:00:00Z,1,rtc
```

Para ello se propone mantener el RTC en UTC, interpretar correctamente el
desplazamiento de la hora entregada por el módem y verificar la sincronización
antes de marcar la hora como válida. `time_source` identificaría la referencia
utilizada; que el RTC responda no garantiza que su hora sea correcta. Cuando no
haya una hora válida, registrar `time_valid=0`, campos temporales vacíos y un
contador `uptime_ms` separado, evitando presentar el respaldo del año 2000 como
fecha real. `millis()` no es un timestamp Unix.

Para consulta humana se podría mostrar además la hora local de
`America/Santiago`, aplicando la zona horaria y sus cambios estacionales al
visualizar. La conversión local no modificaría los timestamps UTC almacenados.
Se propone definir también si los archivos diarios rotan por día UTC o local,
y dejar esa decisión explícita en el formato.

La medición conservaría su timestamp al guardarse en SD, enviarse o reenviarse.
La hora de recepción del servidor sería un campo diferente. Agregar estos campos
requiere acordar una versión de CSV/API y su compatibilidad con el servidor:
**el CSV actual tiene 18 columnas y el GET actual no envía timestamp**.

### Otras mejoras propuestas

- Incorporar validez y antigüedad por sensor para detectar lecturas congeladas
  y distinguir datos ausentes de valores reales, incluido el marcador `-1`.
- Implementar y validar las opciones hoy sin ejecución: reinicio programado de
  HIRI, apagado OLED y habilitación persistente del NeoPixel.
- Reducir bloqueos de red y añadir recuperación de SD; evaluar una cola de
  reenvío con identificador de muestra y control de duplicados en el servidor.
- Añadir confirmación de aceptación/rechazo de `CAL` y un indicador explícito
  de prueba manual, conservando las protecciones térmicas locales.

## Licencias

| Componente | Licencia | Alcance |
|---|---|---|
| Hardware | **CERN-OHL-S-2.0** | Diseños propios y documentación de hardware en `PCB/`. |
| Software | **GPL-3.0-only** | Software propio, herramientas y documentación general del proyecto. |
| Firmware | **GPL-3.0-only** | Firmware propio HIRI, calefactor V2 y código propio derivado. |

Consulta [LICENSE](LICENSE) para el alcance por componente y los textos completos
de [CERN-OHL-S-2.0](LICENSES/CERN-OHL-S-2.0.txt) y
[GPL-3.0-only](LICENSES/GPL-3.0-only.txt). GPL se aplica **solo en su versión 3**,
sin la opción «o cualquier versión posterior».

Los componentes de terceros conservan sus licencias originales. La biblioteca
EAGLE `diy-modules`, incorporada en los diseños del calefactor, incluye un aviso
**CC BY 4.0** de Miroslaw Brudnowski que se conserva. Las bibliotecas, los cores
ESP32/ESP8266 y sus artefactos compilados conservan sus respectivos términos.

Fuentes de los textos: [Open Hardware Repository / CERN](https://ohwr.org/licences/)
y [GNU / Free Software Foundation](https://www.gnu.org/licenses/gpl-3.0.html).

El hardware, el software y el firmware se proporcionan sin garantías, conforme
a las licencias respectivas. Consultar el [descargo de responsabilidad](DISCLAIMER.md).

## Autor

**Alejandro Rebolledo** — [arebolledo@udd.cl](mailto:arebolledo@udd.cl).
