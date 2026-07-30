# Recomendaciones y contexto histórico

> Nota de vigencia: el DHT22 fue retirado. El firmware activo usa SHT40 en `Calefactor_V2_0_0/Calefactor_V2_0_0.ino`. Las referencias siguientes a `dhthum` y `dhttemp` son ejemplos del firmware original y se conservan como antecedente.

## Diagnostico

El firmware original controlaba temperatura mínima y protección por sobretemperatura, pero no cerraba el lazo con humedad. El firmware vigente ya incorpora ese control mediante SHT40.

## Recomendacion principal

Usar una condición de control basada en humedad relativa, idealmente medida cerca de la entrada del Plantower y después del calefactor. En `Calefactor V2`, firmware `1.0.0`, se implementó con SHT40 e histéresis `65% RH ON / 60% RH OFF`.

Ejemplo histórico de regla simple con DHT22 (no usar en el firmware vigente):

```cpp
const float humedadObjetivo = 60.0;
const float humedadAlta = 70.0;

bool humedadAltaDetectada = (dhthum > humedadAlta || humBMP > humedadAlta);
bool temperaturaBaja = (tempBMP < minimo || tempsens < minimo || dhttemp < minimo);
bool calefactorSeguro = (tempThermo > 0 && tempThermo < maxcontrol);

if ((temperaturaBaja || humedadAltaDetectada) && calefactorSeguro) {
  digitalWrite(relay, HIGH);
} else {
  digitalWrite(relay, LOW);
}
```

Esta regla no debe copiarse sin probar: hay que decidir que sensor de humedad representa mejor el flujo que entra al Plantower.

## Mejor opcion de control

Usar dos criterios:

1. Seguridad termica del calefactor:
   - Apagar si `tempThermo >= maxcontrol`.
   - Emergencia si `tempThermo >= maxemergencia`.
   - Apagar ante falla de termocupla.

2. Calidad de medicion PM2.5:
   - Encender calefactor si RH antes del Plantower supera 65-70%.
   - Apagar o modular cuando RH baje a 55-60%.
   - Usar histeresis para evitar prendido/apagado rapido.

Ejemplo de histéresis implementada en el firmware 1.0.0:

```cpp
const float rhOn = 65.0;
const float rhOff = 60.0;
bool heaterByHumidity = false;

if (rhEntradaPlantower >= rhOn) {
  heaterByHumidity = true;
}
if (rhEntradaPlantower <= rhOff) {
  heaterByHumidity = false;
}
```

## Medicion recomendada

Para validar el sistema, registrar como minimo:

- PM1.0, PM2.5, PM10 del Plantower.
- Temperatura y humedad antes del calefactor.
- Temperatura y humedad despues del calefactor o cerca de la entrada del Plantower.
- Temperatura de la termocupla del calefactor.
- Estado del rele.
- Fecha/hora.

Con esos datos se puede comprobar si el calefactor realmente baja la humedad relativa que ve el Plantower.

## Punto de rocio

Una mejora es calcular punto de rocio. Si la temperatura del flujo o de superficies internas se acerca al punto de rocio, aumenta el riesgo de condensacion.

El control puede buscar:

- Mantener la temperatura del aire que entra al Plantower varios grados sobre el punto de rocio.
- Evitar exceder la temperatura maxima recomendada del sensor.

## Cambios de codigo recomendados

- DHT22 reemplazado por SHT40 en `Calefactor V2`, firmware `1.0.0`.
- Validar `isnan()` en lecturas ambientales. Implementado en el firmware `1.0.0`.
- Separar estado de sensores fallidos de valores fisicos como `999`.
- Incluir estado del rele en la salida de datos. Implementado en el firmware `1.0.0`.
- Incluir banderas de error: BME ausente, SHT40 inválido y termocupla inválida.
- Evitar `while (emergencyMode)` bloqueante; usar un estado de emergencia no bloqueante. Implementado en el firmware `1.0.0`.
- Cambiar `restartInterval` a `const unsigned long`. Implementado en el firmware `1.0.0`.
- Corregir comentarios con caracteres mal codificados.

## Decision tecnica

Para un documento academico o informe, la frase correcta seria:

> El prototipo mide humedad relativa, pero la version actual del firmware no la utiliza para controlar el calefactor. El control se basa en umbrales de temperatura y proteccion por termocupla. Dado que la literatura identifica la humedad relativa como un factor relevante en sensores opticos PM2.5 tipo Plantower, se recomienda incorporar una regla de control por RH o punto de rocio y validar experimentalmente la reduccion de RH en la entrada del sensor.
