#ifndef CALIBRATION_LOGIC_H
#define CALIBRATION_LOGIC_H

bool computeStableCalibrationReading(
    const int *samples,
    int sample_count,
    int outlier_max_delta,
    int min_inlier_count,
    int max_inlier_spread,
    int &averaged_reading);

bool validateMonotonicCalibration(const int *readings, int reading_count, int min_step);

#endif // CALIBRATION_LOGIC_H
