// Copyright (c) Thor Schueler. All rights reserved.
// SPDX-License-Identifier: MIT

#include "motion.h"


/**
 * @brief Construct a new Motion object
 * 
 */
Motion::Motion()
{
    this->_tmc_serial = new HardwareSerial(2);
    this->_tmc_driver = new TMC2209Stepper(this->_tmc_serial, R_SENSE, DRIVER_ADDR);
}

/**
 * @brief Destroy the Motion object and cleans up associated resources
 * 
 */
Motion::~Motion()
{
    delete this->_tmc_driver;
    this->_tmc_serial->end();
    delete this->_tmc_serial;
}

/**
 * @brief Initializes key objects and structures and starts the execution
 * 
 */
void Motion::begin()
{
    Logger.Info(F("... Begin motion controller execution."));
    // Blade Motor pin
    Logger.Info(F("...   Configure saw blade GPIO."));
    pinMode(POWER_RELAY_PIN, OUTPUT);
    digitalWrite(POWER_RELAY_PIN, HIGH);

    // Configure Solenoid pins
    Logger.Info(F("...   Configure solenoid GPIO."));
    pinMode(SOLENOID_A_PIN, OUTPUT);
    pinMode(SOLENOID_B_PIN, OUTPUT);
    digitalWrite(SOLENOID_A_PIN, HIGH);
    digitalWrite(SOLENOID_B_PIN, HIGH);

    // Stepper pins
    Logger.Info(F("...   Configure stepper pins GPIO."));
    pinMode(DIR_PIN, OUTPUT);
    pinMode(EN_PIN, OUTPUT);
    pinMode(DIAG_PIN, INPUT_PULLUP);
    digitalWrite(EN_PIN, HIGH);                              // disable stepper initially  
    attachInterruptArg(DIAG_PIN, stall_isr, this, RISING);  // or FALLING depending on polarity

    // Attach limit switches
    Logger.Info(F("...   Configure end stop limit switches."));
    pinMode(LIMIT_1_PIN, INPUT);
    pinMode(LIMIT_2_PIN, INPUT);
    attachInterruptArg(LIMIT_1_PIN, limit_isr, this, CHANGE);  
    attachInterruptArg(LIMIT_2_PIN, limit_isr, this, CHANGE);  

    // Start driver serial
    Logger.Info(F("...   Starting TMC2209 driver."));
    this->_tmc_serial->begin(115200, SERIAL_8N1, TMC_UART_RX, TMC_UART_TX);

    // tmc driver start
    // Driver init
    this->_tmc_driver->begin();
    this->_tmc_driver->toff(4);
    this->_tmc_driver->blank_time(24);
    this->_tmc_driver->rms_current(2000);       // Maximum Stepper current
    this->_tmc_driver->microsteps(4);           // Initial Microstep configuration. 
    this->_tmc_driver->pwm_autoscale(true);     // Enable StealthChop

    //
    // StallGuard configuration
    //
    this->_tmc_driver->en_spreadCycle(true);    // StallGuard ONLY works in SpreadCycle
    this->_tmc_driver->pwm_autoscale(false);    // Disable StealthChop
    this->_tmc_driver->TCOOLTHRS(0xFFFFF);      // Allow SG to operate at all speeds
    this->_tmc_driver->SGTHRS(50);              // StallGuard threshold (tune this)
    this->_tmc_driver->irun(31);
    this->_tmc_driver->ihold(31);

    // PWM configuration
    Logger.Info(F("...   Configure PWM for stepper."));
    ledc_timer_config_t t = {
        .speed_mode      = LEDC_HIGH_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_12_BIT,
        .timer_num       = LEDC_TIMER_0,
        .freq_hz         = SPEED,
        .clk_cfg         = LEDC_USE_APB_CLK,
    };

    ledc_channel_config_t c = {
        .gpio_num   = STEP_PIN,
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .channel    = LEDC_CHANNEL_0,
        .timer_sel  = LEDC_TIMER_0,
        .duty       = 512,  // 50% duty cycle on 12-bit resolution
        .hpoint     = 0
    };
    ledc_timer_config(&t);
    ledc_channel_config(&c);

    Logger.Info(F("...   Done."));
}

/**
 * @brief Event handler watching TMC Stepper Diag pin for stall guard notifications
 * @param arg - argumnent passed to the handler, expected to be the instance of the calling object and can 
 * be cast to Motion*
 */
void IRAM_ATTR Motion::stall_isr(void * arg) 
{
    Motion *_this = reinterpret_cast<Motion *>(arg);
    _this->_stall_alert = true;
}

/**
 * @brief Event handler watching the stepper limit switches.
 * @param arg - argumnent passed to the handler, expected to be the instance of the calling object and can 
 * be cast to Motion*
 */ 
void IRAM_ATTR Motion::limit_isr(void * arg) 
{
    Motion *_this = reinterpret_cast<Motion *>(arg);
    _this->_home_limit = digitalRead(LIMIT_1_PIN);
    _this->_feed_limit = digitalRead(LIMIT_2_PIN);

}

/**
 * @brief Get the status of the bandsaw blade (running or stopped)
 * 
 * @return blade_state_t 
 */
blade_state_t Motion::get_blade_status()
{
    return digitalRead(POWER_RELAY_PIN) ? BLADE_STATE::STOPPED : BLADE_STATE::RUNNING;
        // The relay to switch on the blade is active low...
}

/**
 * @brief Activates the blade
 * 
 * @return blade_state_t containing the actual state of the blade after execution 
 * 
 * @remark The blade will not activate if the EMS is active. 
 */
blade_state_t Motion::activate_blade()
{
    if(this->_is_EMS_active) 
    {
        digitalWrite(POWER_RELAY_PIN, HIGH);
        Logger.Info(F("... Received request to activate blade, but EMS is toggled. Ignore..."));
        return BLADE_STATE::STOPPED;
    }
    if(this->get_blade_status() == BLADE_STATE::RUNNING)
    {
        Logger.Info(F("... Received request to activate blade, blade is already running. Ignore..."));
        return BLADE_STATE::RUNNING;    
    }
    digitalWrite(POWER_RELAY_PIN, LOW);
    Logger.Info(F("... Saw blade has been actviated."));
    return BLADE_STATE::RUNNING;
}

/**
 * @brief Deactivates the blade
 * 
 * @return blade_state_t containing the actual state of the blade after execution 
 */
blade_state_t Motion::deactivate_blade()
{
        if(this->_is_EMS_active) 
    {
        digitalWrite(POWER_RELAY_PIN, HIGH);
        Logger.Info(F("... Received request to de-activate blade, but EMS is toggled. Ignore..."));
        return BLADE_STATE::STOPPED;
    }
    if(this->get_blade_status() == BLADE_STATE::STOPPED)
    {
        Logger.Info(F("... Received request to deactivate blade, blade is already stopped. Ignore..."));
        return BLADE_STATE::STOPPED;    
    }
    digitalWrite(POWER_RELAY_PIN, HIGH);
    Logger.Info(F("... Saw blade has been stopped."));
    return BLADE_STATE::STOPPED;
}

/**
 * @brief Manages the air blast solenoid based on the switch state
 * 
 * @param gpio_on - the state of the always on switch
 * @param gpio_auto - the state of the auto switch
 * @return air_state_t - the actual state of the solenoid after the operation
 */
air_state_t Motion::manage_air(uint8_t gpio_on, uint8_t gpio_auto)
{
    if(gpio_auto == LOW && gpio_on == LOW)
    {
        digitalWrite(SOLENOID_A_PIN, HIGH);
        Logger.Info(F("... Air blast solendoid switched off"));

        //
        // TODO - remove monitoring task if necessary
        //

        return AIR_STATE::OFF;
    }
    if(gpio_on == HIGH)
    {
        digitalWrite(SOLENOID_A_PIN, LOW);
        Logger.Info(F("... Air blast solendoid switched on"));

        //
        // TODO - remove monitoring task if necessary
        //

        return AIR_STATE::ON;
    }
    if(gpio_auto)
    {
        digitalWrite(SOLENOID_A_PIN, HIGH);
        Logger.Info(F("... Air blast solendoid switched to auto. Monitoring feed has started"));

        //
        // TODO - start monitoring task
        //

        return AIR_STATE::AUTO;       
    }
}

/**
 * @brief Manages the coolant solenoid based on the switch state
 * 
 * @param gpio_on - the state of the always on switch
 * @param gpio_auto - the state of the auto switch
 * @return coolant_state_t - the actual state of the solenoid after the operation
 */
coolant_state_t Motion::manage_coolant(uint8_t gpio_on, uint8_t gpio_auto)
{
    if(gpio_auto == LOW && gpio_on == LOW)
    {
        digitalWrite(SOLENOID_B_PIN, HIGH);
        Logger.Info(F("... Coolant solendoid switched off"));
        return COOLANT_STATE::OFF;
    }
    if(gpio_on == HIGH)
    {
        digitalWrite(SOLENOID_B_PIN, LOW);
        Logger.Info(F("... Coolant solendoid switched on"));
        return COOLANT_STATE::ON;
    }
    if(gpio_auto)
    {
        digitalWrite(SOLENOID_B_PIN, HIGH);
        Logger.Info(F("... Coolant solendoid switched to auto. Monitoring feed has started"));
        return COOLANT_STATE::AUTO;       
    }
}