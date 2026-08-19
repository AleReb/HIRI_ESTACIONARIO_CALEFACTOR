# Diseños PCB

Esta carpeta contiene fuentes de Autodesk EAGLE 9.6.2:

| Archivo | Contenido observado | Dimensión aproximada |
|---|---|---:|
| `calefactor.sch` | Esquemático del controlador del calefactor, 1 hoja y 11 componentes | No aplica |
| `calefactor.brd` | Placa del controlador con Wemos D1 Mini Pro, termocupla, dos conectores I2C, relé y serial | 48,30 × 78,09 mm |
| `RESIS-CALOR.brd` | Placa alargada del elemento resistivo/calefactor | 120,40 × 24,00 mm |
| `pcbesp32sim7600/Board_760032v2.sch` | Esquemático de la estación HIRI para ESP32 y SIM7600 | No aplica |
| `pcbesp32sim7600/Board_760032v2.brd` | Placa de la estación HIRI correspondiente al firmware `HIRI_STATIC_CALEFACTOR_0_1` | Fuente EAGLE |

## Correspondencia con el firmware

El firmware vigente usa:

- Wemos/ESP8266;
- MAX6675 y termocupla;
- BME280 y SHT40 por I2C;
- relé;
- enlace serial con HIRI.

El esquemático y la placa conservan un conector rotulado `DHT`. Es una herencia
del diseño anterior y no significa que el firmware vigente utilice DHT22. El
SHT40 vigente se conecta por I2C. Antes de fabricar se debe comprobar en EAGLE
la asignación real de cada conector, señal y pin contra
`Calefactor_V2_0_0/Calefactor_V2_0_0.ino`.

El diseño `pcbesp32sim7600/Board_760032v2` corresponde al firmware de la
estación ubicado en
`HIRI_STATIC_CALEFACTOR_0_1/HIRI_STATIC_CALEFACTOR_0_1.ino`. Incluye la placa y
el esquemático para la electrónica basada en ESP32 y SIM7600, además de las
conexiones de los periféricos de HIRI. Antes de fabricar, también se deben
comparar las señales y los pines del diseño contra ese sketch.

## Estado de fabricación

No se incluyen Gerbers, archivos de perforado, BOM aprobada ni resultados de
DRC/ERC. Estos archivos deben considerarse fuentes editables, no un paquete de
fabricación liberado.

Antes de fabricar:

1. Ejecutar ERC en `calefactor.sch`.
2. Confirmar que `calefactor.brd` corresponde al esquemático vigente.
3. Ejecutar DRC con las reglas del fabricante.
4. Revisar anchos de pista, corriente, separación y aislamiento del circuito
   térmico y del relé.
5. Verificar huellas, polaridades, conectores y dimensiones mecánicas.
6. Revisar `RESIS-CALOR.brd` mediante cálculo térmico y eléctrico independiente.
7. Generar y revisar Gerbers y perforaciones antes de enviar a producción.
8. Ejecutar ERC y DRC sobre `pcbesp32sim7600/Board_760032v2.sch` y
   `pcbesp32sim7600/Board_760032v2.brd`, y verificar su correspondencia con el
   firmware HIRI vigente.

Consulte [el descargo de responsabilidad](../DISCLAIMER.md). Los diseños
originales se distribuyen bajo [CC BY-NC 4.0](../LICENSE.md).
