# Revisión técnica histórica: firmware original con DHT22

> Este documento describe exclusivamente el firmware original respaldado en `codigo_original/Calefactor_bmp_dht22_serialcheck_v3_original.ino`. El firmware vigente usa SHT40 y está en `Calefactor_V2_0_0/Calefactor_V2_0_0.ino`; no utiliza DHT22.

Archivo analizado: `Calefactor_bmp_dht22_serialcheck_v3.ino`

## Hardware asumido

- MCU: ESP8266, por el uso de pines `D0`...`D8` y `ESP.wdtFeed()`.
- Sensor ambiental BME280 por I2C, direccion `0x76`.
- Sensor DHT22 en `D7`.
- Termocupla con MAX6675 por SPI en `D5`, `D6`, `D8`.
- Rele del calefactor en `D0`.
- Comunicacion serial secundaria por `SoftwareSerial`:
  - `externo_RX = D3`
  - `externo_TX = D4`

## Variables principales

- `tempsens`: temperatura recibida desde el puerto serial secundario. Parte en `16`.
- `tempBMP`: temperatura del BME280.
- `humBMP`: humedad relativa del BME280.
- `dhttemp`: temperatura del DHT22.
- `dhthum`: humedad relativa del DHT22.
- `tempThermo`: temperatura del calefactor medida por MAX6675.
- `minimo = 15`: umbral inferior de temperatura.
- `maxcontrol = 70`: temperatura maxima normal del calefactor.
- `maxemergencia = 80`: umbral de emergencia.

## Flujo de operacion

1. Inicializa watchdog, serial principal, serial externo, SPI, rele, MAX6675, BME280 y DHT22.
2. Si falla BME280, marca `statusBME = false` y despues reporta `999` en temperatura/humedad BME.
3. Cada vez que llega una linea por `externo`, valida checksum y actualiza `tempsens`.
4. Cada 280 ms lee:
   - MAX6675.
   - BME280, si esta disponible.
   - DHT22.
5. Imprime una linea CSV:

```text
tempsens,dhttemp,tempBMP,dhthum,humBMP,tempThermo
```

6. Enciende el rele si alguna de estas temperaturas esta bajo `15 C`:
   - `tempBMP`
   - `tempsens`
   - `dhttemp`
7. Apaga el rele si `tempThermo >= 70 C` o si `tempThermo == 0`.
8. Entra a modo emergencia si `tempThermo >= 80 C`; sale cuando baja de `80 C`.
9. Reinicia el ESP cada 4 horas con `Ticker`.

## Punto critico: humedad medida pero no usada

El codigo mide humedad:

```cpp
humBMP = bme.readHumidity();
dht.humidity().getEvent(&event);
dhthum = event.relative_humidity;
```

Pero la humedad solo se imprime/transmite. No aparece en la condicion de control del rele:

```cpp
if (tempBMP < minimo or tempsens < minimo or dhttemp < minimo)
```

Por lo tanto, si la humedad relativa sube a 70%, 80% o 90%, el calefactor no necesariamente encendera. Solo encendera si alguna temperatura baja de 15 C.

## Riesgos y observaciones

### 1. El calefactor no controla directamente la causa del sesgo PM2.5

La literatura indica que el problema de los sensores opticos PM2.5 no es solo baja temperatura, sino humedad relativa alta, condensacion y crecimiento higroscopico. El codigo actual no usa `dhthum` ni `humBMP` para activar el calefactor.

### 2. Lecturas invalidas del DHT22 no se filtran

Si el DHT22 entrega `NaN`, la comparacion `dhttemp < minimo` no se comporta como una lectura valida. Conviene validar con `isnan()`.

### 3. Uso de `tempBMP = 999` si falla BME280

Esto evita que el BME280 active el calefactor, pero tambien oculta la falla dentro de la logica de control. Para seguridad, una falla de sensor deberia quedar separada como estado de error.

### 4. `tempThermo == 0` apaga el rele

Puede ser una proteccion contra error de termocupla. Es correcto como idea, pero deberia documentarse y reportarse como falla. Una termocupla desconectada podria no entregar exactamente cero en todos los casos.

### 5. Modo emergencia con `while`

Durante emergencia el codigo queda bloqueado en un `while`. Sigue leyendo la termocupla y enviando datos, pero no procesa nuevas entradas seriales ni actualiza DHT/BME. Para seguridad esta bien apagar el rele, pero para telemetria conviene evitar bloqueo total.

### 6. Intervalo de lectura DHT22 demasiado corto

El loop intenta leer DHT22 cada 280 ms. El DHT22 normalmente requiere intervalos mas largos entre lecturas, tipicamente alrededor de 2 s. Leerlo tan rapido puede aumentar lecturas invalidas o repetidas.

### 7. `restartInterval` como `int`

`4 * 60 * 60 * 1000` cabe en un entero de 32 bits, pero no en 16 bits. En ESP8266 suele funcionar porque `int` es 32 bits, pero es mejor declararlo como `const unsigned long`.

### 8. `String` en ESP8266

El uso repetido de `String` en loops largos puede fragmentar memoria en algunos escenarios. Si el sistema debe operar meses, conviene evaluar buffers `char[]` o reducir concatenaciones.

## Interpretacion funcional

El programa actual puede describirse como:

> Controlador de calefactor por umbral de temperatura minima, con proteccion por termocupla y telemetria ambiental de temperatura/humedad.

No deberia describirse como:

> Controlador de humedad para estabilizar lecturas PM2.5.

Para que esa segunda frase sea correcta, la humedad debe entrar en la regla de control o al menos en un algoritmo de compensacion/documentacion experimental.
