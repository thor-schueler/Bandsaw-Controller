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


enum class BLADE_STATE : bool {
    STOPPED,
    RUNNING
};
using blade_state_t = BLADE_STATE;

enum class AIR_STATE : uint8_t {
    OFF,
    ON,
    AUTO
};
using air_state_t = AIR_STATE;


enum class COOLANT_STATE : uint8_t {
    OFF,
    ON,
    AUTO
};
using coolant_state_t = COOLANT_STATE; 

/**
 * @brief Manages the blade movement, feed movement and other ciritcal items for bandsaw.
 * 
 */
class Motion
{
    public:

        /**
         * @brief Construct a new Motion object
         * 
         */
        Motion();

        /**
         * @brief Destroy the Motion object and cleans up associated resources
         * 
         */
        ~Motion();

        /**
         * @brief Initializes key objects and structures and starts the execution
         * 
         */
        void begin();

        /**
         * @brief Get the status of the bandsaw blade (running or stopped)
         * 
         * @return blade_state_t 
         */
        blade_state_t get_blade_status();

        /**
         * @brief Activates the blade
         * 
         * @return blade_state_t containing the actual state of the blade after execution 
         * 
         * @remark The blade will not activate if the EMS is active. 
         */
        blade_state_t activate_blade();

        /**
         * @brief Deactivates the blade
         * 
         * @return blade_state_t containing the actual state of the blade after execution 
         */
        blade_state_t deactivate_blade();

        /**
         * @brief Manages the air blast solenoid based on the switch state
         * 
         * @param gpio_on - the state of the always on switch
         * @param gpio_auto - the state of the auto switch
         * @return air_state_t - the actual state of the solenoid after the operation
         */
        air_state_t manage_air(uint8_t gpio_on, uint8_t gpio_auto);

        /**
         * @brief Manages the coolant solenoid based on the switch state
         * 
         * @param gpio_on - the state of the always on switch
         * @param gpio_auto - the state of the auto switch
         * @return coolant_state_t - the actual state of the solenoid after the operation
         */
        coolant_state_t manage_coolant(uint8_t gpio_on, uint8_t gpio_auto);

    protected:

        /**
         * @brief Event handler watching TMC Stepper Diag pin for stall guard notifications
         * @param arg - argumnent passed to the handler, expected to be the instance of the calling object and can 
         * be cast to Motion*
         */
        static void IRAM_ATTR stall_isr(void * arg);

        /**
         * @brief Event handler watching the stepper limit switches.
         * @param arg - argumnent passed to the handler, expected to be the instance of the calling object and can 
         * be cast to Motion*
         */        
        static void IRAM_ATTR limit_isr(void * arg);


    private:

        HardwareSerial* _tmc_serial = nullptr;
        TMC2209Stepper* _tmc_driver = nullptr;
        bool _home_limit = false;
        bool _feed_limit = false;
        bool _stall_alert = false;
        bool _is_EMS_active = false;
        air_state_t _air_state = AIR_STATE::OFF;
        coolant_state_t _coolant_state = COOLANT_STATE::OFF;

};







#endif // _MOTION_H_