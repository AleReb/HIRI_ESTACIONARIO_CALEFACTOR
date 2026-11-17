# Manual técnico de consulta · HIRI AUCA y calefactor V2

Base: HIRI_STATIC_CALEFACTOR_0_1 · CAL V1.0.2  
Módulo asociado: Calefactor_V2_0_0 · firmware 1.0.0  
Revisión documental: 17 de noviembre de 2026 · fuente inicial: commit 5e726a1

## 1. Alcance y guía rápida

Este manual describe el comportamiento ejecutable de la estación HIRI ESP32 y su módulo calefactor ESP8266. Se revisaron todos los archivos .ino y config.h de HIRI y el sketch del calefactor. Los nombres de carpetas no son la versión del firmware: las versiones efectivas son las constantes VERSION y CAL_FIRMWARE_VERSION.

La estación mide partículas, recibe temperatura/humedad del calefactor, muestra información y registra/envía datos. El calefactor funciona autónomamente: HIRI no decide continuamente el estado del relé. No se usaron como especificación los manuales históricos ni los proyectos de codigo_original.

La revisión es estática: no acredita calibración, funcionamiento del montaje físico, compatibilidad exacta de bibliotecas o recepción efectiva por el servidor. Cuando existe una función sin llamada activa se señala expresamente. Las etiquetas «interna» BME y «externa» SHT40 provienen del esquema de datos; el firmware no verifica dónde están instalados físicamente.

| Consulta | Respuesta de referencia |
|---|---|
| ¿Cada cuánto guarda? | Cada 180000 ms: 3 minutos, con SD disponible y logging activo. |
| ¿Cada cuánto envía? | Cada 300000 ms: 5 minutos; primer intento al comenzar el loop después del arranque. |
| ¿Qué hace B1? | Intenta HTTP, guarda esa misma muestra en SD y solicita prueba de calefacción de 10 s. |
| ¿Cómo consultar? | USB HIRI a 115200 baudios, enviar help, config, stats, rtc, sdinfo. |
| ¿Qué hace sin calefactor? | Sigue midiendo; los cinco campos del calefactor se representan con -1 en SD/HTTP. |
| ¿Qué hace sin red? | Tras tres intentos fallidos de conexión entra en modo SD; conserva medición local. |
| ¿Cuándo enciende el relé? | Humedad ≥65 %, o alguna temperatura ambiental válida <15 °C, o prueba aceptada. |
| ¿Cuándo protege? | Termocupla no finita o ≤0 °C, o ≥70 °C: relé apagado. Emergencia desde 80 °C. |
| ¿Se guardan todos los datos seriales? | No. Termocupla, referencia externa y banderas H/T/E no integran SD/HTTP. |

Los intervalos son objetivos del loop, no garantías de tiempo real. HTTP, red y comandos seriales pueden bloquear la estación. El ESP8266 sigue controlando sus sensores mientras conserva alimentación.

## 2. Componentes y arquitectura

```text
 SHT40 (T/RH) --I2C--+                    Plantower (PM/T/RH)
 BME280(T/RH) --I2C--+                              |
 MAX6675 + termocupla|                         UART 9600
          |         v                              v
          +----> ESP8266 <--- UART 9600 ---> ESP32 HIRI
                 |  |                         |  |  |
             rele   USB diagnostico          SD OLED RTC
                 |                            |       |
          elemento calefactor                 + SIM7600
                                                 |
                                         red celular / HTTP
                                                 |
                                      API insertarMedicion
```

| Componente | Función implementada | Interfaz / observación |
|---|---|---|
| ESP32 | Coordina estación, consola, módem, interfaz y almacenamiento. | Firmware HIRI CAL V1.0.2. |
| Plantower PMS | PM1.0, PM2.5 y PM10; intenta extraer T/RH del formato recibido. | SoftwareSerial, tramas de 32 bytes. Modelo exacto no se autodetecta. |
| SIM7600 + SIM | Red celular, petición HTTP GET, reloj de red y GNSS opcional. | Serial1/SerialAT, APN flolive.net. |
| OLED SSD1306 | Pantallas de 128×64 píxeles. | I2C, detección 0x3C o 0x3D. |
| RTC DS3231 | Fecha/hora local de registros y cabecera; temperatura interna de diagnóstico. | I2C. Temperatura RTC no se envía ni guarda en el CSV vigente. |
| Tarjeta microSD | CSV diarios, errores y URLs de transmisiones fallidas. | SPI HSPI. |
| ADC de batería | Estimación de voltaje, no medición de capacidad. | GPIO35, promedio de 30 muestras. |
| NeoPixel | Gradiente de color según PM2.5. | GPIO12, un píxel. |
| Botón B1 | Acción manual HTTP + SD + solicitud CAL. | GPIO39, activo LOW; requiere resistencia externa de pull-up. |
| Botón B2 | Si se habilita, adelanta pantalla. | Deshabilitado: BUTTON_PIN_2 = -1. |
| ESP8266 / NodeMCU | Control local del calefactor y envío de telemetría. | Firmware 1.0.0. |
| SHT40 | T/RH principal para control por humedad. | I2C; alta precisión, calentador interno SHT deshabilitado. |
| BME280 | T/RH y respaldo de humedad si falla SHT40. | I2C 0x76; no se lee presión ni altitud. |
| MAX6675 + termocupla | Temperatura usada para protección térmica. | Interfaz de tres señales, velocidad configurada 4 MHz. |
| Relé y resistencia | Actuación térmica. | D0 HIGH = encendido lógico; verificar correspondencia física de la placa. |

DHT22 pertenece al calefactor histórico; no se utiliza en V2. Las bibliotecas no prueban el modelo físico instalado: comprobar inventario y cableado.

### 2.1 Conexiones declaradas

| Placa / señal | Pin del firmware |
|---|---|
| HIRI RX desde calefactor TX D4 | GPIO19, Serial2RX_PIN |
| HIRI TX hacia calefactor RX D3 | GPIO23, Serial2TX_PIN |
| Calefactor RX / TX | D3 / D4, SoftwareSerial externo |
| HIRI módem RX / TX | GPIO26 / GPIO27 |
| Módem PWRKEY / DTR / FLIGHT | GPIO4 / GPIO32 / GPIO25; DTR está definido, sin manejo activo. |
| SD SCLK / MISO / MOSI / CS | GPIO14 / GPIO2 / GPIO15 / GPIO13 |
| Alimentación sensores I2C HIRI | GPIO0 HIGH al arrancar; espera 300 ms. |
| Batería / NeoPixel / B1 | GPIO35 / GPIO12 / GPIO39 |
| Calefactor relé | D0 |
| MAX6675 SO / SCK / CS | D6 / D5 / D8 |

Plantower: config.h define pms_RX=18 y pms_TX=5, pero el objeto se construye como SoftwareSerial pms(pms_TX, pms_RX). Bajo la firma habitual (RX, TX), la recepción resulta GPIO5 y la transmisión GPIO18. Hay que comprobarlo con la biblioteca y el cableado; este manual conserva esa diferencia y no modifica la instrucción. El bus I2C usa Wire.begin() sin pines explícitos, por lo que depende de la variante de placa seleccionada. Los enlaces UART requieren TX/RX cruzados, masa común y niveles compatibles con las placas.

## 3. Arranque y ciclo de trabajo de HIRI

### 3.1 Secuencia al energizar

1. Energiza I2C, inicia consola, watchdog de 60 s, bus a 50 kHz y NeoPixel.
2. Recupera estado de conexión retenido, contador HTTP y configuración flash.
3. Aplica enforceStationStartupConfig(): fuerza SD automática, SD a 3 min, HTTP a 5 min, autostart activado y no esperar GPS; guarda si hubo cambios.
4. Intenta montar SD y abre el CSV diario. En este punto todavía no se ha llamado rtc.begin(): el nombre inicial puede llevar 01_01_2000 y se corrige al guardar si luego hay RTC.
5. Detecta OLED, muestra animación y revisa OLED, SD, RTC, Plantower y calefactor. «UART lista» indica inicialización del puerto, no validación del sensor conectado.
6. Abre PMS y Serial2 a 9600; consulta VERSION del calefactor. Mantiene ventana de revisión OLED por 3 s, leyendo serial del calefactor.
7. Configura botones e inicia módem a 115200. Espera AT hasta 45 s, realiza registro de red hasta 60 s y conexión de datos.
8. Inicia GNSS si procede. GNSS viene deshabilitado por defecto.
9. Activa streaming y logging si SDOK. Programa HTTP inmediato; SD a 3 min, o inmediato si ya está en modo sin red.

### 3.2 Loop activo

```text
watchdog + heartbeat + boton
            |
    ¿modo WiFi activo? -- si --> DNS/web y retorno
            | no
GNSS -> temperatura RTC -> CSQ -> Plantower -> LED
            |
serial calefactor -> motor AT -> bateria -> OLED
            |
 SD si corresponde -> HTTP si corresponde -> consola USB
            +-------------------------------> repetir
```

SD y HTTP usan muestras instantáneas construidas desde las últimas variables disponibles. No hay promedio de 3 o 5 minutos, ni conjunto de muestras pendientes. En una coincidencia de períodos, el guardado SD ocurre antes del envío HTTP y su etiqueta HTTP describe el resultado anterior.

### 3.3 Tabla de tiempos

| Proceso | Intervalo / límite | Condición |
|---|---|---|
| CSV SD automático | 180 s | loggingEnabled y SD funcional. |
| HTTP automático | 300 s | streaming y no connectionOffline. |
| Primer HTTP | Primer loop elegible | Después de completar setup. |
| Primer guardado SD | 180 s después de setup | En modo SD sin red se adelanta al primer loop. |
| Control y DATA calefactor | 1000 ms | Sincronizados a la lectura de sensores. |
| HIRI → HELLO | 5000 ms | Desde pollHeaterSerial. |
| HIRI → TEMP | 2000 ms | Si heaterShtTemp no es NaN; se devuelve la temperatura SHT recibida. |
| Consulta VERSION | Inicial, reintentos a 30 s | Ventana máxima 3 min; se detiene al conocer versión. |
| Caducidad calefactor | Más de 5000 ms sin DATA | Solo se evalúa después de haber recibido DATA. |
| Rotación de pantallas | 10000 ms | 3 páginas; 4 si GNSS habilitado. |
| Refresco de OLED | Más de 60 ms | Retrasable por bloqueos. |
| RTC temperatura interna | 2000 ms | Si rtcOK. |
| CSQ | 30000 ms | Si no está en modo offline. |
| NeoPixel | 300 ms | Puede apagarse temporalmente durante HTTP. |
| Batería | 30 muestras, separación mínima 5 ms | Aproximadamente ≥150 ms por promedio; el loop puede alargarlo. |
| Heartbeat por consola | 10000 ms | Informa etapa, heap, SD, streaming, batería. |
| Antirrebote B1 / B2 | 80 ms | Guardia adicional UI de 70 ms. |
| Mensaje normal / resultado B1 | 1500 ms / 3000 ms | Resultado B1 se muestra tras terminar la acción. |
| HTTPACTION | 15 s por defecto; ajustable 5–30 s | No incluye todo el tiempo de reconexión ni los demás comandos AT. |
| Watchdog HIRI / calefactor | 60 s / configurado 30 s | Se alimentan desde el firmware. |
| Reinicio calefactor | 4 h desde arranque | Ticker activo, reinicia ESP8266. |
| Reinicio programado HIRI | Configurado 3 h | No se ejecuta: falta comprobación/llamada en loop. |

## 4. Sensores y calidad de las lecturas

### 4.1 Plantower

readPMS() consume bytes disponibles sin esperar una trama completa. Busca encabezado 0x42 0x4D y verifica la suma de los primeros 30 bytes contra los bytes 30–31. Extrae PM de los bytes 10–15, en µg/m³. Se utilizan las posiciones de concentración atmosférica del formato implementado.

Intenta interpretar bytes 24–25 como temperatura multiplicada por 10 y bytes 26–27 como humedad multiplicada por 10. Acepta T mayor que -40 y menor que 85, RH entre 0 y 100, y rechaza ambos valores crudos simultáneamente cero. Esto no demuestra que cualquier PMS5003 entregue T/RH: esos campos dependen de la variante del sensor; en otra variante pueden significar otros datos. Confirmar el modelo antes de interpretar temp_plantower_c y hum_plantower_pct.

No hay caducidad activa que invalide PM si el sensor deja de transmitir. Antes de la primera trama, PM vale 0; después de una desconexión pueden persistir las últimas lecturas. Por tanto un cero de PM no acredita una medición válida, y una lectura almacenada no acredita que sea reciente.

### 4.2 Sensores del calefactor

SHT40: getEvent() obtiene T/RH cada segundo. BME280: readTemperature() y readHumidity(). MAX6675: read() seguido de getTemperature(); el valor de retorno status se obtiene pero no se usa para decidir el control.

Si BME devuelve lectura no finita, el firmware marca el sensor como fallido y pone T/RH en NaN. Si getEvent() de SHT falla, también lo deshabilita y pone NaN. No hay reintento de inicialización hasta reiniciar. En el caso SHT se comprueba el resultado de getEvent(), sin una comprobación adicional de finitud al asignar; el control sí comprueba finitud.

### 4.3 Valores ausentes y precisión

| Dato | Representación vigente |
|---|---|
| Float válido en SD/HTTP | 3 decimales mediante safeFloatStr, aunque el enlace calefactor solo lleva 1 decimal. |
| Float NaN o infinito | -1 en SD/HTTP. |
| Cinco campos de calefactor sin DATA válido | -1 cada uno; también el relé. |
| Relay válido | 0 apagado, 1 encendido. |
| CSQ desconocido | 99; se conserva como entero, no se transforma a -1. |
| GNSS deshabilitado, arranque normal con gnssBringUp | Coordenadas, velocidad y satélites se convierten a -1. |
| PM sin primera lectura | 0 por inicialización; no existe bandera de validez. |
| Diagnóstico OLED del calefactor desconectado | Puede conservar valores antiguos; texto SIN DATOS indica desconexión. |

-1 es un marcador del esquema, no una temperatura físicamente imposible: puede coincidir con una temperatura real de -1 °C. El esquema no incorpora bandera independiente de calidad por sensor.

## 5. Pantalla y botón

### 5.1 Pantallas normales

El render activo es drawStationView(), con rotación automática cada 10 s. Con GNSS apagado se ven Plantower → Calefactor → Estado estación. Con GNSS encendido se antepone GPS y hora.

| Página | Contenido |
|---|---|
| PLANTOWER | PM1.0, PM2.5, PM10 en µg/m³, temperatura y humedad PMS. |
| CALEFACTOR | Versión si fue conocida; SHT T/RH o SIN DATOS; BME T/RH; TERM, REL ON/OFF y banderas H, T, E. |
| ESTADO ESTACION | ID, versión HIRI, batería en V y porcentaje estimado, CSQ/tecnología, contadores TX y SD, modo STREAM + LOG / STREAM ONLY / STANDBY. |
| GPS Y HORA | Latitud, longitud, altitud, velocidad, satélites, HDOP y fecha/hora RTC. Solo con GNSS habilitado. |

H = solicitud por humedad; T = solicitud por temperatura; E = emergencia. No hay bandera visible dedicada a la prueba manual. Puede observarse REL ON con H=0 y T=0 durante una prueba.

```text
 +-----------------------------------------+
 | HH:MM:SS   SD OK    N    CSQ    bateria  |
 | CALEFACTOR FW 1.0.0                 2/3  |
 | SHT: 18.5C / 67.0%                      |
 | BME: 24.0C / 52.0%                      |
 | TERM: 40.0C REL:ON                      |
 | H:1 T:0 E:0                            |
 +-----------------------------------------+
 Ejemplo esquematico; valores ilustrativos.
```

La cabecera muestra hora HH:MM:SS o ??:??:??, SD OK/SD ERR, N si GNSS apagado o satélites/advertencia, estado de red y batería. SD OK indica tarjeta montada y ausencia de error de escritura marcado; no prueba que logging esté activo. La versión conocida del calefactor puede mantenerse visible después de desconectarlo.

Hay código de menús, notas, RTC, información de red, almacenamiento y gestor WiFi. No constituye la navegación actual: el render normal usa las páginas de estación y ui_btn2_click() retorna después de adelantar página. Las ramas posteriores del menú quedan inalcanzables por esa ruta.

### 5.2 Botón B1: funcionamiento efectivo

1. Tras nivel LOW estable por más de 80 ms se ejecuta ui_btn1_click(). Mantener presionado no genera una función larga adicional: hay un evento por transición estable.
2. Intenta enviar una muestra actual a HTTP, aunque streaming esté detenido.
3. Guarda en SD el mismo conjunto de 16 valores usado en ese intento; etiqueta HTTP_MANUAL_OK o HTTP_MANUAL_FALLO. No depende de loggingEnabled.
4. Envía HIRI,1,CAL,10 al calefactor y muestra durante 3 s el resultado, por ejemplo TX:OK SD:OK CAL:CMD.

CAL:CMD solo confirma que HIRI emitió el comando; no confirma recepción ni encendido. La aceptación depende de la termocupla y de las protecciones del ESP8266. HTTP se realiza antes del guardado y del comando CAL: con red lenta la respuesta del botón puede tardar. Un fallo de conexión puede reiniciar HIRI antes de completar los pasos siguientes. No se altera el calendario lastHttpSend/lastSdSave de los eventos automáticos.

En estados especiales ya activos, B1 cancela un prompt, sale de RTC o sale de WiFi si está inactivo; con WiFi activo queda bloqueado por esa rama. Esos estados no son accesibles desde la navegación normal vigente.

B2 está deshabilitado. Si se asigna un GPIO real, usa interrupción RISING (soltar en un botón activo LOW) y adelanta pantalla; no habilita por sí solo los menús antiguos.

### 5.3 NeoPixel

PM2.5 ≤15: verde de intensidad creciente; entre 15 y 25: verde hacia amarillo; entre 25 y 50: amarillo hacia rojo; sobre 50: rojo. Son umbrales visuales del código, no una clasificación sanitaria documentada. updatePmLed() no comprueba config.ledEnabled, por lo que set led off puede ser revertido por el refresco de color. El brillo sí se aplica mediante setBrightness().

## 6. Consola de consulta y mantenimiento

Conectar al USB del ESP32, 115200 baudios, nueva línea LF. Comandos sin checksum, uno por línea. Se recortan espacios externos y se convierten a minúsculas. El USB del ESP8266 es de diagnóstico; no procesa estos comandos de HIRI.

| Comando | Función real / consideración |
|---|---|
| help o ? | Lista de ayuda. cal está implementado aunque no aparece en esa lista. |
| sysinfo / mem | Versión, chip, heap y uptime; mem detalla memoria. |
| stats / counters | Estado y contadores. SD es de la sesión; HTTP se recupera de flash. |
| resetcnt | Pone a cero contadores; no borra archivos SD. |
| rtc | Fecha/hora, epoch y temperatura DS3231. |
| modemtime | Hora obtenida por AT+CCLK?. |
| rtcsync | Intenta ajustar RTC si diferencia absoluta >30 s; puede decir fallo si ya estaba dentro del umbral. |
| sdinfo / sdlist | Espacio, archivo activo y lista de archivos raíz. |
| sdnew | Selecciona nombre diario y escribe cabecera si vacío; no genera un nombre único en el mismo día. |
| sdclear | Solo muestra advertencia e instrucción de confirmación. |
| sdclear confirm | Borra hasta 50 archivos no directorio de la raíz. Acción destructiva. |
| netinfo / csq | Estado de red y consulta de señal. csq=99 significa desconocida aunque su texto de clasificación pueda decir excellent. |
| start | Si estaba detenido, activa HTTP y deja logging SD desactivado. Si ya está activo no cambia nada. |
| stop | Si estaba activo, detiene HTTP automático y logging. No detiene sensores ni control del ESP8266. |
| cal 10 / cal.10 | Solicita calefacción por 10 s. Se aceptan enteros 1–300. |
| reboot | Reinicia HIRI tras espera de 2 s. No reinicia directamente ESP8266. |
| config | Imprime configuración cargada. |
| config sd/http/display/power/system | Consultar un grupo: escribir un nombre, por ejemplo config sd. |
| configsave | Guarda configuración en Preferences. |
| configreset | Restaura valores del código, guarda y aplica LED; reiniciar para aplicar todo. |

La tasa mostrada por counters/stats es sendCounter/sdSaveCounter×100. No es una tasa fiable de éxito HTTP: los períodos son distintos, HTTP persiste entre reinicios y SD no se restaura al arrancar.

### 6.1 Parámetros set

Los cambios aceptados se guardan automáticamente en flash. Un campo configurado no garantiza que exista una ruta que lo aplique.

| Sintaxis | Valores / efecto |
|---|---|
| set sdsave N | Solo 3, 60, 600, 1200 segundos. Se fuerza 180 s al reiniciar. |
| set httpsend N | Solo 3, 60, 600, 1200 segundos. Se fuerza 300 s al reiniciar. |
| set sdauto on/off | Campo persistente; setup monta SD de todos modos y fuerza on al arrancar. |
| set httptimeout N | 5–30 segundos para espera HTTPACTION. |
| set oledoff on/off | Guarda opción; no hay setPowerSave activo que la ejecute. |
| set oledtime N | 60, 120, 180 segundos; temporizador de apagado no aplicado. |
| set led on/off | Aplica estado inmediato; refresco PM puede volver a encenderlo. |
| set ledbright N | 10, 25, 50, 100 %. |
| set autostart on/off | Se fuerza on en el siguiente arranque. |
| set autowaitgps on/off | Se fuerza off en el siguiente arranque; no hay espera GPS en setup. |
| set autogpsto N | 60–900 s; sin ruta de espera activa. |
| set rotatedisplay on/off | on aplica U8G2_R0 y off U8G2_R2. Nombres no equivalen literalmente a girado/no girado del montaje físico. |
| set reboot N | 0–720 horas; guarda/muestra, pero no existe ejecución periódica de este reinicio en HIRI. |
| set gnss on/off | Se aplica al reiniciar. Por defecto off. |
| set gnssmode N | 1, 3, 5, 7, 15; aplicar reiniciando. |

No existen set sdsave 180 ni set httpsend 300. Para volver a esos valores de estación, reiniciar; configreset también los establece pero restablece otras opciones. La rotación por defecto de loadConfig es true si falta la clave, mientras configSetDefaults la deja false.

Ejemplo de consulta sin cambiar configuración:

```text
sysinfo
config
rtc
sdinfo
netinfo
stats
```

## 7. Comunicación HIRI ↔ calefactor

### 7.1 Formato común

UART ASCII a 9600, 8N1. Trama = payload + asterisco + dos dígitos hexadecimales + fin de línea. println() emite CRLF; ambos receptores toleran ese CR y usan LF. El checksum es la suma de bytes del payload módulo 256, sin incluir *HH ni CR/LF. No es CRC, cifrado ni autenticación.

HIRI valida checksum, acepta ID CALEF o CAL, y limita el acumulador a 180 caracteres. ESP8266 recibe con readStringUntil, potencialmente bloqueante hasta timeout si la línea está incompleta. No hay número de secuencia, acuse de prueba CAL ni fecha/hora en DATA.

| Sentido | Payload antes de *HH | Función |
|---|---|---|
| HIRI → módulo | HIRI,1,HELLO | Sondeo cada 5 s. |
| HIRI → módulo | HIRI,1,VERSION | Solicitud opcional de versión. |
| HIRI → módulo | HIRI,1,TEMP,18.5 | Actualiza tempsens, referencia de temperatura para control. |
| HIRI → módulo | HIRI,1,CAL,10 | Solicitud de prueba temporizada. |
| Módulo → HIRI | CALEF,1,HELLO,1.0.0 | Anuncio al arrancar y respuesta a HELLO. |
| Módulo → HIRI | CALEF,1,VERSION,1.0.0 | Respuesta de versión. |
| Módulo → HIRI | CALEF,1,DATA,... | Diez valores de telemetría por segundo. |

HIRI devuelve por TEMP la última temperatura SHT40 recibida del propio calefactor, no la temperatura del Plantower. Ese envío se condiciona a que no sea NaN, sin comprobar antigüedad o conexión. ESP8266 conserva tempsens indefinidamente, inicializada a 16 °C; no caduca al perder HIRI.

### 7.2 Orden de DATA

```text
CALEF,1,DATA,tempsens,shtTemp,tempBME,shtHum,humBME,
  tempThermo,relayOn,heaterByHumidity,heaterByTemperature,
  emergencyMode*HH
```

La trama real ocupa una sola línea. Temperaturas en °C, humedades en %, floats con un decimal y banderas 0/1. En la consola USB del calefactor se imprime el mismo payload sin *HH; en UART externo sí se añade el checksum.

| Posición después de DATA | Variable / significado |
|---|---|
| 1 | tempsens: referencia recibida de HIRI. |
| 2 / 3 | Temperatura SHT40 / BME280. |
| 4 / 5 | Humedad SHT40 / BME280. |
| 6 | Temperatura de termocupla. |
| 7 | relayOn. |
| 8 / 9 / 10 | Solicitud por humedad / temperatura / emergencia. |

Ejemplos completos con checksum calculado:

```text
HIRI,1,HELLO*29
HIRI,1,VERSION*DB
HIRI,1,TEMP,18.5*E3
HIRI,1,CAL,10*12
CALEF,1,HELLO,1.0.0*71
CALEF,1,DATA,18.5,18.5,24.0,67.0,52.0,40.0,1,1,0,0*26
```

El parser HIRI reparte trece campos y convierte con toFloat/toInt, pero no valida exhaustivamente cantidad de campos, texto numérico ni rango físico. Una trama con checksum correcto no equivale por sí sola a lectura físicamente válida. HELLO/VERSION pueden marcar conexión antes del primer DATA; SD/HTTP exigen además heaterLastDataMs distinto de cero.

## 8. Control autónomo del calefactor V2

En setup se apaga relé, inicia sensores, anuncia versión y programa reinicio a 4 h. El loop alimenta watchdog, recibe comandos y cada segundo lee sensores, actualiza control y transmite DATA.

```text
             leer termocupla y sensores
                         |
       ¿no finita, <=0 C, o >=70 C?
            si |                  | no
        rele OFF           ¿emergencia previa?
        H=T=0                 si |       | no
        cancela prueba       OFF         actualizar H/T
        si >=80: E=1       limpia E      aplicar H o T
                           si <70        o prueba manual
```

### 8.1 Humedad e histéresis

Usa SHT40 si su humedad es finita; en su defecto BME280. RH≥65 activa heaterByHumidity. RH≤60 la desactiva. En 60<RH<65 conserva el estado anterior. Si no hay humedad válida, desactiva esa solicitud. Desde RH≥70 emite aviso por USB; el aviso no es un nuevo modo de control ni una corrección numérica de partículas.

### 8.2 Temperatura y combinación de solicitudes

Si cualquiera de tempsens, shtTemp o tempBME es finita y menor que 15 °C, activa heaterByTemperature. A 15 °C deja de cumplir la condición; no hay histéresis térmica ambiental. Se combina por OR con humedad y prueba manual: desactivar la solicitud de humedad no garantiza relé apagado si otra solicitud sigue activa.

### 8.3 Protecciones y recuperación

Termocupla no finita o ≤0 °C, o temperatura ≥70 °C: apaga relé, borra H/T y cancela prueba manual. A ≥80 °C también activa emergencyMode. Con emergencia previa y temperatura ya inferior a 70 °C, limpia emergencia pero conserva relé apagado en esa pasada; puede volver al control normal en la lectura siguiente. No queda bloqueado indefinidamente.

La protección usa el valor devuelto por getTemperature(), no el código status de read(). No existe validación de rango para temperatura/humedad ambiental más allá de finitud en el control. No se debe interpretar el firmware como certificación de protección física del montaje.

### 8.4 Prueba manual

CAL acepta exclusivamente dígitos, duración entre 1 y 300 s, termocupla válida >0 y <70 °C, sin emergencia. Marca heaterByManualTest y fecha de fin en millis. El relé se actualiza en la siguiente pasada de control, aproximadamente en hasta un segundo si no hay bloqueos. Una nueva prueba aceptada reemplaza la fecha de fin. Al terminar, solo desaparece la solicitud manual: puede seguir calentando por H/T. No existe CAL 0 para detener una prueba; 0 se rechaza.

La bandera de prueba no se transmite. verifyChecksum() y numberString son restos del protocolo anterior: el flujo actual usa decodeFrame()/sendFrame(), no esa función histórica.

## 9. Envío al servidor

### 9.1 Transporte y resultado

HTTP GET por módem SIM7600 a:

```text
http://api-sensores.cmasccp.cl/insertarMedicion
```

API_BASE se concatena con tres parámetros: idsSensores, idsVariables y valores. Son listas separadas por comas alineadas por posición. En la revisión DEVICE_ID_STR="2", por lo que la estación es HIRI-AUCA-2. El código admite IDs 1–4; no los asigna dinámicamente.

ensurePdpAndNet() verifica registro/PDP y NETOPEN. httpGet_webhook() realiza HTTPTERM previo, HTTPINIT, HTTPPARA CID/URL, HTTPACTION=0, HTTPREAD si hay cuerpo y HTTPTERM final. Se considera éxito cualquier código 200–299; no se interpreta el contenido para confirmar inserción semántica. No hay sello de tiempo de medición en la URL: el comportamiento de fecha del servidor no se puede determinar con este repositorio.

### 9.2 Diccionario de los 16 valores

| Nº | Valor / origen | Unidad | ID variable |
|---|---|---|---|
| 1 | Latitud GNSS | grados decimales | 11 |
| 2 | Longitud GNSS | grados decimales | 12 |
| 3 | CSQ del módem | índice 0–31, 99 desconocido | 15 |
| 4 | Velocidad GNSS | km/h | 45 |
| 5 | Satélites | cantidad | 46 |
| 6 | Voltaje batería ADC | V | 4 |
| 7 | Temperatura Plantower | °C | 3 |
| 8 | Humedad Plantower | % | 6 |
| 9 | PM1.0 | µg/m³ | 7 |
| 10 | PM2.5 | µg/m³ | 8 |
| 11 | PM10 | µg/m³ | 9 |
| 12 | Temperatura SHT40 calefactor | °C | 3 |
| 13 | Humedad SHT40 calefactor | % | 6 |
| 14 | Relé calefactor | 0/1 | 57 |
| 15 | Temperatura BME280 calefactor | °C | 58 |
| 16 | Humedad BME280 calefactor | % | 59 |

| Estación | Sensor posiciones 1–5 | Posición 6 | Posiciones 7–11 | Posiciones 12–16 |
|---|---|---|---|---|
| 1 | 1144 | 1145 | 1146 | 1200 |
| 2 (actual) | 1151 | 1152 | 1153 | 1201 |
| 3 | 1158 | 1159 | 1160 | 1202 |
| 4 | 1165 | 1166 | 1167 | 1203 |

Ejemplo ilustrativo para estación 2 (dividido para lectura; la URL real es continua):

```text
?idsSensores=1151,1151,1151,1151,1151,1152,1153,1153,
1153,1153,1153,1201,1201,1201,1201,1201
&idsVariables=11,12,15,45,46,4,3,6,7,8,9,3,6,57,58,59
&valores=-1,-1,18,-1,-1,3.900,20.000,50.000,5,8,10,
18.500,67.000,1,24.000,52.000
```

### 9.3 Fallos y reintentos

Un fallo HTTP que retorna normalmente guarda la URL en /failed_h2.csv con HTTP_FAIL, si SD está disponible. No hay reenvío automático de esas URLs. El siguiente evento periódico envía una nueva muestra actual. Si el fallo provoca reinicio dentro de connectionFailed(), puede no alcanzarse el registro de URL del intento en curso.

Los fallos de conexión de red/PDP reservan un strike antes de la operación. Se permite un máximo de tres; los primeros fallos reinician la estación y el tercero entra a modo SD sin más intentos automáticos de red. Un éxito de conexión limpia strikes, aunque la API posteriormente pueda responder con error. Un error HTTP por sí solo no equivale a strike de conexión.

Se declaran seis fallos HTTP y una pausa de una hora como constantes, pero no hay lógica activa que las aplique; httpBackoffActive permanece false en el flujo revisado. No contar con esa pausa. El contador de strikes sobrevive a reinicios de software/watchdog mediante memoria RTC; al encendido/brownout se reinicializa. Un reinicio software no garantiza salir del modo offline; para nueva oportunidad de conexión se requiere reinicializar ese estado, por ejemplo mediante un ciclo de alimentación.

## 10. Guardado SD, fecha y hora

### 10.1 CSV diario

Patrón: /HIRI_AUCA_ID_DD_MM_AAAA.csv. Ejemplo: /HIRI_AUCA_2_17_11_2026.csv. Se añade al archivo existente y se escribe cabecera solo si está vacío. Se comprueba el nombre esperado en cada guardado para rotar al cambiar fecha RTC. Una corrección de fecha puede cambiar el archivo utilizado.

El CSV tiene 18 columnas: los mismos 16 valores de telemetría, reordenados, más timestamp y conexion_http. No es un archivo de cada transmisión HTTP: SD y HTTP automáticos son independientes. Solo la acción B1 guarda exactamente el payload de su intento HTTP.

| Nº | Columna exacta | Contenido |
|---|---|---|
| 1 | timestamp | RTC: AAAA-MM-DD HH:MM:SS. |
| 2 | pm_1_0_ug_m3 | PM1.0. |
| 3 | pm_2_5_ug_m3 | PM2.5. |
| 4 | pm_10_ug_m3 | PM10. |
| 5 | temp_plantower_c | T PMS. |
| 6 | hum_plantower_pct | RH PMS. |
| 7 | temp_interna_calefactor_bme_c | T BME. |
| 8 | hum_interna_calefactor_bme_pct | RH BME. |
| 9 | temp_externa_calefactor_sht40_c | T SHT. |
| 10 | hum_externa_calefactor_sht40_pct | RH SHT. |
| 11 | relay_calefactor | 0, 1 o -1 ausente. |
| 12 | senal_csq | CSQ entero. |
| 13 | conexion_http | Etiqueta textual; ver tabla siguiente. |
| 14 | latitud | GNSS. |
| 15 | longitud | GNSS. |
| 16 | velocidad_kmh | GNSS. |
| 17 | satelites | GNSS. |
| 18 | voltaje_v | ADC. |

| Etiqueta conexion_http | Significado |
|---|---|
| SIN_RED_MODO_SD | Estado de conexión offline, no se intenta HTTP periódico. |
| SIN_INTENTO | No se ha completado un intento HTTP durante esa sesión. |
| ULTIMO_HTTP_OK | Último intento completado fue 2xx; no corresponde necesariamente a esta fila. |
| ULTIMO_HTTP_FALLO | Último intento completado falló. |
| HTTP_MANUAL_OK | Fila de botón; el mismo payload obtuvo respuesta 2xx. |
| HTTP_MANUAL_FALLO | Fila de botón; el mismo payload falló. |

Ejemplo conectado, con resultado HTTP anterior exitoso:

```text
2026-11-17 12:03:00,5,8,10,20.000,50.000,24.000,52.000,
18.500,67.000,1,18,ULTIMO_HTTP_OK,-1,-1,-1,-1,3.900
```

Ejemplo sin calefactor y en modo SD sin red:

```text
2026-11-17 12:06:00,5,8,10,20.000,50.000,-1,-1,-1,-1,
-1,99,SIN_RED_MODO_SD,-1,-1,0.0,0,3.900
```

Cada ejemplo corresponde a una sola línea real. En el arranque offline se omite gnssBringUp(), de modo que velocidad y satélites pueden conservar los iniciales 0.0 y 0; no debe suponerse que todo GNSS ausente es siempre -1.

saveMeasurementToSD() verifica 16 valores de entrada, abre FILE_APPEND, hace println, flush, comprueba cantidad escrita y error, cierra y aumenta sdSaveCounter solo si la escritura se considera completa. Un fallo de SD marca sdWriteError, pero no existe remontaje automático periódico ni una cola alternativa en flash.

### 10.2 Otros archivos y memoria

| Medio / archivo | Contenido |
|---|---|
| /errors_h2.csv | timestamp,type,context,message; eventos de error, no cada muestra. Suprime repeticiones del mismo tipo/contexto dentro de 5 s. |
| /failed_h2.csv | timestamp,error_type,url; URL entre comillas porque contiene comas. |
| Flash Preferences config | Opciones del struct SystemConfig. |
| Flash Preferences system | Contador HTTP, nombre CSV y algunos estados de operación. |
| Flash Preferences rtc | Contador de ajustes de reloj cuando se realiza rtcsync. |
| Memoria RTC ESP32 | Strikes y etapa previa para diagnóstico de reinicios. No es un respaldo CSV. |
| RAM ESP8266 | Últimas lecturas, solicitudes y estado de control; no historial permanente. |

Los sufijos h2 corresponden al ID actual; cambian a h1/h3/h4 según compilación. No se guardan en el CSV vigente: tempThermo, tempsens, heaterByHumidity, heaterByTemperature, emergencyMode, prueba manual, temperatura RTC, altitud, HDOP, versión de firmware, motivo de reinicio ni notas de menú. Parte de esa información solo está disponible en consola/pantalla.

### 10.3 Formatos temporales y sincronización

| Lugar | Formato / origen |
|---|---|
| CSV mediciones y fallos HTTP | AAAA-MM-DD HH:MM:SS, RTC. |
| Nombre de archivo | DD_MM_AAAA. |
| Cabecera OLED | HH:MM:SS. |
| Página GPS/hora OLED | DD/MM/AAAA HH:MM, RTC. |
| Error log con RTC | DateTime.timestamp(): AAAA-MM-DDTHH:MM:SS. |
| Error log sin RTC | millis(), milisegundos desde arranque. |
| Falta RTC en mediciones/fallos | 2000-01-01 00:00:00 como respaldo; no fecha real. |
| Consola rtc | Fecha/hora legible y epoch en segundos. |
| Respuesta módem | +CCLK: "aa/mm/dd,hh:mm:ss±zz"; getModemEpoch ignora ±zz. |
| Variables NMEA | gpsDate DD/MM/20AA; gpsTime HH:MM:SS. No van al CSV vigente. |
| Trama calefactor / URL HTTP | Sin timestamp. |

No hay tratamiento explícito de America/Santiago, UTC ni cambio de hora estacional en el formato CSV. mktime() depende de la configuración del entorno y el sufijo horario de CCLK no se aplica: no asumir que el registro está normalizado a UTC o a hora local chilena.

En el setup vigente se llama rtc.begin(), pero no syncRtcSmart(). Por eso no se garantiza ajuste automático al compilar ni sincronización de red al arrancar. rtcsync solicita CTZU/CTZR y solo ajusta si la diferencia supera 30 s. Cuando no obtiene hora deja rtcNetSyncPending, pero no hay un consumidor de esa bandera en loop que reintente. La rutina smart y su límite de tres ajustes no forman parte del arranque efectivo.

## 11. Operación con y sin conexión

| Situación | SD | Servidor | Calefactor / control |
|---|---|---|---|
| Red, SD y calefactor disponibles | Cada 3 min, 18 columnas. | Cada 5 min, 16 valores. | Control autónomo y DATA cada segundo. |
| Sin calefactor o DATA caducado | Continúa; cinco campos calefactor -1. | Continúa con esos cinco campos -1. | Si ESP8266 sigue energizado, mantiene control local y antigua tempsens. |
| HELLO/VERSION, sin primer DATA | Cinco campos -1. | Cinco campos -1. | Un anuncio no basta para disponer de lecturas. |
| Sin red tras tres intentos | Inmediato al entrar y luego cada 3 min. | Sin intentos automáticos nuevos. | Recepción UART y control local siguen. |
| HTTP responde error con PDP válido | Continúa; etiqueta último fallo. | Intenta muestra nueva al próximo período; URL fallida si se alcanza su registro. | Control local sigue. |
| Sin SD | Sin CSV local ni archivos de error/fallos. | Puede continuar. | Continúa. |
| Sin SD ni red | Sin respaldo permanente de mediciones. | Sin entrega. | Solo valores en RAM/pantalla/consola y control autónomo. |
| Plantower deja de transmitir | Puede guardar PM antiguos o iniciales. | Puede enviar PM antiguos o iniciales. | No detiene calefactor. |
| stop por consola | Desactiva logging cuando streaming estaba activo. | Detiene HTTP automático. | Lectura, pantalla y control local continúan; B1 sigue siendo manual. |
| start tras stop | Deja logging OFF. | Reactiva streaming. | Para recuperar arranque normal con SD, reiniciar HIRI. |
| Modo WiFi si se activa por otra ruta | Loop omite guardado periódico. | Loop omite HTTP y consola. | ESP32 omite pollHeaterSerial; ESP8266 sigue autónomo. |
| Corte de energía | Conserva lo ya escrito; no recupera muestras aún en RAM. | No hay reenvío histórico. | Relé se inicializa LOW al nuevo arranque. |

### 11.1 Gestor WiFi SD presente en el código

startWifiApServer() monta SD, crea AP HIRI-AUCA-2, contraseña 12345678, IP 192.168.4.1 y servidor en puerto 80; admite listar, descargar, subir, renombrar y borrar archivos. Rutas: GET /, /download, /ip, /ssid; POST /delete, /rename, /upload, /delete_all. El borrado web procesa hasta 256 archivos raíz; la lista presenta hasta 100.

No hay comando USB para activar ese modo ni camino normal accesible del menú actual. No debe describirse como función disponible mediante pulsación larga. Si llega a activarse por otra ruta, loop atiende web/DNS y retorna antes de sensores, serial calefactor, SD, HTTP y consola. B1 sigue leyéndose pero en la pantalla WiFi activa queda bloqueado; B2 no alcanza su antigua rama de apagado. Una salida práctica para volver al flujo de arranque es reiniciar.

## 12. Mapa de todo el código vigente

Los .ino de una misma carpeta se compilan juntos como un sketch Arduino. Esta tabla cubre cada archivo fuente del firmware HIRI; los proyectos de codigo_original son antecedentes separados.

| Archivo HIRI | Responsabilidades y funciones principales |
|---|---|
| config.h | Pines, definición SIM7600, buffer AT de 4096, hook TinyGSM para watchdog, struct SystemConfig y estados de pantalla. |
| HIRI_STATIC_CALEFACTOR_0_1.ino | Globales; watchdog, etapas y heartbeat; arranque obligatorio de estación; OLED de boot; motor AT (atBegin/atTick/atRun/sendAtSync); red/strikes; IDs y payload; sensores RTC; botones; setup y loop. |
| config.ino | loadConfig/saveConfig/configSetDefaults/printConfig; consulta SD; lista y borrado de hasta 50 archivos; aplicación de brillo/LED. |
| PMSandSensorEXTRA.ino | Parser binario Plantower con checksum, PM y T/RH; interpolación de colores NeoPixel. |
| heater_serial.ino | Checksum del enlace, identificación CAL/CALEF, parseo DATA, consulta versión, HELLO, TEMP, solicitud CAL y timeout. |
| http.ino | PDP/NETOPEN, comandos de HTTP, lectura de respuesta, evaluación 2xx y log de errores. |
| sd_card.ino | Payload de 16 valores a CSV de 18 columnas, rotación diaria, cabeceras y URLs fallidas. El formato saveCSVData antiguo está en #if 0 y no se compila. |
| rtc.ino | Interpretación de CCLK, epoch de compilación, rutina smart no llamada en setup, ajuste manual del RTC y contador de ajustes. |
| gps.ino | NMEA GGA/RMC/VTG/GSA/GSV, coordenadas decimales, GNSS con fallback de constelaciones, diagnóstico, recuperación HOT/WARM y rutinas XTRA. |
| helpers.ino | Sanitización y -1, motivo de reset, log con supresión de repetidos, hora de red para UI y tamaño de CSV. |
| ui.ino | Cabecera y batería estimada; páginas de estación; B1 manual, B2 de página; mensajes y funciones históricas de menús/RTC/WiFi. |
| serial_commands.ino | Parser USB, ayuda y comandos de consulta, control y configuración. |
| wifi.ino | HTML del gestor SD, validación de rutas, operaciones de archivos, AP, DNS y servidor web. |
| animacion.ino | Dibuja logo HIRI/PRO con offsets que setup mueve; no procesa ni almacena mediciones. |

### 12.1 Motor AT y GNSS

atBegin inicia una operación y vacía respuestas previas; atTick consume respuesta, enruta NMEA y detecta cadenas esperadas o timeout. atRun y sendAtSync envuelven esa máquina con esperas bloqueantes alimentando watchdog. No hay tareas paralelas de adquisición que garanticen leer UART mientras una llamada bloqueante ocupa el loop.

GNSS se configura con modos 1 GPS, 3 GPS+GLONASS, 5 GPS+BeiDou, 7 GPS+GLONASS+BeiDou y 15 todas las constelaciones indicadas por el firmware; si el módem rechaza el modo, intenta 7, 3 y 1. Solicita NMEA a 1 Hz. GGA actualiza fix, satélites, HDOP, altitud y posición; RMC estado, fecha/hora y posición; VTG velocidad; GSA/GSV métricas diagnósticas de satélites, DOP y SNR.

Si falta fix, gnssWatchdog solicita HOT desde más de 120 s y WARM desde más de 300 s desde el último inicio. Cada reinicio reinicia ese temporizador: en la práctica los HOT repetidos pueden impedir alcanzar el umbral WARM durante llamadas regulares. No se invalida automáticamente haveFix por edad si dejan de llegar sentencias; vigilar también el diagnóstico ageNMEA/ageGGA.

gnssDiagTick calcula tasa NMEA cada segundo, imprime cada 5 s, intenta reactivar salida tras más de 5 s sin NMEA y asegura alimentación/CGPS tras más de 15 s. Esas acciones usan atRun y pueden bloquear brevemente. gnssDebugPollAsync consulta CGPSSTATUS y CGPSINFO por estados cada ciclo de unos 5 s. TTFF se imprime una vez tras obtener fix.

detectAndEnableXtra comprueba soporte de asistencia; las rutinas de descarga y refresco cada 3 días existen, pero downloadXtraIfDue() no se llama en loop y el resultado de detección no se asigna a xtraSupported en setup. No se garantiza descarga XTRA automática.

### 12.2 Funciones del sketch del calefactor

| Grupo | Funciones / propósito |
|---|---|
| Protocolo | frameChecksum/sendFrame/decodeFrame y readExternalTemperature: valida mensajes y despacha HELLO, VERSION, TEMP y CAL. |
| Arranque y ciclo | setup inicializa relé, UART y sensores; loop alimenta watchdog y agenda lectura/control/DATA. |
| Sensores | readSensors actualiza MAX6675, BME280 y SHT40, deshabilitando los sensores I2C que fallan. |
| Control | updateControl protege y combina solicitudes; updateHumidityControl aplica histéresis; updateTemperatureControl evalúa <15 °C. |
| Apoyo | getControlHumidity selecciona SHT/BME; isValidReading comprueba finitud; setRelay escribe D0. |
| Prueba | startTimedManualHeating valida 1–300 s y condiciones térmicas. |
| Salida | printValues forma DATA, lo imprime en USB y UART y emite aviso RH alta por USB. |
| Reinicio | restartESP reinicia a las 4 h mediante Ticker. |
| Legado | verifyChecksum no se invoca en el flujo actual. |

## 13. Procedimientos de consulta en terreno

### 13.1 Verificación inicial

1. Abrir USB HIRI a 115200 LF y registrar sysinfo, config, rtc, sdinfo y stats.
2. Confirmar ID de estación, RTC real y SD OK. Un archivo con año 2000 o fecha incorrecta requiere revisar reloj antes de interpretar la serie.
3. Revisar PLANTOWER y calefactor durante al menos un ciclo de 30 s sin GNSS: comparar pantalla con mensajes [CALEFACTOR][RX].
4. Esperar el evento SD de 3 min y HTTP de 5 min; comprobar [SD][TX] y [HTTP] code. La primera petición HTTP ocurre al entrar al loop.
5. Copiar el CSV y verificar 18 columnas. Revisar en especial -1 del calefactor y la etiqueta de conexión.

### 13.2 Consulta o prueba del calefactor

Consultar RX serial y observar TERM, REL y H/T/E. Si se requiere una prueba, cal 10 solicita 10 s; no acredita por sí solo actuación. Observar la termocupla y el relé durante la prueba. B1 además produce un envío/guardado manual. Al vencer la prueba puede continuar el relé por humedad o baja temperatura.

### 13.3 Diagnóstico rápido

| Síntoma | Revisar |
|---|---|
| SD ERR | Presencia, montaje, espacio y errores de escritura; sdinfo/sdlist. No hay recuperación periódica automática. |
| Calefactor SIN DATOS | Alimentación ESP8266, RX/TX cruzados, GND, baudios y checksum; buscar DATA, no solo HELLO. |
| Relé no enciende con CAL | MAX6675 >0 y <70 °C, ausencia de emergencia; CAL:CMD no es ACK. |
| Relé sigue ON al finalizar CAL | Solicitudes H/T; control por humedad o temperatura aún activo. |
| Valores BME/SHT antiguos en OLED | Ver heaterConnected y timeout; el almacenamiento aplica -1, la pantalla puede conservar RAM. |
| HTTP falla | netinfo, CSQ, SIM/APN, respuesta HTTP y errors_hID/failed_hID. No usar solo el LED como prueba de entrega. |
| No vuelve la red tras reboot | Strikes conservados en RTC; modo offline persiste a reinicios software. |
| GPS muestra N | GNSS deshabilitado por configuración; no necesariamente avería. |
| No aparecen menús | El firmware actual muestra páginas rotativas; B2 está deshabilitado. |
| Cambios set desaparecen al reiniciar | Arranque fuerza períodos SD/HTTP, autostart y opciones de estación. |
| PM queda fijo tras desconexión | Falta de caducidad Plantower; inspeccionar llegada de tramas por UART. |

## 14. Fuentes y límites de implementación

Cada sección se apoya en estas rutas y funciones del repositorio:

| Secciones | Fuente de verdad |
|---|---|
| 1–3, tiempos y arranque | HIRI_STATIC_CALEFACTOR_0_1.ino: setup, loop, enforceStationStartupConfig, beginConnectionAttempt, connectionFailed. |
| Componentes/pines | config.h y declaraciones de objetos de ambos sketches. |
| 4, sensores | PMSandSensorEXTRA.ino: readPMS; Calefactor_V2_0_0.ino: readSensors. |
| 5, pantalla/botón | ui.ino: renderDisplay, drawStationView, drawHeader, ui_btn1_click, ui_btn2_click. |
| 6, comandos | serial_commands.ino: processSerialCommand; config.ino: loadConfig, configSetDefaults. |
| 7–8, protocolo/control | heater_serial.ino y Calefactor_V2_0_0.ino. |
| 9, API | HIRI_STATIC_CALEFACTOR_0_1.ino: buildCurrentMeasurementValues/getIdsSensores; http.ino. |
| 10–11, SD y hora | sd_card.ino, helpers.ino, rtc.ino y wifi.ino. |
| 12, GNSS | gps.ino; rutas realmente llamadas desde setup/loop. |

En la revisión documental se corrigen comentarios contradictorios, sin cambiar instrucciones ejecutables. Permanecen las limitaciones descritas: reinicio HIRI configurado pero sin ejecución; opciones OLED sin aplicación; navegación heredada inalcanzable; valores PMS sin caducidad; validación numérica serial limitada; sincronización RTC automática sin llamada; XTRA sin refresco invocado. No se presentan esas funciones declaradas como prestaciones operativas.

Creación original del proyecto: Alejandro Rebolledo. Este manual, el software y el firmware propios: GPL-3.0-only, exclusivamente versión 3. Diseños propios y documentación de hardware en PCB/: CERN-OHL-S-2.0. Los componentes de terceros conservan sus términos; diy-modules de Miroslaw Brudnowski conserva CC BY 4.0. Consultar LICENSE, LICENSES/ y DISCLAIMER.md. Este manual acompaña el código revisado y debe actualizarse cuando cambien sus constantes, rutas o esquemas de datos.
