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

     // Attach limit switches
    Logger.Info(F("...   Configure end stop limit switches."));
    pinMode(LIMIT_1_PIN, INPUT);
    pinMode(LIMIT_2_PIN, INPUT);
    //attachInterruptArg(LIMIT_1_PIN, limit_isr, this, CHANGE);  
    //attachInterruptArg(LIMIT_2_PIN, limit_isr, this, CHANGE); 

    // Stepper pins
    Logger.Info(F("...   Configure stepper pins GPIO."));
    pinMode(DIR_PIN, OUTPUT);
    pinMode(EN_PIN, OUTPUT);
    pinMode(DIAG_PIN, INPUT_PULLUP);
    digitalWrite(EN_PIN, HIGH);                             // disable stepper initially  
    digitalWrite(DIR_PIN, HIGH);                            // initialize feed direction towards blade       
    attachInterruptArg(DIAG_PIN, stall_isr, this, RISING);  // or FALLING depending on polarity

    // PWM configuration
    Logger.Info(F("...   Configure PWM for stepper."));
    ledc_timer_config_t _timer_config = {
        .speed_mode      = LEDC_HIGH_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_12_BIT,
        .timer_num       = LEDC_TIMER_0,
        .freq_hz         = BASE_SPEED,
        .clk_cfg         = LEDC_USE_APB_CLK,
    };

    ledc_channel_config_t _channel_config = {
        .gpio_num   = STEP_PIN,
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .channel    = LEDC_CHANNEL_0,
        .timer_sel  = LEDC_TIMER_0,
        .duty       = 512,  // 50% duty cycle on 12-bit resolution
        .hpoint     = 0
    };
    this->_frequency = BASE_SPEED;
    esp_err_t r = ledc_timer_config(&_timer_config);
    if(r != ESP_OK) Logger.Error_f(F("PWM timer configuration failed with 0x%04X"), r);
    r = ledc_channel_config(&_channel_config);
    if(r != ESP_OK) Logger.Error_f(F("PWM channel configuration failed with 0x%04X"), r);

    // Start driver serial
    Logger.Info(F("...   Starting TMC2209 driver."));
    this->_tmc_serial->begin(115200, SERIAL_8N1, TMC_UART_RX, TMC_UART_TX);
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

    Logger.Info(F("...   TMC2209 initialized with StallGuard."));
    Logger.Info_f(F("...   PWM freq: %u"), ledc_get_freq(LEDC_HIGH_SPEED_MODE, LEDC_TIMER_0));
    Logger.Info_f(F("...   Driver IHold/IRun: 0x%08X"), this->_tmc_driver->IHOLD_IRUN());
    Logger.Info_f(F("...   Driver TPowerDown: 0x%08X"), this->_tmc_driver->TPOWERDOWN());
    Logger.Info_f(F("...   Driver Config: 0x%08X"), this->_tmc_driver->GCONF());
    Logger.Info_f(F("...   Driver Status: 0x%08X"), this->_tmc_driver->DRV_STATUS());
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
    // debounce limit switch for 250ms
    volatile uint32_t lastDebounceTime = 0; // Last debounce time volatile 
    uint32_t currentTime = millis(); 

    if ((currentTime - lastDebounceTime) < 250) return;                         // ignore bounces

    Motion *_this = reinterpret_cast<Motion *>(arg);  
    _this->_home_limit = digitalRead(LIMIT_1_PIN);                              // active low
    _this->_feed_limit = digitalRead(LIMIT_2_PIN);                              // active low
    if(!_this->_home_limit || !_this->_feed_limit) digitalWrite(EN_PIN, HIGH);  // disable Stepper
    if(!_this->_home_limit) digitalWrite(DIR_PIN, HIGH);                        // set allowable direction
    if(!_this->_feed_limit) digitalWrite(DIR_PIN, LOW);
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
    if(this->_ems_state == EMS_STATE::SHUTDOWN) 
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
        //
        // TODO - remove monitoring task if necessary
        //

        if(this->_ems_state == EMS_STATE::SHUTDOWN)
        {
            Logger.Info(F("... Air blast activation request received, but EMS is active. Ignoring..."));
            return AIR_STATE::AUTO;   
        }
        digitalWrite(SOLENOID_A_PIN, LOW);
        Logger.Info(F("... Air blast solendoid switched on"));
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
        if(this->_ems_state == EMS_STATE::SHUTDOWN)
        {
            Logger.Info(F("... Coolant activation request received, but EMS is active. Ignoring..."));
            return COOLANT_STATE::AUTO;   
        }
        
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

/**
 * @brief Manages the EMS shutdown and startup
 * 
 * @param gpio_state - the state of the EMS gpio pin
 * @return ems_state_t - The actual EMS state (active low)
 * 
 * @remarks - the actual EMS shutdown is performed via physical switch. The handler is responsible
 * for auxiliary shutdowns such as air and lubricant and feed
 */
ems_state_t Motion::manage_ems_state(uint8_t gpio_state)
{
    if(!gpio_state)
    {
        // shudown feed first
        digitalWrite(EN_PIN, HIGH);             // active low

        // shutdown pwoer relay 
        digitalWrite(POWER_RELAY_PIN, HIGH);    // active low
                                                // the power is already physically  disconnected 
                                                // physically via the EMS switch

        // shutdown solenoids
        digitalWrite(SOLENOID_A_PIN, HIGH);     // active low
        digitalWrite(SOLENOID_B_PIN, HIGH);     // active low
        Logger.Info(F("... Emergency shutdown performed, blade, feed and solenoids have been shutdown."));
        this->_ems_state = EMS_STATE::SHUTDOWN;
        this->_state = MOTION_STATE::SHUTDOWN;
    }
    else
    {
        this->_ems_state = EMS_STATE::RUNNING;
        this->_state = MOTION_STATE::IDLE;
        Logger.Info(F("... Emergency shutdown cleared"));
    }
    return this->_ems_state;
}

/**
 * @brief Starts the homing sequence
 * 
 * @param complete - function to call when the homing is complete.
 */
void Motion::home(std::function<void()> complete)
{
    if(this->_homing_task != NULL)
    {
        _job_should_exit = true;
    }
    else
    {
        TaskArgs* args = new TaskArgs { this, std::move(complete) };
        xTaskCreatePinnedToCore(homing_runner, "homingRunner", 2048, args, 1, &_homing_task, 1);
    }
}

/**
 * @brief Task function performing the homing to of the feed carriage
 * 
 * @param args - pointer to task arguments 
 */
void Motion::homing_runner(void * args)
{
    TaskArgs* _args = static_cast<TaskArgs*>(args);
    Motion* _this = _args->self;
    int i=0;
    uint16_t _s = _this->_frequency;

    Logger.Info(F("... Starting homing task"));
    _this->_state = MOTION_STATE::HOMING;
    _this->_frequency = HIGH_SPEED;
    ledc_set_freq(LEDC_HIGH_SPEED_MODE, LEDC_TIMER_0, _this->_frequency); 
    Logger.Info_f(F("...   PWM freq: %u"), ledc_get_freq(LEDC_HIGH_SPEED_MODE, LEDC_TIMER_0));
    while(digitalRead(LIMIT_1_PIN) == HIGH)
    {
        if(_this->_ems_state == EMS_STATE::SHUTDOWN)  break;
        if(_this->_job_should_exit) break;

        digitalWrite(DIR_PIN, LOW);         // set direction towards home
        digitalWrite(EN_PIN, LOW);          // enable stepper
        vTaskDelay(100);                    // delay 100ms. There is no risk here as the interrupt handler will
                                            // disable the stepper as soon as the home limit has been hit. 
        i++;
        if(i > 100) break;
    } 

    digitalWrite(EN_PIN, HIGH);             // disable the stepper in case we terminated due to EMS or user termination.
    ledc_set_freq(LEDC_HIGH_SPEED_MODE, LEDC_TIMER_0, _s); 
    _this->_frequency = _s;
    _args->callback();
    if(_this->_ems_state == EMS_STATE::SHUTDOWN) 
    {
        Logger.Info(F("...   Homing task terminated due to EMS shutdown.")); 
        _this->_state = MOTION_STATE::SHUTDOWN;
    }
    else if(_this->_job_should_exit)
    {
        Logger.Info(F("...   Homing task terminated due to user request.")); 
        _this->_job_should_exit = false;
        _this->_state = MOTION_STATE::IDLE;
    }
    else
    {    
        if(i == 0) Logger.Info(F("...   Carriage already home."));
        if(i > 0) Logger.Info(F("...   Homing successful, carriage home."));
        Logger.Info(F("...   Homing task complete."));
        _this->_state = MOTION_STATE::IDLE;
    }
    _this->_homing_task = NULL;
    delete _args;
    vTaskDelete(NULL);
}

/**
 * @brief Gets the current feed rate.
 *
 *      Assumptions:
 *          - 200 step/rev motor
 *          - 4 microsteps/full step
 *          - 20T : 80T pulley ratio (4:1 reduction)
 *          - TR8x4 leadscrew (4 mm lead)
 *
 * @return Feed rate in inches per minute.
 */
float Motion::feed_rate_ipm()
{
    const float microsteps_per_rev = MOTOR_STEPS_PER_REV * MICROSTEPS;
    const float screw_rev_per_microstep = GEAR_RATIO / microsteps_per_rev;
    const float mm_per_microstep = screw_rev_per_microstep * LEADSCREW_LEAD_MM;
    const float mm_per_min = this->_frequency * mm_per_microstep * 60.0f;
    return mm_per_min / MM_PER_INCH;  
}
