/**
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */
#ifndef __SERVO_H__
#define __SERVO_H__

// for GPIO
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <pigpio.h>

namespace servo
{
    extern const unsigned int pan_gpio;
    extern const unsigned int tilt_gpio;
    // This value is from the specification of the Servo
    extern const int pulse_width_0;   // Pulse width in microseconds for 0 degrees
    extern const int pulse_width_360; // Pulse width in microseconds for 360 degrees

    int panServo(unsigned int pulsewidth);
    int tiltServo(unsigned int pulsewidth);
    unsigned angleToPulsewidth(int angle);
    void stop(int signum);
} // namespace servo

#endif //__SERVO_H__