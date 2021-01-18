// Example usage for Adafruit_ADS1015 library by Adafruit.

#include "Adafruit_ADS1015.h"

// Initialize objects from the lib
Adafruit_ADS1015 adafruit_ADS1015;

void setup() {
    // Call functions on initialized library objects that require hardware
    adafruit_ADS1015.begin();
}

void loop() {
    // Use the library's initialized objects and functions
    adafruit_ADS1015.process();
}
