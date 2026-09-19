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
#define HIGH_SPEED 1500
#define BASE_SPEED 1500

#define LIMIT_1_PIN 33
#define LIMIT_2_PIN 35

#define SOLENOID_A_PIN 12
#define SOLENOID_B_PIN 14
#define POWER_RELAY_PIN 25

// motion parameters
#define MOTOR_STEPS_PER_REV 200.0f
#define MICROSTEPS 4.0f
#define LEADSCREW_LEAD_MM 4.0f
#define GEAR_RATIO 20.0f / 80.0f
#define MM_PER_INCH 25.4f

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

enum class EMS_STATE : bool {
    RUNNING,
    SHUTDOWN
};
using ems_state_t = EMS_STATE; 

enum class MOTION_STATE : uint8_t {
    IDLE,
    HOMING,
    RUNNING,
    SHUTDOWN
};
using motion_state_t = MOTION_STATE;


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

        /**
         * @brief Manages the EMS shutdown and startup
         * 
         * @param gpio_state - the state of the EMS gpio pin
         * @return ems_state_t - The actual EMS state
         * 
         * @remarks - the actual EMS shutdown is performed via physical switch. The handler is responsible
         * for auxiliary shutdowns such as air and lubricant and feed
         */
        ems_state_t manage_ems_state(uint8_t gpio_state);

        /**
         * @brief Starts the homing sequence
         * 
         * @param complete - function to call when the homing is complete.
         */
        void home(std::function<void()> complete);

        /**
         * @brief Gets the current state of the motion object
         * 
         * @return motion_state_t - a state enum class.
         */
        motion_state_t get_state() { return this->_state; }

        /**
        * @brief Gets the current feed rate.
        *
        * Assumptions:
        * - 200 step/rev motor
        * - 4 microsteps/full step
        * - 20T : 80T pulley ratio (4:1 reduction)
        * - TR8x4 leadscrew (4 mm lead)
        *
        * @return Feed rate in inches per minute.
        */
        float feed_rate_ipm();

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


        /**
         * @brief Task function performing the homing to of the feed carriage
         * 
         * @param args - pointer to task arguments 
         */
        static void homing_runner(void * args);


    private:

        HardwareSerial* _tmc_serial = nullptr;
        TMC2209Stepper* _tmc_driver = nullptr;
        volatile bool _home_limit = false;
        volatile bool _feed_limit = false;
        volatile bool _job_should_exit = false;
        volatile bool _stall_alert = false;
        volatile motion_state_t _state = MOTION_STATE::IDLE;
        volatile ems_state_t _ems_state = EMS_STATE::RUNNING;
        volatile air_state_t _air_state = AIR_STATE::OFF;
        volatile coolant_state_t _coolant_state = COOLANT_STATE::OFF;
        uint16_t _frequency = BASE_SPEED;
        TaskHandle_t _homing_task = NULL;
};

struct TaskArgs
{
    Motion* self;
    std::function<void()> callback;
};





#endif // _MOTION_H_