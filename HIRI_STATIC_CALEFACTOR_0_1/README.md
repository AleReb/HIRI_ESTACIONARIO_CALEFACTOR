# HIRI AUCA 4 con calefactor

Firmware para una estación ambiental estática basada en ESP32. Integra un
Plantower, un calefactor externo, OLED, RTC, SD, batería, SIM7600 y GNSS
configurable.

## Estado vigente

- Sketch: `HIRI_STATIC_CALEFACTOR_0_1.ino`
- Plataforma: `esp32:esp32:esp32`
- Versión mostrada por el firmware: `CAL V1.0.1`
- Dispositivo activo: `HIRI-AUCA-4`
- `DEVICE_ID_STR`: `4`
- SSID local: `HIRI-AUCA-4`
- APN: `flolive.net`
- API: `http://api-sensores.cmasccp.cl/insertarMedicion`

## Hardware y pines

| Función | Pines ESP32 |
|---|---|
| SIM7600 | TX 27, RX 26, PWRKEY 4, DTR 32, FLIGHT 25 |
| Plantower PMS | RX 18, TX 5 por `SoftwareSerial` |
| Calefactor | RX2 19, TX2 23 a 9600 baudios |
| SD HSPI | SCLK 14, MISO 2, MOSI 15, CS 13 |
| Batería | ADC 35 |
| Alimentación auxiliar | GPIO 33 |
| NeoPixel | GPIO 12 |
| BTN1 | GPIO 39, activo en LOW y con resistencia externa |
| BTN2 | Deshabilitado (`-1`) |
| Enable/alimentación I2C | GPIO 0 (`I2C_POWER_PIN`) |
| OLED/RTC | Bus I2C: SDA 21 y SCL 22, valores predeterminados del ESP32 usados por `Wire.begin()` |

### Comportamiento actual de GPIO0 e I2C

El firmware vigente configura GPIO0 como salida y lo coloca directamente en
`HIGH` al comienzo de `setup()`. Después espera aproximadamente 600 ms antes de
ejecutar `Wire.begin()`. El bus se configura con timeout de 50 ms y reloj de
50 kHz.

La secuencia implementada actualmente es:

```text
GPIO0 OUTPUT
GPIO0 HIGH
espera 300 ms
inicialización Serial/watchdog
espera 300 ms
Wire.begin()
```

Esta variante **no ejecuta** el power-cycle `LOW -> HIGH` presente en el
firmware estático anterior. Tampoco contiene la función de recuperación
`recoverI2CBus()` ni el comando serial `i2c reset`.

Los comentarios de `config.h` describen GPIO0 como control activo en HIGH:
`LOW` corta la alimentación y `HIGH` energiza el bus. Esa es la intención del
firmware; durante diagnóstico de hardware se debe confirmar la polaridad real
midiendo tanto GPIO0 como la alimentación conmutada de OLED/RTC.

## Funciones activas

- Plantower: PM1.0, PM2.5, PM10 y T/RH cuando el modelo los entrega.
- Calefactor: SHT40, BME280, termocupla, relé y banderas de control.
- RTC DS3231.
- SD con CSV diario y archivos de error/transmisiones fallidas.
- HTTP por SIM7600.
- GNSS opcional; apagado por defecto.
- OLED con vistas rotativas de Plantower, calefactor y sistema; agrega GPS
  cuando GNSS está habilitado.
- Reinicio programado cada 3 horas.

## Detección y versión del calefactor

HIRI acepta el identificador vigente `CALEF` y el histórico `CAL`. Al arrancar
envía una consulta `VERSION` no bloqueante. Si no hay respuesta, reintenta cada
30 segundos durante un máximo de 3 minutos y luego deja de consultar.

La falta de versión no impide usar un calefactor antiguo. HIRI acepta tanto
`CAL,1,HELLO` sin versión como `CAL/CALEF,1,HELLO,<versión>`; `HELLO` o `DATA`
siguen estableciendo la conexión. Cuando se recibe una versión válida, se
muestra como `CALEFACTOR FW <versión>` en la OLED.

## Revisión de arranque

La ventana de inicio contiene:

```text
OLED
SD
RTC
PLANTOWER
CALEFACTOR
```

En `CALEFACTOR`, el estado inicial confirma que la UART quedó configurada. Si
la versión llega durante la ventana de revisión, se muestra en esa fila. La
ausencia de `VERSION` no se informa como fallo.

## Configuración operativa

En cada arranque el firmware fuerza:

```text
SD auto-mount:      ON
Guardado SD:        180000 ms (3 min)
Envío HTTP:         300000 ms (5 min)
Autostart:          ON
Esperar GPS:        OFF
```

Defaults al restablecer configuración:

```text
Timeout HTTP:       15 s
OLED auto-off:      OFF
NeoPixel:           ON, brillo 50 %
GNSS:               OFF
Modo GNSS:          15
Reinicio:           3 h
```

## Payload HTTP

Se envían exactamente 16 valores:

| Posición | Valor |
|---:|---|
| 1 | Latitud |
| 2 | Longitud |
| 3 | CSQ |
| 4 | Velocidad GNSS |
| 5 | Satélites |
| 6 | Voltaje |
| 7 | Temperatura Plantower |
| 8 | Humedad Plantower |
| 9 | PM1.0 |
| 10 | PM2.5 |
| 11 | PM10 |
| 12 | Temperatura SHT40 del calefactor |
| 13 | Humedad SHT40 del calefactor |
| 14 | Estado del relé |
| 15 | Temperatura BME280 del calefactor |
| 16 | Humedad BME280 del calefactor |

Los datos no disponibles se representan con `-1`.

Para `DEVICE_ID_STR = "4"`:

```text
idsSensores=1165,1165,1165,1165,1165,1166,1167,1167,1167,1167,1167,1203,1203,1203,1203,1203
idsVariables=11,12,15,45,46,4,3,6,7,8,9,3,6,57,58,59
```

## Tarjeta SD

Archivo diario:

```text
/HIRI_AUCA_4_DD_MM_AAAA.csv
```

Columnas:

```text
timestamp,pm_1_0_ug_m3,pm_2_5_ug_m3,pm_10_ug_m3,
temp_plantower_c,hum_plantower_pct,
temp_interna_calefactor_bme_c,hum_interna_calefactor_bme_pct,
temp_externa_calefactor_sht40_c,hum_externa_calefactor_sht40_pct,
relay_calefactor,senal_csq,conexion_http,
latitud,longitud,velocidad_kmh,satelites,voltaje_v
```

Archivos auxiliares:

- `/errors_h4.csv`
- `/failed_h4.csv`

## Interfaz actual

La pantalla normal rota automáticamente entre vistas de estación. BTN1 ejecuta
una prueba manual compuesta por envío HTTP, guardado SD y solicitud de
calefacción por 10 segundos. BTN2 avanzaría la pantalla, pero está deshabilitado
en la configuración de pines actual.

Las estructuras de menú todavía existen en `ui.ino`, pero el flujo de render y
botones vigente retorna desde la vista de estación antes de navegar esos menús.
Consulte `documentacion/menu_structure.md`.

## Compilación

Desde la carpeta superior:

```powershell
arduino-cli compile --fqbn esp32:esp32:esp32 HIRI_STATIC_CALEFACTOR_0_1
```

## Documentación, licencia y seguridad

- [Manual técnico](documentacion/MANUAL_TECNICO.md)
- [Manual de usuario](documentacion/MANUAL_USUARIO.md)
- [Interfaz actual](documentacion/menu_structure.md)
- [Licencia del proyecto](../LICENSE.md)
- [Descargo de responsabilidad](../DISCLAIMER.md)

Creación original: **Alejandro Rebolledo** (`arebolledo@udd.cl`).

Licencia: **CC BY-NC 4.0**.
