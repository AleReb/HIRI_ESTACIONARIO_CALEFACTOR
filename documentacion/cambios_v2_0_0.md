# Cambios implementados en Calefactor V2 — firmware 1.0.0

## Control de version del firmware

La generacion de hardware/proyecto sigue siendo `Calefactor V2`. Su primera
version completa de firmware se identifica semanticamente como `1.0.0` mediante
la constante `CAL_FIRMWARE_VERSION`.

HIRI puede consultarla por el enlace serial de 9600 baudios:

```text
HIRI,1,VERSION*HH
CALEF,1,VERSION,1.0.0*HH
```

`HH` es la suma de comprobacion hexadecimal ya usada por el protocolo. La trama
`HELLO` tambien incluye la version para conservar la deteccion existente.

Archivo nuevo:

```text
Calefactor_V2_0_0/Calefactor_V2_0_0.ino
```

Respaldo del codigo original:

```text
codigo_original/Calefactor_bmp_dht22_serialcheck_v3_original.ino
```

## Cambio de sensor

Se elimino el uso de DHT22:

```cpp
#include <DHT.h>
#include <DHT_U.h>
```

Se agrego SHT40 por I2C usando la libreria de Adafruit:

```cpp
#include <Adafruit_SHT4x.h>
Adafruit_SHT4x sht4 = Adafruit_SHT4x();
```

La lectura principal de humedad para el control es `shtHum`. Si el SHT40 falla, el codigo puede usar `humBME` como respaldo.

## Nueva regla de activacion por humedad

La literatura reporta que el efecto higroscopico en PMS5003 puede comenzar cerca de `65-70% RH`. Por eso la version V2 usa:

```cpp
const float RH_ON = 65.0;
const float RH_OFF = 60.0;
const float RH_ALTA = 70.0;
```

La logica queda:

- Si `RH >= 65%`, el calefactor queda habilitado por humedad.
- Si `RH <= 60%`, se deshabilita el control por humedad.
- Si `RH >= 70%`, se imprime un aviso porque ya esta en zona alta de posible sesgo higroscopico.

La histeresis evita que el rele prenda y apague continuamente cerca de un solo umbral.

## Proteccion termica

Se mantiene la termocupla MAX6675 como proteccion del calefactor:

- Apaga el rele si la termocupla es invalida o entrega `0`.
- Apaga el rele si `tempThermo >= 70 C`.
- Entra en emergencia si `tempThermo >= 80 C`.
- Sale de emergencia solo cuando `tempThermo < 70 C`.

## Control combinado

El rele se activa si hay condicion de humedad o condicion de temperatura baja:

```cpp
setRelay(heaterByHumidity || heaterByTemperature);
```

Pero esa activacion solo ocurre si la termocupla esta en rango seguro.

## Salida de datos del firmware 1.0.0

La nueva salida CSV es:

```text
tempsens,shtTemp,tempBME,shtHum,humBME,tempThermo,relayOn,heaterByHumidity,heaterByTemperature,emergencyMode
```

Donde:

- `tempsens`: temperatura recibida por serial externo.
- `shtTemp`: temperatura del SHT40.
- `tempBME`: temperatura del BME280.
- `shtHum`: humedad relativa del SHT40.
- `humBME`: humedad relativa del BME280.
- `tempThermo`: temperatura de la termocupla MAX6675.
- `relayOn`: `1` si el rele esta encendido, `0` si esta apagado.
- `heaterByHumidity`: `1` si la humedad esta pidiendo calefaccion.
- `heaterByTemperature`: `1` si alguna temperatura baja esta pidiendo calefaccion.
- `emergencyMode`: `1` si esta en modo emergencia.

## Librerias requeridas

Ademas de las librerias ya usadas por el proyecto, el firmware 1.0.0 requiere:

- `Adafruit SHT4x Library`
- `Adafruit BusIO`
- `Adafruit Unified Sensor`

## Recomendacion de prueba

Antes de conectar el Plantower a una campana o flujo real, probar con monitor serial:

1. Confirmar que `shtHum` y `shtTemp` entregan valores validos.
2. Subir RH sobre `65%` y verificar `heaterByHumidity = 1` y `relayOn = 1`.
3. Bajar RH bajo `60%` y verificar `heaterByHumidity = 0`.
4. Calentar termocupla sobre `70 C` y verificar que `relayOn = 0`.
5. Confirmar que el rele no enciende si la termocupla falla o marca `0`.
