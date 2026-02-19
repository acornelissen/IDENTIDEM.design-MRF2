#ifndef LIGHTMETER_LOGIC_H
#define LIGHTMETER_LOGIC_H

#include <Arduino.h>

String formatShutterSpeed(float lux, float aperture, int iso);
float applyExposureCompensationToLux(float lux, float exposure_comp_ev);
float getMeterSmoothingAlpha(int smoothing_mode);
const char *getMeterSmoothingLabel(int smoothing_mode);
float calculateEV100(float lux);

#endif // LIGHTMETER_LOGIC_H
