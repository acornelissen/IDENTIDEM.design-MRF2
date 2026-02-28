#ifndef LIDAR_LOGIC_H
#define LIDAR_LOGIC_H

#include <stddef.h>
#include <DTS6012M_UART.h>

struct LidarCandidate
{
  bool valid;
  int distance_cm;
  int confidence;
  int quality_level; // 1..4 (poor..excellent), 0 when invalid
};

LidarCandidate chooseBestLidarCandidate(const DTSMeasurement &measurement,
                                        int previous_distance_cm,
                                        bool has_lens_prior,
                                        int lens_prior_cm);

int blendLidarDistance(int previous_distance_cm, int next_distance_cm, int confidence);

void formatDistanceDisplay(int corrected_cm, char *buffer, size_t bufferSize);

#endif // LIDAR_LOGIC_H
