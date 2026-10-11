// Copyright (c) Thor Schueler. All rights reserved.
// SPDX-License-Identifier: MIT

#include "inputs.h"

uint8_t __instances = 0;

std::array<input_entry_t, 20> __inputs_main = {{
    { nullptr, nullptr, EXT_GPIO_ENGAGE_PIN, "Engage Feed", true },
    { nullptr, nullptr, EXT_GPIO_START_PIN, "Toggle Blade", true },
    { nullptr, nullptr, EXT_GPIO_LIGHT_COLD, "Active Cold Light", true },
    { nullptr, nullptr, EXT_GPIO_LIGHT_WARM, "Activate Warm Light", true },
    { nullptr, nullptr, EXT_GPIO_AIR_ON, "Air Blast Always On", true },
    { nullptr, nullptr, EXT_GPIO_AIR_AUTO, "Air Blast Auto", true  },
    { nullptr, nullptr, EXT_GPIO_LUBE_ON, "Coolant Always On", true  },
    { nullptr, nullptr, EXT_GPIO_LUBE_AUTO, "Coolant Auto", true  },
    { nullptr, nullptr, EXT_GPIO_HOME_PIN, "Homing", true  },
    { nullptr, nullptr, 9, "", true },
    { nullptr, nullptr, 10, "", true },
    { nullptr, nullptr, 11, "", true },
    { nullptr, nullptr, 12, "", true },
    { nullptr, nullptr, 13, "", true },
    { nullptr, nullptr, 14, "", true },
    { nullptr, nullptr, EXT_GPIO_EMS, "Emergency Shutdown", false},
    { nullptr, nullptr, 16, "Settings", true},
    { nullptr, nullptr, 17, "Alerts", true},
    { nullptr, nullptr, 18, "", true},
    { nullptr, nullptr, 19, "", true}
}};

std::array<input_entry_t, 4> __inputs_alerts = {{
    { nullptr, nullptr, 0, "Clear Alerts", true },
    { nullptr, nullptr, 1, "Previous", true },
    { nullptr, nullptr, 2, "Next", true },
    { nullptr, nullptr, 3, "Return Home", true }
}};

std::array<input_entry_t, 5> __inputs_settings = {{
    { nullptr, nullptr, 0, "", true },
    { nullptr, nullptr, 1, "", true },
    { nullptr, nullptr, 2, "", true },
    { nullptr, nullptr, 3, "", true },
    { nullptr, nullptr, 4, "", true }
}};

std::array<input_entry_t, 5> __inputs_ems = {{
    { nullptr, nullptr, 0, "", true }
}};


inputs_t __inputs [] = 
{
    [0] = {__inputs_main.data(), __inputs_main.size(), 17 },
    [1] = {__inputs_alerts.data(), __inputs_alerts.size(), 0},
    [2] = {__inputs_settings.data(), __inputs_settings.size(), 0},
    [3] = {__inputs_ems.data(), __inputs_settings.size(), 0}
};

/**
 * @brief Construct a new Inputs object
 * 
 */
Inputs::Inputs()
{
    if(Inputs::_pcf8575 == NULL) Inputs::_pcf8575 = new PCF8575(PCF8575_ADDRESS, PCF8575_SDA_PIN, PCF8575_SCL_PIN);
    _wheel_callbacks.reserve(2);
    __instances++;
}

/** 
 * @brief Destructs an Inputs object and cleans up resources
 * 
 */
Inputs::~Inputs()
{
    __instances--;
    if(this->_wheelRunner != NULL)
    {
        this->_wheelRunner_stop_requested.store(true, std::memory_order_release);
        this->request_task_stop(this->_wheelRunner, this->_wheelRunner_stop_requested, "wheelRunner", 2000);
        this->_wheelRunner = NULL;
    }

    if(this->_extendedGPIOWatcher != NULL)
    {
        this->_extended_gpio_watcher_stop_requested.store(true, std::memory_order_release);
        this->request_task_stop(this->_extendedGPIOWatcher, this->_extended_gpio_watcher_stop_requested, "extendedGPIOWatcher", 2000);
        this->_extendedGPIOWatcher = NULL;
    }

    if(__instances == 0)
    {
        if(Inputs::_pcf8575 != NULL) delete Inputs::_pcf8575;
        Inputs::_pcf8575 = NULL;
    }
}

/**
 * @brief Requests a task to stop and waits for it to acknowledge exit.
 * 
 * The task owns the actual shutdown path and clears its handle only after it has
 * exited. This prevents stale task handles and race-prone polling on a handle that may
 * be invalid while the task is still running.
 */
void Inputs::request_task_stop(TaskHandle_t& task_handle, std::atomic<bool>& stop_flag, const char* task_name, uint32_t timeout_ms)
{
    if(task_handle == NULL) return;

    stop_flag.store(true, std::memory_order_release);
    xTaskNotifyGive(task_handle);           // important to unblock a blocked task. Note that the task logic
                                            // needs to reflect the fact that the unblock might 
                                            // come from this request and process the stop and exit before
                                            // normal flow control logic. 
    const uint32_t start_ms = millis();
    while(task_handle != NULL && (millis() - start_ms) < timeout_ms)
    {
        if(eTaskGetState(task_handle) == eDeleted)
        {
            task_handle = NULL;
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    if(task_handle != NULL)
    {
        Logger.Info_f(F("... Inputs: timed out waiting for %s task to exit; task owns shutdown path and retains control."), task_name);
    }
}

/**
 * @brief Logs the usage of the various task stacks to the console
 * 
 */
void Inputs::log_stack_usage()
{
    if(this->_extendedGPIOWatcher != NULL) Logger.Info_f(F("Inputs: Extended GPIO Watcher High Water Mark: %u"), uxTaskGetStackHighWaterMark(this->_extendedGPIOWatcher));
    if(this->_wheelRunner != NULL)         Logger.Info_f(F("Inputs: Wheel runner High Water Mark:          %u"), uxTaskGetStackHighWaterMark(this->_wheelRunner));
}


/**
 * @brief Initialize teh GPIO extender, configure interrupts and start monitoring
 * 
 */    
void Inputs::begin()
{
    Logger.Info(F("... Begin input controller execution."));
    Logger.Info(F("...   Setup Extended GPIO"));
    for(int i=0; i<16; i++) Inputs::_pcf8575->pinMode(i, INPUT);
    Inputs::_pcf8575->begin();

    Logger.Info(F("...   Setup GPIO pins"));
    pinMode(WHEEL_A, INPUT);
    pinMode(WHEEL_B, INPUT);

    Logger.Info(F("...   Attach event receivers for GPIO"));
    attachInterruptArg(digitalPinToInterrupt(WHEEL_A), Inputs::handle_encoder_change, this, CHANGE);
    attachInterruptArg(digitalPinToInterrupt(WHEEL_B), Inputs::handle_encoder_change, this, CHANGE);
    attachInterruptArg(PCF8575_INT_PIN, [](void *arg){
        Inputs *_this = static_cast<Inputs *>(arg); _this->on_PCF8575_input_changed();
    }, this, FALLING);

    Logger.Info(F("...   Create various tasks"));
    _wheelRunner_stop_requested.store(false, std::memory_order_release);
    xTaskCreatePinnedToCore(wheel_runner, "wheelRunner", 2560, this, 1, &_wheelRunner, 0);

    Logger.Info(F("...   Done."));
}

/**
 * @brief Starts the extended GPIO monitoring task
 * 
 */
void Inputs::start_monitoring()
{
    if(Inputs::_extendedGPIOWatcher == NULL)
    {
        Logger.Info(F("...   Configure Extended GPIO monitoring task"));
        Inputs::_extended_gpio_watcher_stop_requested.store(false, std::memory_order_release);
        xTaskCreatePinnedToCore(extended_GPIO_watcher, "extendedGPIOWatcher", 4096, this, 1, &_extendedGPIOWatcher, 0);
    }
}

/**
 * @brief Pauses Monitoring for all commands that are allowed to pause
 * 
 */
void Inputs::pause_monitoring() { this->_pause.store(true, std::memory_order_release); }

/**
 * @brief Resumes Monitoring for commands that are paused
 * 
 */    
void Inputs::resume_monitoring() { this->_pause.store(false, std::memory_order_release); }



/**
 * @brief Get the inputs object for a screen
 * @param screen - the screen for which to return the inputs 
 * 
 * @return reference to a std::array of input_entry_t types.  
 */
inputs_t& Inputs::get_inputs(uint8_t screen) {  return __inputs[screen]; }

/**
 * @brief Registers a command for a specific GPIO
 * 
 * @param screen - the screen index for which to register the command 
 * @param gpio   - the gpio that will invoke the command 
 * @param entry  - the function to call when the GPIO goes active (low). NULL if no function should be called.
 * @param exit   - the finction to call when the GPIO goes inactuve (high). NULL if no function should be called.
 * @param allow_pause - the command monitoring can be paused for this command
 */
void Inputs::register_command(uint8_t screen, uint8_t gpio, std::function<void(uint8_t gpio, const char*)> entry, std::function<void(uint8_t gpio, const char*)> exit, bool allow_pause)
{
    if(gpio < 0 || gpio > __inputs[screen].max_inputs-1)
    {   
        Logger.Error_f(F("... GPIO %d is invalid. Should be between 0 and %d. Ignoring...."), gpio, __inputs[screen].max_inputs-1);
        return;
    }
    __inputs[screen].inputs[gpio].entry = entry;
    __inputs[screen].inputs[gpio].exit = exit;
    __inputs[screen].inputs[gpio].ext_gpio = gpio;
    __inputs[screen].inputs[gpio].can_be_paused = allow_pause;
}

/**
 * @brief Registers a command for a specific GPIO
 * 
 * @param screen - the screen index for which to register the command
 * @param gpio  - the gpio that will invoke the command 
 * @param entry - the function to call when the GPIO goes active (low). NULL if no function should be called.
 * @param exit  - the finction to call when the GPIO goes inactuve (high). NULL if no function should be called.
 * @param cmd   - the title of the command. 
 * @param allow_pause - the command monitoring can be paused for this command
 */
void Inputs::register_command(uint8_t screen, uint8_t gpio, std::function<void(uint8_t gpio, const char*)> entry, std::function<void(uint8_t gpio, const char*)> exit, String cmd, bool allow_pause)
{
    if(gpio < 0 || gpio > __inputs[screen].max_inputs-1)
    {   
        Logger.Error_f(F("... GPIO %d is invalid. Should be between 0 and %d. Ignoring...."), gpio, __inputs[screen].max_inputs-1);
        return;
    }
    __inputs[screen].inputs[gpio].entry = entry;
    __inputs[screen].inputs[gpio].exit = exit;
    __inputs[screen].inputs[gpio].command = cmd;
    __inputs[screen].inputs[gpio].ext_gpio = gpio;
    __inputs[screen].inputs[gpio].can_be_paused = allow_pause;
}


/**
 * @brief Task function to process changes to the inputs on the PCF8575
 * GPIO extender. This funtions runs an endless blocking loop, waiting for notification 
 * from on_PCF8575_input_changed upon which it evaluates the inputs and performs
 * the appropriate actions. 
 * @param args - pointer to task arguments
 */
void Inputs::extended_GPIO_watcher(void* args)
{
    uint16_t button_state = 0x0000;
    Inputs *_this = reinterpret_cast<Inputs *>(args);
    Logger.Info(F("... Extended GPIO Watcher has started."));
    for (;;) 
    {
        if (_this->_extended_gpio_watcher_stop_requested.load(std::memory_order_acquire))
        {
            Logger.Info(F("... Extended GPIO Watcher shutdown requested."));
            break;
        }

        PCF8575::DigitalInput di = Inputs::_pcf8575->digitalReadAll();
        uint16_t bs = 0x0000;
        bs |= (!di.p0 & 0x01) << 0; 
        bs |= (!di.p1 & 0x01) << 1; 
        bs |= (!di.p2 & 0x01) << 2; 
        bs |= (!di.p3 & 0x01) << 3; 
        bs |= (!di.p4 & 0x01) << 4; 
        bs |= (!di.p5 & 0x01) << 5; 
        bs |= (!di.p6 & 0x01) << 6; 
        bs |= (!di.p7 & 0x01) << 7; 
        bs |= (!di.p8 & 0x01) << 8; 
        bs |= (!di.p9 & 0x01) << 9; 
        bs |= (!di.p10 & 0x01) << 10; 
        bs |= (!di.p11 & 0x01) << 11; 
        bs |= (!di.p12 & 0x01) << 12; 
        bs |= (!di.p13 & 0x01) << 13; 
        bs |= (!di.p14 & 0x01) << 14;
        bs |= (!di.p15 & 0x01) << 15;  

        if(bs != button_state)
        {            
            uint16_t changed = button_state ^ bs;
            uint16_t turned_on = ~button_state & bs;
            uint16_t turned_off = button_state & ~bs;
            button_state = bs;
            
            for (int i = 15; i >= 0; --i)
            { 
                if (__inputs[0].inputs[i].can_be_paused && _this->_pause) continue;
                if (turned_on & (1u << i) && __inputs[0].inputs[i].entry != nullptr) __inputs[0].inputs[i].entry(__inputs[0].inputs[i].ext_gpio, __inputs[0].inputs[i].command.c_str());
                if (turned_off & (1u << i) && __inputs[0].inputs[i].exit != nullptr) __inputs[0].inputs[i].exit(__inputs[0].inputs[i].ext_gpio, __inputs[0].inputs[i].command.c_str());
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
        
        // Wait for the notification to come from the event handler
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    }
    _this->_extendedGPIOWatcher = NULL;
    _this->_extended_gpio_watcher_stop_requested.store(false, std::memory_order_release);
    Logger.Info(F("... Extended GPIO Watcher has stopped."));
    vTaskDelete(NULL);
}

/**
 * @brief Task function managing wheel movements. This task runs an endless blocking loop,
 * waiting for notification from handle_encoder_change upon which it will process
 * and execute the appropriate action.
 * @param args - pointer to task arguments 
 */
void Inputs::wheel_runner(void* args)
{
    Inputs *_this = reinterpret_cast<Inputs *>(args);
    Logger.Info(F("... Wheel Runner task has started."));
    for (;;) 
    {
        if (_this->_wheelRunner_stop_requested.load(std::memory_order_acquire))
        {
            Logger.Info(F("... Wheel Runner shutdown requested."));
            break;
        }
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (_this->_wheelRunner_stop_requested.load(std::memory_order_acquire))
        {
            Logger.Info(F("... Wheel Runner shutdown requested."));
            break;
        }

        // this section is executed for every wheel position change.
        for (auto& callback : _this->_wheel_callbacks) callback(_this->_direction, _this->_wheel_position);
    }
    _this->_wheelRunner = NULL;
    _this->_wheelRunner_stop_requested.store(false, std::memory_order_release);
    Logger.Info(F("... Wheel Runner task complete."));
    vTaskDelete(NULL);
}

/**
 * @brief Event handler handling input change events on the PCF8575 
 */
void IRAM_ATTR Inputs::on_PCF8575_input_changed()
{
    static std::atomic<uint32_t> last_input_debounce_ms{0};

    // debounce check to prevent double button presses. The PCF8575 can be a bit noisy and this has been 
    // found to be a reliable way to prevent it.
    BaseType_t xHigherPriorityTaskToken = pdFALSE;
    const uint32_t currentTime = millis();
    uint32_t lastDebounceTime = last_input_debounce_ms.load(std::memory_order_relaxed);

    if ((currentTime - lastDebounceTime) > 250 && this->_extendedGPIOWatcher != NULL)
    {
        if (last_input_debounce_ms.compare_exchange_weak(lastDebounceTime, currentTime, std::memory_order_relaxed, std::memory_order_relaxed))
        {
            vTaskNotifyGiveFromISR(this->_extendedGPIOWatcher, &xHigherPriorityTaskToken);
            portYIELD_FROM_ISR(xHigherPriorityTaskToken);
        }
    }
}

/**
 * @brief Event handler watching the Quadradure encoder GPIOs.
 * @param arg - argumnent passed to the handler, expected to be the instance of the calling object and can 
 * be cast to Inputs*
 */
void IRAM_ATTR Inputs::handle_encoder_change(void* arg)
{
    Inputs *_this = reinterpret_cast<Inputs *>(arg);
    static int8_t c = 0;
    static const int8_t enconder_state_table[16] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};
    int MSB = digitalRead(WHEEL_A); // Most significant bit 
    int LSB = digitalRead(WHEEL_B); // Least significant bit 
    int encoded = (MSB << 1) | LSB; // Combine the two signals 
    if(encoded != _this->_wheel_encoded)
    {
        int sum = (_this->_wheel_encoded << 2) | encoded;  // Add the two previous bits 
        c += enconder_state_table[sum];
        if(c == 4 || c == -4)
        {
            _this->_wheel_position += c == 4 ? 1 : -1;
            _this->_direction = c > 0 ? 1 : -1;
            c = 0x0;
            _this->_wheel_encoded = encoded;   // Update the last encoded value

            // Signal our job to run the axis....
            BaseType_t xHigherPriorityTaskToken = pdFALSE;
            vTaskNotifyGiveFromISR(_this->_wheelRunner, &xHigherPriorityTaskToken); 
            portYIELD_FROM_ISR(xHigherPriorityTaskToken);
        }
        else
        {
            _this->_wheel_encoded = encoded;   // Update the last encoded value  
        }
    }
}

/**
 * @brief Reads a specific GPIO on the PCF8575 extender.
 * 
 * @param gpio - GPIO to read
 * @return uint8_t - the state of the GPIO
 */
uint8_t Inputs::digitalReadEx(uint8_t gpio)
{
    return this->_pcf8575->digitalRead(gpio, true);
}