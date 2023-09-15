/**
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */
#include "Servo.h"

namespace servo
{
    extern const unsigned int pan_gpio = 25;
    extern const unsigned int tilt_gpio = 23;
    // This value is from the specification of the Servo
    extern const int pulse_width_0 = 500;    // Pulse width in microseconds for 0 degrees
    extern const int pulse_width_360 = 2500; // Pulse width in microseconds for 360 degrees

    int run = 1;

    int panServo(unsigned int pulsewidth)
    {
        int result;
        result = gpioServo(pan_gpio, pulsewidth);
        return result;
    }

    int tiltServo(unsigned int pulsewidth)
    {
        int result;
        result = gpioServo(tilt_gpio, pulsewidth);
        return result;
    }

    unsigned angleToPulsewidth(int angle)
    {
        unsigned int pulsewidth;
        pulsewidth = pulse_width_0 + (angle * (pulse_width_360 - pulse_width_0) / 360);
        return pulsewidth;
    }

    void stop(int signum)
    {
        run = 0;
    }
} // namespace servo