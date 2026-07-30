# Documentación vigente del calefactor

Fuente de verdad:
`../Calefactor_V2_0_0/Calefactor_V2_0_0.ino`.

## Identificación

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
- Humedad: activa desde `65 % RH`, desactiva bajo `60 % RH`.
- Temperatura mínima de referencia: `15 °C`.
- Corte de relé por termocupla inválida, igual a cero o desde `70 °C`.
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

La consulta de versión no es obligatoria para operar. HIRI puede usar
calefactores antiguos que sólo entreguen `HELLO` o `DATA`.

## Documentos

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
