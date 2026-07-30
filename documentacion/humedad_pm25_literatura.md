# Humedad relativa y sensores PM2.5 tipo Plantower

## Contexto

Los sensores Plantower/PMS5003 estiman PM por dispersion de luz. No pesan directamente el material particulado: convierten senales opticas en conteos y concentraciones mediante algoritmos internos. Por eso sus lecturas dependen de propiedades opticas, tamano de particula, composicion y humedad.

## Que dice la literatura

### 1. Plantower admite operacion hasta alta humedad, pero advierte condiciones con niebla/agua

El manual del PMS5003 indica rango de humedad de trabajo de `0~99%`. Tambien advierte que, en condiciones como niebla de agua, bano/termas u outdoor, se debe agregar proteccion. Esto significa que el sensor puede operar en aire humedo, pero no implica que la medicion sea exacta en alta humedad o con condensacion/aerosol de agua.

Fuente: Plantower PMS5003 series data manual.

### 2. La humedad relativa puede producir sobreestimacion

Una revision/laboratorio sobre sensores de bajo costo indica que, en humedad relativa alta, el efecto higroscopico puede sobreestimar la salida del sensor. Tambien cita que para Plantower PMS5003 el efecto empieza a observarse sobre aproximadamente 65-70% RH.

Fuente: *Calibration of Low-cost Sensors for Measurement of Indoor Particulate Matter Concentrations via Laboratory/Field Evaluation*, AAQR.

### 3. El mecanismo fisico es el crecimiento higroscopico

Cuando la humedad relativa sube, algunas particulas absorben agua. Eso cambia su tamano y su indice de refraccion efectivo. Como el sensor optico mide luz dispersada, puede interpretar ese cambio como mas particulas o mas masa PM2.5, aunque parte del aumento sea agua.

Fuente: Patel et al. 2024, *Towards a hygroscopic growth calibration for low-cost PM2.5 sensors*, Atmospheric Measurement Techniques.

### 4. La composicion del aerosol importa

El efecto de humedad no es igual para todos los aerosoles. Particulas con sulfatos, nitratos u otras fracciones mas higroscopicas absorben mas agua. Por eso una regla fija universal de humedad puede mejorar, pero no garantiza exactitud absoluta.

Fuente: Patel et al. 2024; Frontiers 2021 field evaluation of Plantower PMS5003.

### 5. Se han propuesto correcciones por humedad

Patel et al. 2024 desarrollan una calibracion para PMS5003 que incorpora crecimiento higroscopico y parametros estacionales. En San Francisco, la calibracion con dependencia estacional de RH redujo el RMSE cerca de 40% frente a datos sin calibrar.

## Implicancia para este calefactor

Si el calefactor esta antes del Plantower, su justificacion tecnica probable es reducir la humedad relativa del aire que entra al sensor, o evitar condensacion. Calentar el aire sin agregar vapor de agua baja la humedad relativa, aunque no cambia la humedad absoluta.

Ejemplo conceptual:

- Aire frio y humedo entra al sistema.
- El calefactor aumenta la temperatura del flujo.
- Al subir la temperatura, la capacidad del aire para contener vapor aumenta.
- La humedad relativa baja.
- Baja el riesgo de condensacion y se reduce parte del sesgo optico asociado al crecimiento higroscopico.

El firmware vigente mide humedad con SHT40 y usa una histéresis de `65 % RH` para activar y `60 % RH` para desactivar el control por humedad. Aún es recomendable registrar una medición adicional después del calefactor para validar experimentalmente la reducción de RH antes del Plantower.

## Umbrales utiles para documentar o probar

Estos valores no son reglas universales, pero sirven como punto de partida experimental:

- RH < 60%: zona generalmente menos problematica.
- RH 65-70%: literatura reporta inicio de efecto higroscopico en PMS5003 en algunos casos.
- RH 74-80%: estudios de laboratorio reportan debilitamiento de desempeno dependiendo de composicion.
- RH > 85%: evaluaciones de campo muestran influencia clara y respuesta no lineal.
- RH cercana a 95% o condensacion: zona de alto riesgo para datos y hardware.

## Fuentes consultadas

- Plantower PMS5003 series data manual: https://www.aqmd.gov/docs/default-source/aq-spec/resources-page/plantower-pms5003-manual_v2-3.pdf
- Patel, M. Y. et al. 2024. *Towards a hygroscopic growth calibration for low-cost PM2.5 sensors*. Atmospheric Measurement Techniques. https://amt.copernicus.org/articles/17/1051/2024/
- *Calibration of Low-cost Sensors for Measurement of Indoor Particulate Matter Concentrations via Laboratory/Field Evaluation*. AAQR. https://aaqr.org/articles/aaqr-23-04-jk-0097.pdf
- *Field Calibration and Evaluation of an Internet-of-Things-Based Particulate Matter Sensor*. Frontiers in Environmental Science. https://www.frontiersin.org/journals/environmental-science/articles/10.3389/fenvs.2021.798485/full
