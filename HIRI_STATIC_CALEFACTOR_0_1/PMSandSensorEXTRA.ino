// -------------------- Plantower PMS non-blocking parser --------------------

#include "config.h"
extern Adafruit_NeoPixel pixels;
extern SoftwareSerial pms;

void readPMS() {
  while (pms.available() > 0) {
    int c = pms.read();
    if (c < 0) break;
    lastPmsSeen = millis();
    if (pmsHead < sizeof(pmsBuf)) {
      pmsBuf[pmsHead++] = (uint8_t)c;
    } else {
      memmove(pmsBuf, pmsBuf + 1, sizeof(pmsBuf) - 1);
      pmsBuf[sizeof(pmsBuf) - 1] = (uint8_t)c;
    }
  }

  size_t i = 0;
  while (pmsHead >= 32 && i + 32 <= pmsHead) {
    uint8_t *frame = &pmsBuf[i];
    if (frame[0] != 0x42 || frame[1] != 0x4D) {
      ++i;
      continue;
    }

    uint16_t receivedChecksum =
        ((uint16_t)frame[30] << 8) | frame[31];
    uint16_t calculatedChecksum = 0;
    for (uint8_t k = 0; k < 30; ++k) calculatedChecksum += frame[k];

    if (receivedChecksum != calculatedChecksum) {
      ++i;
      continue;
    }

    PM1 = ((uint16_t)frame[10] << 8) | frame[11];
    PM25 = ((uint16_t)frame[12] << 8) | frame[13];
    PM10 = ((uint16_t)frame[14] << 8) | frame[15];

    uint16_t temp10 = ((uint16_t)frame[24] << 8) | frame[25];
    uint16_t humidity10 = ((uint16_t)frame[26] << 8) | frame[27];
    float temperature = temp10 / 10.0f;
    float humidity = humidity10 / 10.0f;
    if ((temp10 != 0 || humidity10 != 0) &&
        temperature > -40.0f && temperature < 85.0f &&
        humidity >= 0.0f && humidity <= 100.0f) {
      pmsTempC = temperature;
      pmsHum = humidity;
    } else {
      pmsTempC = NAN;
      pmsHum = NAN;
    }

    size_t remaining = pmsHead - (i + 32);
    memmove(pmsBuf, &frame[32], remaining);
    pmsHead = remaining;
    i = 0;
  }

  if (i > 0 && i < pmsHead) {
    memmove(pmsBuf, pmsBuf + i, pmsHead - i);
    pmsHead -= i;
  } else if (i >= pmsHead) {
    pmsHead = 0;
  }
}

// -------------------- PM2.5 smooth LED gradient (15/25/50) --------------------
static inline uint8_t lerp8(uint8_t a, uint8_t b, float t) {
  float value = a + (b - a) * t;
  if (value < 0) value = 0;
  if (value > 255) value = 255;
  return (uint8_t)value;
}

void updatePmLed(float pm25) {
  uint8_t r = 0, g = 0, b = 0;
  if (pm25 <= 15.0f) {
    g = lerp8(60, 127, pm25 / 15.0f);
  } else if (pm25 <= 25.0f) {
    float t = (pm25 - 15.0f) / 10.0f;
    r = lerp8(0, 127, t);
    g = 127;
  } else if (pm25 <= 50.0f) {
    float t = (pm25 - 25.0f) / 25.0f;
    r = 127;
    g = lerp8(127, 0, t);
  } else {
    r = 127;
  }
  pixels.setPixelColor(0, pixels.Color(r, g, b));
  pixels.show();
}
