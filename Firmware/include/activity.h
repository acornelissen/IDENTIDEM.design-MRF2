#ifndef ACTIVITY_H
#define ACTIVITY_H

void registerActivity();
void updateSleepMode(unsigned long now_ms);
unsigned long getIdleDurationMs(unsigned long now_ms);

#endif // ACTIVITY_H
