# Guía de mantenimiento

## Fuente de verdad

- Sketch principal: `HIRI_STATIC_CALEFACTOR_0_1.ino`
- Versión actual: variable `VERSION`, hoy `CAL V0.0.1`
- ID de estación: `DEVICE_ID_STR`, hoy `4`
- Protocolo de calefactor: `heater_serial.ino`

El directorio `build/` contiene artefactos y copias generadas; no debe usarse
como fuente para editar ni documentar.

## Regla de cambios

Todo cambio funcional debe:

1. actualizar `VERSION` según la política de releases;
2. registrarse en `documentacion/CAMBIOS.md`;
3. actualizar README y manuales afectados;
4. compilar con Arduino CLI;
5. validarse en hardware antes de declararse liberado.

## Elementos que deben mantenerse sincronizados

| Cambio | Documentos |
|---|---|
| Pines o hardware | README, manual técnico y PCB README |
| OLED o botones | manual de usuario y `menu_structure.md` |
| Protocolo del calefactor | ambos README y manual técnico |
| CSV o HTTP | README y manual técnico |
| Comandos | manuales técnico y de usuario |
| Defaults | README y manual técnico |
| Licencia/autoría | README, `LICENSE.md` y `DISCLAIMER.md` |

## Compilación

Desde la carpeta superior:

```powershell
arduino-cli compile --fqbn esp32:esp32:esp32 HIRI_STATIC_CALEFACTOR_0_1
```

Calefactor:

```powershell
arduino-cli compile --fqbn esp8266:esp8266:nodemcuv2 Calefactor_V2_0_0
```

## Checklist mínimo

- Compilan ambos sketches.
- HIRI acepta IDs `CALEF` y `CAL`.
- La falta de respuesta `VERSION` no bloquea la operación.
- Plantower y calefactor entregan datos o fallan de forma controlada.
- El CSV coincide con la cabecera documentada.
- HTTP contiene exactamente 16 valores.
- OLED y botones coinciden con `menu_structure.md`.
- SD, RTC, SIM7600, watchdog y reinicio programado funcionan.
- PCB se revisa mediante ERC/DRC antes de fabricación.

## Autoría y redistribución

Conservar la atribución a Alejandro Rebolledo (`arebolledo@udd.cl`), el aviso
CC BY-NC 4.0, la indicación de cambios y el descargo de responsabilidad.
