# Interfaz OLED vigente

Este documento describe el flujo que realmente ejecuta `ui.ino`.

## Vista normal

`renderDisplay()` llama directamente a `drawStationView()`. La vista rota cada
10 segundos.

Con GNSS apagado:

1. Plantower.
2. Calefactor.
3. Estado de la estación.

Con GNSS encendido:

1. GPS y hora.
2. Plantower.
3. Calefactor.
4. Estado de la estación.

## Botones activos

### BTN1

En la vista normal realiza una prueba manual:

- envío HTTP;
- guardado SD del payload;
- solicitud de calefacción por 10 segundos.

También atiende cancelación o salida en algunos estados transitorios.

### BTN2

En la vista normal avanza a la siguiente pantalla. La configuración vigente usa
`BUTTON_PIN_2=-1`, por lo que no hay un segundo botón físico activo.

## Estados transitorios

El enum `DisplayState` conserva:

```text
DISP_NORMAL
DISP_SD_SAVED
DISP_MESSAGE
DISP_PROMPT
DISP_NETWORK
DISP_RTC
DISP_STORAGE
DISP_GPS
DISP_WIFI
```

Las rutinas de render para esos estados permanecen disponibles, aunque varias
se alcanzan sólo desde código o funciones heredadas.

## Menús retenidos pero no activos

`ui.ino` todavía declara arreglos para:

- menú principal;
- Mensajes;
- Configuración;
- Información.

Sin embargo, el render normal no dibuja esos menús y los manejadores BTN1/BTN2
retornan desde la lógica de vista de estación antes de ejecutar la navegación
heredada. Por tanto, esos arreglos no representan una interfaz accesible en el
firmware actual.

Si se reactiva el menú, se deben revisar los retornos anticipados de
`ui_btn1_click()`, `ui_btn2_click()` y `renderDisplay()`, asignar un GPIO real a
BTN2 y actualizar este documento y el manual de usuario.
