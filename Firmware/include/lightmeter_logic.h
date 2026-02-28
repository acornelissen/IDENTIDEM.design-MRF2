#ifndef LIGHTMETER_LOGIC_H
#define LIGHTMETER_LOGIC_H

#include <Arduino.h>
#include <stddef.h>

void formatShutterSpeed(float lux, float aperture, int iso, char *buffer, size_t bufferSize);
float applyExposureCompensationToLux(float lux, float exposure_comp_ev);
float getMeterSmoothingAlpha(int smoothing_mode);
const char *getMeterSmoothingLabel(int smoothing_mode);
float calculateEV100(float lux);

#endif // LIGHTMETER_LOGIC_H
