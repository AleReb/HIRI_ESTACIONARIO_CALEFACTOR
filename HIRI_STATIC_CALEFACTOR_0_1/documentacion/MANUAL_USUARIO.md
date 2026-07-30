# Manual de usuario — HIRI AUCA 4

Firmware: `CAL V0.0.1`

Equipo: `HIRI-AUCA-4`

## Encendido

1. Compruebe cableado, antenas, termocupla, relé, Plantower y tarjeta SD.
2. Energice la estación y el calefactor.
3. La OLED muestra la animación y luego revisa:
   `OLED`, `SD`, `RTC`, `PLANTOWER` y `CALEFACTOR`.
4. La fila del calefactor confirma que el puerto serial fue iniciado. Si el
   equipo soporta consulta de versión, aparecerá su número.
5. La falta de número de versión no es un error: los calefactores antiguos
   siguen operando mediante sus datos normales.
6. La estación inicia automáticamente guardado SD y telemetría.

## Pantallas

Sin GNSS, la OLED rota entre:

- Plantower: PM1.0, PM2.5, PM10 y T/RH;
- calefactor: versión cuando existe, SHT40, BME280, termocupla y relé;
- sistema: ID, versión HIRI, batería, señal, contadores y modo.

Con GNSS habilitado se agrega GPS y hora.

## Botones

- BTN1 (GPIO39): ejecuta una acción manual de prueba:
  1. intenta un envío HTTP;
  2. guarda el mismo payload en SD;
  3. solicita 10 segundos de calefacción.
- BTN2 está deshabilitado en el firmware actual (`BUTTON_PIN_2=-1`).

La prueba térmica sólo es una solicitud: el calefactor la rechaza o la corta si
la termocupla es inválida, llega a 70 °C o existe una emergencia.

## Operación automática

```text
Guardado SD: cada 3 minutos
Envío HTTP: cada 5 minutos
Reinicio programado: cada 3 horas
GNSS: apagado por defecto
```

La estación consulta opcionalmente el firmware del calefactor durante los
primeros 3 minutos. Esas consultas no interrumpen ninguna otra función.

## Archivos SD

Datos diarios:

```text
/HIRI_AUCA_4_DD_MM_AAAA.csv
```

Logs:

```text
/errors_h4.csv
/failed_h4.csv
```

El CSV contiene PM, temperaturas, humedades, estado del relé, señal, resultado
HTTP, GNSS y batería.

## Consola USB

Conectar a 115200 baudios y terminación de línea `\n`.

Comandos útiles:

```text
help
cal 10
sdinfo
sdlist
netinfo
csq
sysinfo
start
stop
config
```

Use `cal N` sólo durante una prueba supervisada; `N` debe estar entre 1 y 300
segundos.

## Solución de problemas

### El calefactor no muestra versión

Puede ser un firmware antiguo. Si llegan mediciones y la pantalla indica datos
del calefactor, puede usarse normalmente. HIRI deja de consultar la versión
después de 3 minutos.

### No llegan datos del calefactor

- Verifique cruce TX/RX: calefactor TX hacia ESP32 RX19 y calefactor RX desde
  ESP32 TX23.
- Confirme 9600 baudios y tierra común.
- Revise las tramas `[CALEFACTOR]` en el monitor serial.
- Compruebe alimentación del ESP8266.

### No guarda en SD

Ejecute `sdinfo`, revise la tarjeta y compruebe que exista el archivo diario.

### No transmite

Ejecute `csq` y `netinfo`; revise antena, SIM, cobertura y
`/failed_h4.csv`.

### Lectura térmica inválida

No fuerce el relé. Revise MAX6675, termocupla, polaridad y conexión antes de
continuar.

## Seguridad y licencia

No deje una prueba del calefactor sin supervisión. El software no sustituye
protecciones térmicas y eléctricas independientes.

Autor original: Alejandro Rebolledo (`arebolledo@udd.cl`).

Licencia: [CC BY-NC 4.0](../../LICENSE.md).

Lea el [descargo de responsabilidad](../../DISCLAIMER.md).
