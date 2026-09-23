// Copyright (c) Thor Schueler. All rights reserved.
// SPDX-License-Identifier: MIT

#include "motion.h"

ledc_channel_config_t _channel_config = {
    .gpio_num   = STEP_PIN,
    .speed_mode = LEDC_HIGH_SPEED_MODE,
    .channel    = LEDC_CHANNEL_0,
    .timer_sel  = LEDC_TIMER_0,
    .duty       = 512,  // 50% duty cycle on 12-bit resolution
    .hpoint     = 0
 };

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
    if(this->_pulse_task != NULL) vTaskDelete(this->_pulse_task);
    this->_job_should_exit = true;
    this->_pulse_task = NULL;

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
    digitalWrite(SOLENOID_A_PIN, LOW);                      // active high
    digitalWrite(SOLENOID_B_PIN, LOW);                      // active high

     // Attach limit switches
    Logger.Info(F("...   Configure end stop limit switches."));
    pinMode(LIMIT_1_PIN, INPUT);
    pinMode(LIMIT_2_PIN, INPUT);
    attachInterruptArg(LIMIT_1_PIN, limit_isr, this, CHANGE);  
    attachInterruptArg(LIMIT_2_PIN, limit_isr, this, CHANGE); 

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
        .freq_hz         = FREQUENCY_BASE,
        .clk_cfg         = LEDC_USE_APB_CLK,
    };

    esp_err_t r = ledc_timer_config(&_timer_config);
    if(r != ESP_OK) Logger.Error_f(F("PWM timer configuration failed with 0x%04X"), r);
    r = ledc_channel_config(&_channel_config);
    if(r != ESP_OK) Logger.Error_f(F("PWM channel configuration failed with 0x%04X"), r);
    r = ledc_stop(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, 0); 
    pinMode(STEP_PIN, OUTPUT); 
    this->_frequency = 0;
                // Stop LEDC PWM for now, force STEP low this allow for manual control. 
                // we are only using PWM feed for homing and feeding. 
    if(r != ESP_OK) Logger.Error_f(F("PWM pausing failed with 0x%04X"), r);

    // Task Registration
    Logger.Info(F("...   Starting Manual Pulse Runner Task."));
    xTaskCreatePinnedToCore(manual_pulse_runner, "manualPulseRunner", 2048, this, 1, &_pulse_task, 1);

    // Start driver serial
    Logger.Info(F("...   Starting TMC2209 driver."));
    this->_tmc_serial->begin(115200, SERIAL_8N1, TMC_UART_RX, TMC_UART_TX);
    this->_tmc_driver->begin();
    this->_tmc_driver->toff(4);
    this->_tmc_driver->blank_time(24);
    this->_tmc_driver->rms_current(MOTOR_CURRENT);      // Maximum Stepper current
    this->_tmc_driver->microsteps(MOTOR_MICROSTEPS);    // Microsteps per step
    this->_tmc_driver->intpol(true);

    //
    // StallGuard configuration
    //
    this->_tmc_driver->en_spreadCycle(false);           // StallGuard ONLY works in SpreadCycle
    this->_tmc_driver->pwm_autoscale(true);             // Enable Stealthchop
    this->_tmc_driver->TCOOLTHRS(0);                    // Stallguard disabled at this time
    this->_tmc_driver->SGTHRS(0);                       // Stallguard disabled at this time
    this->_tmc_driver->irun(31);                        // Running current ratio, 31 = 100%
    this->_tmc_driver->ihold(31);                       // Holding current ratio, 31 = 100%

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
    bool hl = false;
    bool fl = false;
    bool dir = digitalRead(DIR_PIN);
    //for(int i=0; i<10; i++)
    //{
    //    hl |= digitalRead(LIMIT_1_PIN);
    //    fl |= digitalRead(LIMIT_2_PIN);
    //    delayMicroseconds(1);
    //}

    hl = digitalRead(LIMIT_1_PIN);                              // active low
    fl = digitalRead(LIMIT_2_PIN);                              // active low
    //if((_this->_home_limit &&  !digitalRead(DIR_PIN)) || (_this->_feed_limit) && digitalRead(DIR_PIN)) 
    if((!hl && !dir) || (!fl && dir))
    {
        if(!hl && !dir) _this->_home_limit = true;
        if(!fl && dir) _this->_feed_limit = true;
        digitalWrite(EN_PIN, HIGH);  // disable Stepper
    }
    if(hl) _this->_home_limit = false;
    if(fl) _this->_feed_limit = false;
    if(!hl && !dir) _this->_home_limit = true;
    if(!fl && dir) _this->_feed_limit = true;
    if(_this->_home_limit || _this->_feed_limit) digitalWrite(EN_PIN, HIGH);  // disable Stepper
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
 * @brief Task function monitoring the blade state to enable air and collant when on auto....
 * 
 * @param args - task arguments
 */
void Motion::blade_monitor(void * args)
{
    BladeTaskArgs* _args = static_cast<BladeTaskArgs*>(args);
    Motion* _this = _args->self;
    Logger.Info(F("... Starting blade monitoring task"));
    
    for(;;)
    {
        if(_this->_blade_job_should_exit) break;
        vTaskDelay(pdMS_TO_TICKS(50));

        if(_this->get_blade_status() == BLADE_STATE::STOPPED)
        {
            if(_this->_air_state == AIR_STATE::AUTO && digitalRead(SOLENOID_A_PIN) == HIGH)
            {
                digitalWrite(SOLENOID_A_PIN, LOW);
                _args->callback(5, 2);
            } 
            if(_this->_coolant_state == COOLANT_STATE::AUTO && digitalRead(SOLENOID_B_PIN) == HIGH)
            {
                digitalWrite(SOLENOID_B_PIN, LOW); 
                _args->callback(7, 2);
            }
        }
        else 
        {
            if(_this->_air_state == AIR_STATE::AUTO && digitalRead(SOLENOID_A_PIN) == LOW) 
            {
                digitalWrite(SOLENOID_A_PIN, HIGH);
                _args->callback(5, 0);
            }
            if(_this->_coolant_state == COOLANT_STATE::AUTO && digitalRead(SOLENOID_B_PIN) == LOW)
            {   
                digitalWrite(SOLENOID_B_PIN, HIGH);
                _args->callback(7, 0);
            }  
        }
    }
    Logger.Info(F("... Blade monitoring task complete"));
    _this->_blade_task = NULL;
    delete _args;
    vTaskDelete(NULL);
}

/**
 * @brief Manages the air blast solenoid based on the switch state
 * 
 * @param gpio_on - the state of the always on switch
 * @param gpio_auto - the state of the auto switch
 * @param indicator_callback - callback function invoked by the monitoring job when in AUTO mode
 * to set the correct indicator in the UI.
 * @return air_state_t - the actual state of the solenoid after the operation
 */
air_state_t Motion::manage_air(uint8_t gpio_on, uint8_t gpio_auto, std::function<void(uint8_t, uint8_t)> indicator_callback)
{
    if(gpio_auto == LOW && gpio_on == LOW)
    {
        digitalWrite(SOLENOID_A_PIN, LOW);
        Logger.Info(F("... Air blast solendoid switched off"));

        if(this->_blade_task != NULL && this->_coolant_state != COOLANT_STATE::AUTO)
        {
            this->_blade_job_should_exit = true;
            while( this->_blade_task != NULL) vTaskDelay(pdMS_TO_TICKS(10));
        }

        this->_air_state = AIR_STATE::OFF;
    }
    if(gpio_on == HIGH)
    {
        if(this->_blade_task != NULL && this->_coolant_state != COOLANT_STATE::AUTO)
        {
            this->_blade_job_should_exit = true;
            while( this->_blade_task != NULL) vTaskDelay(pdMS_TO_TICKS(10));
        }

        if(this->_ems_state == EMS_STATE::SHUTDOWN)
        {
            Logger.Info(F("... Air blast activation request received, but EMS is active. Ignoring..."));
            this->_air_state = AIR_STATE::AUTO;   
        }
        digitalWrite(SOLENOID_A_PIN, HIGH);
        Logger.Info(F("... Air blast solendoid switched on"));
        this->_air_state = AIR_STATE::ON;
    }
    if(gpio_auto)
    {
        digitalWrite(SOLENOID_A_PIN, LOW);
        Logger.Info(F("... Air blast solendoid switched to auto. Monitoring has started"));

        if(this->_blade_task == NULL) 
        {
            this->_blade_job_should_exit = false;
            BladeTaskArgs *args = new BladeTaskArgs{ this, indicator_callback };
            xTaskCreatePinnedToCore(blade_monitor, "Blade Monitor", 2048, args, 1, &_blade_task, 1);
        }

        this->_air_state =  AIR_STATE::AUTO;       
    }
    return this->_air_state;
}

/**
 * @brief Manages the coolant solenoid based on the switch state
 * 
 * @param gpio_on - the state of the always on switch
 * @param gpio_auto - the state of the auto switch
 * @param indicator_callback - callback function invoked by the monitoring job when in AUTO mode
 * to set the correct indicator in the UI.
 * @return coolant_state_t - the actual state of the solenoid after the operation
 */
coolant_state_t Motion::manage_coolant(uint8_t gpio_on, uint8_t gpio_auto, std::function<void(uint8_t, uint8_t)> indicator_callback)
{
    if(gpio_auto == LOW && gpio_on == LOW)
    {
        digitalWrite(SOLENOID_B_PIN, LOW);
        Logger.Info(F("... Coolant solendoid switched off"));

        if(this->_blade_task != NULL && this->_air_state != AIR_STATE::AUTO)
        {
            this->_blade_job_should_exit = true;
            while( this->_blade_task != NULL) vTaskDelay(pdMS_TO_TICKS(10));
        }

        this->_coolant_state = COOLANT_STATE::OFF;
    }
    if(gpio_on == HIGH)
    {
        if(this->_blade_task != NULL && this->_air_state != AIR_STATE::AUTO)
        {
            this->_blade_job_should_exit = true;
            while( this->_blade_task != NULL) vTaskDelay(pdMS_TO_TICKS(10));
        }

        if(this->_ems_state == EMS_STATE::SHUTDOWN)
        {
            Logger.Info(F("... Coolant activation request received, but EMS is active. Ignoring..."));
            this->_coolant_state = COOLANT_STATE::AUTO;  
            return this->_coolant_state; 
        }
        
        digitalWrite(SOLENOID_B_PIN, HIGH);
        Logger.Info(F("... Coolant solendoid switched on"));
        this->_coolant_state = COOLANT_STATE::ON;
    }
    if(gpio_auto)
    {
        digitalWrite(SOLENOID_B_PIN, LOW);
        Logger.Info(F("... Coolant solendoid switched to auto. Monitoring has started"));

        if(this->_blade_task == NULL) 
        {
            this->_blade_job_should_exit = false;
            BladeTaskArgs *args = new BladeTaskArgs{ this, indicator_callback };
            xTaskCreatePinnedToCore(blade_monitor, "Blade Monitor", 2048, args, 1, &_blade_task, 1);
        }
        this->_coolant_state = COOLANT_STATE::AUTO;       
    }
    return this->_coolant_state;
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
        this->_frequency = 0;
    }
    else
    {
        this->_frequency = 0;
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
    uint16_t i=0;
    uint16_t _s = _this->_frequency;
    bool startup_complete = false;

    Logger.Info(F("... Starting homing task"));
    _this->_state = MOTION_STATE::HOMING;
    _this->_frequency = FREQUENCY_HOME_START;

    esp_err_t r = ledc_channel_config(&_channel_config);
    if(r != ESP_OK) Logger.Error_f(F("PWM channel configuration failed with 0x%04X"), r);
    r = ledc_set_freq(LEDC_HIGH_SPEED_MODE, LEDC_TIMER_0, _this->_frequency); 
    if(r != ESP_OK) Logger.Error_f(F("PWM frequency configuration failed with 0x%04X"), r);
            // enable PWM to drive the stepper

    Logger.Info_f(F("...   PWM freq: %u"), ledc_get_freq(LEDC_HIGH_SPEED_MODE, LEDC_TIMER_0));
    digitalWrite(DIR_PIN, LOW);         // set direction towards home
    digitalWrite(EN_PIN, LOW);          // enable stepper
    while(_this->_home_limit == false)
    {
        if(_this->_ems_state == EMS_STATE::SHUTDOWN)  break;
        if(_this->_frequency < FREQUENCY_HOME && !startup_complete) 
        {
            _this->_frequency += FREQUENCY_HOME_INC;
            if(_this->_frequency >= FREQUENCY_HOME) 
            {
                _this->_frequency = FREQUENCY_HOME;
                startup_complete = true;
            }
            ledc_set_freq(LEDC_HIGH_SPEED_MODE, LEDC_TIMER_0, _this->_frequency); 
        }
        if(_this->_job_should_exit) break;

        vTaskDelay(50);                     // delay 100ms. There is no risk here as the interrupt handler will
                                            // disable the stepper as soon as the home limit has been hit. 
        i++;
    } 

    digitalWrite(EN_PIN, HIGH);             // disable the stepper in case we terminated due to EMS or user termination.


    r = ledc_set_freq(LEDC_HIGH_SPEED_MODE, LEDC_TIMER_0, _s < FREQUENCY_MIN ? FREQUENCY_MIN : _s); 
    if(r != ESP_OK) Logger.Error_f(F("PWM frequency configuration failed with 0x%04X"), r);
    r = ledc_stop(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, 0);
    pinMode(STEP_PIN, OUTPUT); 
                // Stop LEDC PWM for now, force STEP low this allow for manual control. 
                // we are only using PWM feed for homing and feeding. 
    if(r != ESP_OK) Logger.Error_f(F("PWM channel pausing failed with 0x%04X"), r);
    _this->_frequency = 0;

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
 * @brief Creates a manual step pulse for the stepper. Used for manual operation
 * 
 * @param dir - direction to move the stepper in. True to step into the feed.
 */
void Motion::step(bool dir)
{
    //static uint64_t last_pulse = esp_timer_get_time();
    static TimerHandle_t timer = NULL;
    static Motion* _this = nullptr;

    if(this->_state != MOTION_STATE::IDLE)  return;
                    // when we are not in idle state, we should not do anything here as 
                    // we are either autonomously feeding, homing, settings or in shutdown.

                               
    if(timer == NULL)
    {
        digitalWrite(EN_PIN, LOW);
        //this->_frequency = 0;
        _this = this;
        timer = xTimerCreate("JogTimeout", pdMS_TO_TICKS(5000), pdFALSE, this,
            [](TimerHandle_t t) 
            {
                if(_this->_state == MOTION_STATE::IDLE) digitalWrite(EN_PIN, HIGH); 
                _this->_steps_taken = 0;
                //_this->_frequency = 0;
                timer = NULL; 
                Logger.Info(F("... Stepper disabled due to idle timout."));
            });
    }
    else
    {
        xTimerReset(timer, 0);
        //uint32_t period = esp_timer_get_time() - last_pulse;
        //float f = 1000000.0f / period;
        //this->_frequency = (this->_frequency * 0.8f) + (f * 0.2f);
    }
    //last_pulse = esp_timer_get_time();

    digitalWrite(DIR_PIN, dir);
    digitalWrite(STEP_PIN, HIGH);
    delayMicroseconds(350);
    digitalWrite(STEP_PIN, LOW);
    delayMicroseconds(350);
    this->_steps_taken++;
    //Logger.Info_f(F("... Manual pulse %u"), this->_frequency);
}

/**
 * @brief Task function performing manual movement based on the wheel motion
 * 
 * @param args - pointer to task arguments 
 */
void Motion::manual_pulse_runner(void * args)
{
    Motion* _this = static_cast<Motion*>(args);
    Logger.Info(F("... Start manual pulse runner task"));
    for(;;)
    {
        if(_this->_queued_steps > 0)
        {
            _this->step(true);
            _this->_queued_steps --;
        }
        else if(_this->_queued_steps < 0)
        {
            _this->step(false);
            _this->_queued_steps ++;
        }
        if(_this->_queued_steps == 0) taskYIELD();
    }
    _this->_pulse_task = NULL;
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
    const float microsteps_per_rev = MOTOR_STEPS_PER_REV * (MOTOR_MICROSTEPS == 0 ? 1 : MOTOR_MICROSTEPS);
    const float screw_rev_per_microstep = GEAR_RATIO / microsteps_per_rev;
    const float mm_per_microstep = screw_rev_per_microstep * LEADSCREW_LEAD_MM;

    if(this->_state == MOTION_STATE::HOMING || this->_state == MOTION_STATE::FEEDING)
    {
        const float mm_per_min = this->_frequency * mm_per_microstep * 60.0f;
        return mm_per_min / MM_PER_INCH;  
    }
    else
    {
        const uint32_t elapsed = esp_timer_get_time() - this->_time_stamp;
        const float mm_per_sec = (this->_steps_taken * mm_per_microstep * 1000000.0f ) / elapsed;
        if(elapsed > 5000000) this->_steps_taken = 0;
        return 60.0f * mm_per_sec / MM_PER_INCH;
    }
}

/**
 * @brief Processes wheel movement events and takes the appropriate actions depending on hte motion state.
 * @param direction - the direction of the wheel movement.
 * @param steps - the number of steps moved.
 */
void Motion::process_wheel_movement(int direction, int steps) 
{ 
    if(this->_state == MOTION_STATE::SHUTDOWN) return;
    if(this->_state == MOTION_STATE::SETTINGS) 
    {
        // TODO - whatever we need to do during settings management. 
        Logger.Info_f(F("Wheel change: position %d, direction %d"), steps, direction);
    }
    if(this->_state == MOTION_STATE::HOMING || this->_state == MOTION_STATE::FEEDING)
    {
        // when homing or feeding, the wheel will increase and decrease the 
        // homing speed....
        uint16_t f = this->_frequency + direction * FREQUENCY_INCREMENT;
        if(f < FREQUENCY_MIN) f = FREQUENCY_MIN;
        if(f > FREQUENCY_MAX) f = FREQUENCY_MAX;
        if(f != this->_frequency)
        {
            this->_frequency = f;
            ledc_set_freq(LEDC_HIGH_SPEED_MODE, LEDC_TIMER_0, this->_frequency);
            Logger.Info_f(F("... Manual pulse %u"), ledc_get_freq(LEDC_HIGH_SPEED_MODE, LEDC_TIMER_0));
        } 
    }
    if(this->_state == MOTION_STATE::IDLE)
    {
        // when idle the wheel will manually feed the carriage in that case, we will 
        // need to generate pulses in sync with the wheel motion. 
        if(this->_steps_taken == 0) this->_time_stamp = esp_timer_get_time();
        this->_queued_steps += direction > 0  ? 10 :  -10;
    }
}