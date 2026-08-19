# HIRI AUCA con calefactor para Plantower

Proyecto de firmware y hardware para una estación ambiental HIRI AUCA basada en
ESP32, un sensor Plantower y un calefactor controlado por ESP8266. El calefactor
mide temperatura y humedad, protege el elemento térmico mediante termocupla y
entrega sus datos a la estación por un enlace serial dedicado.

## Estado vigente

| Componente | Archivo principal | Plataforma | Versión |
|---|---|---|---|
| Calefactor | `Calefactor_V2_0_0/Calefactor_V2_0_0.ino` | ESP8266 / NodeMCU | Firmware `1.0.0` |
| Estación HIRI | `HIRI_STATIC_CALEFACTOR_0_1/HIRI_STATIC_CALEFACTOR_0_1.ino` | ESP32 Dev Module | `CAL V0.0.1` |
| PCB calefactor | `PCB/calefactor.sch` y `PCB/calefactor.brd` | Autodesk EAGLE 9.6.2 | Diseño fuente |
| PCB HIRI ESP32/SIM7600 | `PCB/pcbesp32sim7600/Board_760032v2.sch` y `PCB/pcbesp32sim7600/Board_760032v2.brd` | Autodesk EAGLE 9.6.2 | Diseño fuente |
| PCB resistivo | `PCB/RESIS-CALOR.brd` | Autodesk EAGLE 9.6.2 | Diseño fuente |

El firmware histórico con DHT22 se conserva sólo como referencia. El calefactor
vigente usa SHT40; no requiere DHT22.

## Funciones principales

### Calefactor

- SHT40 para temperatura y humedad.
- BME280 como medición interna/respaldo.
- MAX6675 y termocupla para protección térmica.
- Relé en `D0`.
- Control por humedad con histéresis: enciende desde `65 % RH` y libera bajo
  `60 % RH`.
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
- Consulta opcional de versión del calefactor, sin bloquear calefactores
  antiguos.

## Protocolo serial HIRI-calefactor

Las tramas son ASCII a `9600 8N1` y terminan en `*HH`, donde `HH` es la suma
de comprobación hexadecimal de 8 bits del texto anterior al asterisco.

El identificador vigente del calefactor es `CALEF`; HIRI también acepta `CAL`
para equipos anteriores.

```text
HIRI,1,HELLO*HH
CALEF,1,HELLO,1.0.0*HH

HIRI,1,VERSION*HH
CALEF,1,VERSION,1.0.0*HH

HIRI,1,TEMP,valor*HH
HIRI,1,CAL,segundos*HH

CALEF,1,DATA,tempExt,tempSHT,tempBME,humSHT,humBME,tempTermocupla,
             rele,porHumedad,porTemperatura,emergencia*HH
```

La consulta `VERSION` es opcional. HIRI la envía al arrancar y luego cada 30
segundos durante un máximo de 3 minutos. La ausencia de respuesta no detiene el
arranque, la adquisición, el guardado ni el uso del calefactor.

## Estructura

- `Calefactor_V2_0_0/`: firmware vigente del calefactor.
- `HIRI_STATIC_CALEFACTOR_0_1/`: firmware vigente de la estación.
- `PCB/`: esquemáticos y placas EAGLE del calefactor, del elemento resistivo y
  de la estación HIRI con ESP32/SIM7600; consultar su README antes de fabricar.
- `documentacion/`: documentación del calefactor, cambios y antecedentes.
- `codigo_original/`: respaldos históricos; no son el firmware recomendado.

## Compilación

```powershell
arduino-cli compile --fqbn esp8266:esp8266:nodemcuv2 Calefactor_V2_0_0
arduino-cli compile --fqbn esp32:esp32:esp32 HIRI_STATIC_CALEFACTOR_0_1
```

Se requieren las librerías Arduino indicadas por los `#include` de cada
sketch. Antes de cargar, sustituir el puerto de ejemplo por el puerto real.

## Documentación

- [Documentación del calefactor](documentacion/README.md)
- [Documentación de HIRI](HIRI_STATIC_CALEFACTOR_0_1/README.md)
- [PCB y advertencias de fabricación](PCB/README.md)
- [Licencia y atribución](LICENSE.md)
- [Descargo de responsabilidad](DISCLAIMER.md)

## Autor y licencia

Creación original: **Alejandro Rebolledo**

Contacto: **arebolledo@udd.cl**

Salvo los componentes de terceros identificados por separado, el firmware, la
documentación y los diseños PCB originales de este repositorio se ofrecen bajo
[Creative Commons Attribution-NonCommercial 4.0 International](LICENSE.md).
Se permite compartir y adaptar con atribución, únicamente para fines no
comerciales. El uso comercial requiere autorización separada del autor.

Este proyecto se entrega tal cual y no constituye un producto certificado.
Revise el [descargo de responsabilidad](DISCLAIMER.md) antes de construir,
energizar o desplegar el equipo.
