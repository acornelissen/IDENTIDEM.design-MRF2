#ifndef SETFUNCS_H
#define SETFUNCS_H

// Functions to read values from sensors and set variables
// ---------------------
void setDistance();

int getLensSensorReading();

void setLensDistance();

void setFilmCounter();

void setVoltage();

void setLightMeter();

void toggleLidar(bool lidarStatus);
// ---------------------

#endif // SETFUNCS_H