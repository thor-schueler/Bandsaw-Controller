// Copyright (c) Thor Schueler. All rights reserved.
// SPDX-License-Identifier: MIT

#ifndef _MOTION_H_
#define _MOTION_H_

#include "Arduino.h"
#include <functional>
#include <TMCStepper.h>
#include "driver/ledc.h"
#include "src/logging/SerialLogger.h"

// UART pins (single-wire PDN_UART)
#define TMC_UART_TX   17   // ESP32 TX → module RX pad via 1K resistor (PDN_UART)
#define TMC_UART_RX   16   // ESP32 RX → module RX pad (PDN_UART)

// Step/Dir pins
#define STEP_PIN      2
#define DIR_PIN       15
#define EN_PIN        0
#define DIAG_PIN      13

// PWM channel
#define STEP_PWM_CHANNEL 0

// TMC2209 settings
#define R_SENSE       0.10f
#define DRIVER_ADDR   0b00
#define SPEED 1500

#define LIMIT_1_PIN 33
#define LIMIT_2_PIN 35

#define SOLENOID_A_PIN 12
#define SOLENOID_B_PIN 14
#define POWER_RELAY_PIN 25










#endif // _MOTION_H_