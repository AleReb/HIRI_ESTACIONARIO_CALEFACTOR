# HIRI AUCA con calefactor · estación 2

Firmware para una estación ambiental estática basada en ESP32. Integra un
Plantower, un calefactor externo, OLED, RTC, SD, batería, SIM7600 y GNSS
configurable.

## Estado vigente

- Sketch: `HIRI_STATIC_CALEFACTOR_0_1.ino`
- Plataforma: `esp32:esp32:esp32`
- Versión mostrada por el firmware: `CAL V1.0.2`
- Dispositivo activo: `HIRI-AUCA-2`
- `DEVICE_ID_STR`: `2`
- SSID local: `HIRI-AUCA-2`
- APN: `flolive.net`
- API: `http://api-sensores.cmasccp.cl/insertarMedicion`

## Manual de consulta vigente

- [Manual completo en Markdown](../documentacion/manual_tecnico_hiri_calefactor_v2.md).
- [Manual completo en PDF](../documentacion/manual_tecnico_hiri_calefactor_v2.pdf).

El manual usa este sketch como base y explica componentes, función de cada
archivo, tiempos, botón, pantalla, comandos, serial, API y almacenamiento con o
sin calefactor/red. Este README resume la revisión estática del código; las
pruebas físicas indicadas al final siguen pendientes.

## Hardware y pines

| Función | Pines ESP32 |
|---|---|
| SIM7600 | TX 27, RX 26, PWRKEY 4, FLIGHT 25; DTR 32 declarado sin manejo activo. |
| Plantower PMS | Define pms_RX=18 y pms_TX=5, pero construye `pms(pms_TX, pms_RX)`; con firma (RX, TX), recibe en GPIO5 y transmite por GPIO18. Verificar biblioteca y montaje. |
| Calefactor | RX2 19, TX2 23 a 9600 baudios |
| SD HSPI | SCLK 14, MISO 2, MOSI 15, CS 13 |
| Batería | ADC 35 |
| POWER_PIN | GPIO33 declarado; no tiene manejo activo en el firmware vigente. |
| NeoPixel | GPIO 12 |
| BTN1 | GPIO 39, activo en LOW y con resistencia externa |
| BTN2 | Deshabilitado (`-1`) |
| Enable/alimentación I2C | GPIO 0 (`I2C_POWER_PIN`) |
| OLED/RTC | `Wire.begin()` sin pines explícitos; SDA/SCL dependen de la variante de placa seleccionada. |

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
- Reinicio de HIRI configurado a 3 horas, sin comprobación activa en el loop; no se ejecuta periódicamente. El ESP8266 del calefactor sí reinicia cada 4 horas.

## Detección y versión del calefactor

HIRI acepta el identificador vigente `CALEF` y el histórico `CAL`. Al arrancar
envía una consulta `VERSION` no bloqueante. Si no hay respuesta, reintenta cada
30 segundos durante un máximo de 3 minutos y luego deja de consultar.

La falta de versión no impide procesar tramas del protocolo actual. HIRI acepta tanto
`CAL,1,HELLO` sin versión como `CAL/CALEF,1,HELLO,<versión>`, siempre con checksum `*HH`; `HELLO` o `DATA`
siguen estableciendo la conexión. Cuando se recibe una versión válida, se
muestra como `CALEFACTOR FW <versión>` en la OLED. Para guardar/enviar valores
del calefactor se requiere además un DATA recibido y no caducado. Después de
más de 5 segundos sin DATA se marca desconectado; la versión y las últimas
lecturas pueden permanecer visibles en RAM/pantalla.

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
Reinicio HIRI:      3 h configuradas, sin ejecución periódica
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

Los floats no finitos y los campos textuales ausentes se representan con `-1`.
Sin DATA válido del calefactor, sus cinco campos (SHT T/RH, relé y BME T/RH)
se envían como `-1`. CSQ desconocido conserva `99`; PM conserva ceros iniciales
o la última lectura porque no hay caducidad Plantower. GNSS puede conservar
valores iniciales si se omite su arranque por modo offline. El formato T/RH del
Plantower requiere una variante compatible del sensor.

No se incluyen termocupla, referencia externa ni banderas H/T/E en esos 16
valores. La URL no lleva fecha/hora de medición. Éxito HTTP significa código
2xx, sin validar semánticamente el cuerpo de respuesta.

Para `DEVICE_ID_STR = "2"`:

```text
idsSensores=1151,1151,1151,1151,1151,1152,1153,1153,1153,1153,1153,1201,1201,1201,1201,1201
idsVariables=11,12,15,45,46,4,3,6,7,8,9,3,6,57,58,59
```

## Tarjeta SD

Archivo diario:

```text
/HIRI_AUCA_2_DD_MM_AAAA.csv
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

- `/errors_h2.csv`
- `/failed_h2.csv`

El CSV tiene 18 columnas: los 16 valores HTTP reordenados más timestamp y
conexion_http. Guarda cada 3 minutos, independientemente del HTTP cada 5 minutos;
no es una copia de cada envío. Se guardan las últimas lecturas, sin promedios.
El primer HTTP se intenta al entrar al loop; el primer guardado periódico ocurre
a los 3 minutos, o en la primera vuelta si ya está en modo SD sin red.

Timestamp: `AAAA-MM-DD HH:MM:SS`, desde el RTC; sin RTC usa
`2000-01-01 00:00:00`. No hay normalización explícita de zona horaria. El setup
detecta RTC pero no invoca la sincronización inteligente automática; consultar
`rtc`, `modemtime` y `rtcsync`. No se reenvían automáticamente las URLs fallidas.

## Interfaz actual

La pantalla normal rota automáticamente entre vistas de estación. BTN1 ejecuta
una prueba manual compuesta por envío HTTP, guardado SD y solicitud de
calefacción por 10 segundos. El guardado de B1 usa el mismo payload del intento
HTTP, aunque el registro periódico esté detenido; `CAL:CMD` confirma emisión,
no recepción ni encendido. El ESP8266 mantiene sus protecciones y control
autónomo. BTN2 avanzaría la pantalla, pero está deshabilitado
en la configuración de pines actual.

Las estructuras de menú todavía existen en `ui.ino`, pero el flujo de render y
botones vigente retorna desde la vista de estación antes de navegar esos menús.
El gestor WiFi SD existe, pero no tiene acceso desde la navegación normal ni
comando USB de activación. Si se activa por otra ruta, el loop omite adquisición,
serial calefactor, guardado, HTTP y consola mientras atiende web/DNS.

La rotación normal es cada 10 segundos. Consultar el manual vigente para los
estados especiales y las limitaciones de los menús heredados.

## Consola y opciones de configuración

USB a **115200 baudios**, nueva línea, un comando por línea. Consultas:
`help`, `sysinfo`, `config`, `stats`, `rtc`, `modemtime`, `sdinfo`, `sdlist`,
`netinfo`. `cal 10` o `cal.10` solicita prueba de 10 s; rango 1–300.
`stop` detiene HTTP/logging; `start` tras `stop` reactiva HTTP con logging OFF.
Ninguno apaga el control autónomo del ESP8266; B1 mantiene su acción manual.

Los cambios `set` aceptados se guardan en flash, pero el arranque fuerza los
períodos 180/300 s, SD automática, autostart ON y espera GPS OFF.
`set sdsave` y `set httpsend` solo aceptan 3, 60, 600 o 1200 segundos; 180 y 300
no son argumentos admitidos. Reiniciar restaura los períodos de estación.
Las opciones de apagado OLED no tienen aplicación activa. `set led off` aplica
el apagado inmediato, pero el refresco PM puede encender el NeoPixel de nuevo.
`set reboot` guarda una opción sin ejecución periódica. GNSS sí puede
habilitarse con `set gnss on` y reinicio.

## Compilación

Desde la carpeta superior:

```bash
arduino-cli compile --fqbn esp32:esp32:esp32 HIRI_STATIC_CALEFACTOR_0_1
```

## Documentación, licencia y seguridad

- [Manual técnico vigente (Markdown)](../documentacion/manual_tecnico_hiri_calefactor_v2.md)
- [Manual técnico vigente (PDF)](../documentacion/manual_tecnico_hiri_calefactor_v2.pdf)
- [Antecedente técnico local; contrastar con el manual vigente](documentacion/MANUAL_TECNICO.md)
- [Antecedente de usuario local; contrastar con el manual vigente](documentacion/MANUAL_USUARIO.md)
- [Antecedente de interfaz y menús](documentacion/menu_structure.md)
- [Licencia del proyecto](../LICENSE.md)
- [Descargo de responsabilidad](../DISCLAIMER.md)

Creación original: **Alejandro Rebolledo** (`arebolledo@udd.cl`).

Firmware propio y documentación general: **GPL-3.0-only**, solo versión 3.
Hardware propio: **CERN-OHL-S-2.0**. Consultar el [alcance por componente](../LICENSE).

## Proteccion de conexion y modo SD

`StrikesConect` es el contador global de intentos consecutivos de conexion;
`MAX_STRIKES_CONECT = 3` fija el limite. Se incrementa antes de entrar en las
llamadas bloqueantes y se limpia al establecer la conexion de datos. Se conserva
en memoria RTC ante reinicios por software/watchdog, sin escribir la flash.

Los fallos 1 y 2 provocan un reinicio controlado. El fallo 3 activa
`connectionOffline`: se suspenden los intentos celulares y GNSS, se mantiene
el muestreo y se intenta guardar en SD en la primera vuelta del loop y luego
cada 3 minutos. Si un reinicio interrumpe el tercer intento, el siguiente
arranque entra directamente en modo SD. Apagar y volver a encender inicia
una nueva serie de intentos. No hay recuperacion automatica de red en modo SD.

La cabecera muestra `SD OK` si esta montada y no se ha detectado un error de
escritura, o `SD ERR` si falta la tarjeta o falla abrir/escribir el CSV.
Sin SD ni conexion se siguen leyendo sensores, pero no se pueden conservar
las mediciones. No se implementa montaje automatico de una tarjeta insertada
posteriormente; la función WiFi SD podría montarla si se invoca, pero no es
accesible desde la navegación normal actual. Reiniciar vuelve a intentar montaje.

TinyGSM alimenta ahora el watchdog desde `TINY_GSM_YIELD`, dentro de sus esperas
AT. Un reset antes de este cambio podia ocurrir al esperar 60 segundos por la
red con un watchdog de 60 segundos. Tambien se elimino el pulso repetido de
PWRKEY durante la espera de arranque. Los tiempos de espera siguen siendo
bloqueantes; el cambio evita el reset del watchdog, no los convierte en tareas
asincronicas.

Pruebas de banco pendientes:

- Sin cobertura y con SD: verificar strikes 1, 2 y 3, solo dos reinicios y
  crecimiento del CSV cada 3 minutos con `SIN_RED_MODO_SD`.
- Sin cobertura ni SD: verificar modo de medicion estable y `SD ERR`.
- Con red desde encendido: verificar contador en cero y envio HTTP normal.
- Retirar la SD durante muestreo: verificar `SD ERR` tras el siguiente guardado.
- Cortar y restablecer alimentacion: verificar nuevos intentos de conexion.
- Revisar el motivo de reinicio serial para distinguir watchdog de brownout.
