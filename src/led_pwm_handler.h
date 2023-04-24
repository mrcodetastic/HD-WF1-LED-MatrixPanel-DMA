#pragma once

#include <driver/ledc.h>
#include <math.h>

// LED control settings
const int LED_CHANNEL = 0;
const int LED_FREQ = 5000;
const int LED_RESOLUTION = 8;
const int LED_FADE_TIME = 1000; // in milliseconds

// Breathing effect settings
const int BREATH_MIN = 0;
const int BREATH_MAX = 255;
const int BREATH_TIME = 5000; // in milliseconds


// For pointless led fading in and out to tell me it's alive.
void ledFadeTask(void * parameter) {

    while(true)
    {

        unsigned long start_time = millis();

        // Fade in
        for (int i = BREATH_MIN; i <= BREATH_MAX; i++) {
            int duty = map(i, BREATH_MIN, BREATH_MAX, 0, pow(2, LED_RESOLUTION) - 1);
            ledcWrite(LED_CHANNEL, duty);
            delay(LED_FADE_TIME / (BREATH_MAX - BREATH_MIN));
        }
        
        // Fade out
        for (int i = BREATH_MAX; i >= BREATH_MIN; i--) {
            int duty = map(i, BREATH_MIN, BREATH_MAX, 0, pow(2, LED_RESOLUTION) - 1);
            ledcWrite(LED_CHANNEL, duty);
            delay(LED_FADE_TIME / (BREATH_MAX - BREATH_MIN));
        }
    }

    return;
}
