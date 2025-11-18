#ifndef MRFCONSTANTS_H
#define MRFCONSTANTS_H

// Constants
#define FWVERSION "7.0"
#define SLEEPTIMEOUT 60000

#define RXD2 RX
#define TXD2 TX

#define SS_SWITCH 24
#define SS_NEOPIX 6
#define SEESAW_ADDR 0x36 // Set this to the address of the seesaw

#define LENS_ADC_PIN 1 // Set this to the pin you've soldered the lens position sensor to on the ADS1015

#define SCREEN_WIDTH 128        // OLED display width, in pixels
#define SCREEN_HEIGHT 128       // OLED display height, in pixels
#define SCREEN_HEIGHT_EXT 32    // OLED display height, in pixels
#define OLED_RESET -1           // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3D     ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
#define SCREEN_ADDRESS_EXT 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32

const int SMOOTHING_WINDOW_SIZE = 15;
const int LENS_INF_THRESHOLD = 5;
const int LENS_CALIB_OFFSET = 0;

const float LENS_OUTLIER_THRESHOLD = 50.0;

#define RETICLE_OFFSET_X -4
#define RETICLE_OFFSET_Y -4
#define CLOSE_FOCUS 100
#define LIDAR_OFFSET 5

#define DISTANCE_MIN 5
#define DISTANCE_MAX 18

const int ISOS[] = {50, 80, 100, 125, 200, 400, 500, 640, 800, 1600, 3200, 6400};
const float CALIB_DISTANCES[] = {1, 1.2, 1.5, 2, 3, 5, 10};

const int K = 20;

#endif // MRFCONSTANTS_H