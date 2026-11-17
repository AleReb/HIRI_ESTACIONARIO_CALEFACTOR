# Documentación vigente de HIRI y calefactor V2

El [manual técnico de consulta](manual_tecnico_hiri_calefactor_v2.md), disponible
[también en PDF](manual_tecnico_hiri_calefactor_v2.pdf), toma la estación HIRI como
base y explica su interacción con el módulo calefactor.

## Fuentes de verdad

| Firmware | Sketch | Versión |
|---|---|---|
| HIRI ESP32 | [HIRI_STATIC_CALEFACTOR_0_1.ino](../HIRI_STATIC_CALEFACTOR_0_1/HIRI_STATIC_CALEFACTOR_0_1.ino) y sus pestañas | CAL V1.0.2, estación 2 |
| Calefactor ESP8266 | [Calefactor_V2_0_0.ino](../Calefactor_V2_0_0/Calefactor_V2_0_0.ino) | 1.0.0 |

## Resumen de HIRI

- SD cada 3 minutos y HTTP cada 5 minutos; primer intento HTTP al entrar al loop.
- Botón B1: HTTP, SD del mismo payload y solicitud de calefacción de 10 s.
- OLED rota cada 10 s; B2 deshabilitado, GNSS apagado por defecto.
- Cinco campos del calefactor a -1 cuando no hay DATA válido.
- Tras tres fallos de conexión entra en modo SD, sin reconexión automática.
- CSV diario de 18 columnas con hora RTC; API recibe 16 valores sin timestamp.
- Reinicio HIRI de 3 h y apagado OLED son opciones sin ejecución activa.
- Menús heredados y WiFi SD no son accesibles desde la navegación normal.

El manual detalla componentes, archivos, comandos, calidad de datos, protocolos,
fechas y funcionamiento con y sin red/calefactor. Las pruebas son documentales;
no se ha validado el montaje en banco.

## Identificación del calefactor

- Generación del proyecto: Calefactor V2.
- Versión de firmware: `1.0.0`.
- Plataforma: ESP8266 / NodeMCU.
- ID de protocolo vigente: `CALEF`.
- ID histórico aceptado por HIRI: `CAL`.

## Hardware utilizado por el firmware

| Función | Componente o pin |
|---|---|
| Temperatura/humedad externa | SHT40 por I2C |
| Temperatura/humedad interna | BME280 en dirección `0x76` |
| Protección térmica | MAX6675: SO `D6`, SCK `D5`, CS `D8` |
| Salida del relé | `D0` |
| Serial hacia HIRI | RX `D3`, TX `D4`, 9600 8N1 |

El DHT22 sólo pertenece al firmware histórico.

## Control implementado

- Muestreo cada 1 segundo.
- Humedad: activa desde `65 % RH`, desactiva a `60 % RH` o menos.
- Temperatura mínima de referencia: `15 °C`.
- Corte de relé por termocupla no finita, menor o igual a cero, o desde `70 °C`.
- Emergencia desde `80 °C`; recuperación bajo `70 °C`.
- Prueba manual serial de 1 a 300 segundos con las mismas protecciones.
- Reinicio automático cada 4 horas.

## Protocolo

Las tramas usan checksum aditivo hexadecimal `*HH`.

```text
HIRI,1,HELLO*HH
HIRI,1,VERSION*HH
HIRI,1,TEMP,valor*HH
HIRI,1,CAL,segundos*HH

CALEF,1,HELLO,1.0.0*HH
CALEF,1,VERSION,1.0.0*HH
CALEF,1,DATA,...*HH
```

La consulta de versión no es obligatoria para operar. HIRI acepta `HELLO`
sin versión y los IDs `CAL`/`CALEF`, siempre con el formato y checksum del
protocolo actual. Esto no garantiza compatibilidad con formatos anteriores
arbitrarios. Guardar/enviar lecturas requiere un DATA no caducado.

## Documentos

- [Manual técnico de HIRI y calefactor V2 (Markdown)](manual_tecnico_hiri_calefactor_v2.md)
- [Manual técnico de HIRI y calefactor V2 (PDF)](manual_tecnico_hiri_calefactor_v2.pdf)
- [Verificación de cambios exclusivos en comentarios](verificacion_comentarios.json)
- [README de la estación HIRI](../HIRI_STATIC_CALEFACTOR_0_1/README.md)
- [Cambios V2](cambios_v2_0_0.md)
- [Revisión histórica del código DHT22](revision_codigo.md)
- [Humedad y medición PM2.5](humedad_pm25_literatura.md)
- [Recomendaciones de diseño](recomendaciones.md)
- [PCB](../PCB/README.md)

Los documentos históricos se conservan como antecedentes y están marcados como
tales. Para comportamiento ejecutable prevalece siempre el sketch vigente.

## Seguridad y licencia

Creación original: Alejandro Rebolledo, arebolledo@udd.cl.

Licencia: [CC BY-NC 4.0](../LICENSE.md).

Lea el [descargo de responsabilidad](../DISCLAIMER.md) antes de energizar el
calefactor o fabricar los PCB.
