// Copyright (c) Thor Schueler. All rights reserved.
// SPDX-License-Identifier: MIT

#include "inputs.h"

uint8_t __instances = 0;

std::array<input_entry_t, 16> __inputs = {{
    { nullptr, nullptr, EXT_GPIO_ENGAGE_PIN, "Engage Feed" },
    { nullptr, nullptr, EXT_GPIO_START_PIN, "Toggle Blade" },
    { nullptr, nullptr, EXT_GPIO_LIGHT_COLD, "Active Cold Light" },
    { nullptr, nullptr, EXT_GPIO_LIGHT_WARM, "Activate Warm Light" },
    { nullptr, nullptr, EXT_GPIO_AIR_ON, "Air Blast Always On" },
    { nullptr, nullptr, EXT_GPIO_AIR_AUTO, "Air Blast Auto" },
    { nullptr, nullptr, EXT_GPIO_LUBE_ON, "Coolant Always On" },
    { nullptr, nullptr, EXT_GPIO_LUBE_AUTO, "Coolant Auto" },
    { nullptr, nullptr, EXT_GPIO_HOME_PIN, "Homing" },
    { nullptr, nullptr, 9, "" },
    { nullptr, nullptr, 10, "" },
    { nullptr, nullptr, 11, "" },
    { nullptr, nullptr, 12, "" },
    { nullptr, nullptr, 13, "" },
    { nullptr, nullptr, 14, "" },
    { nullptr, nullptr, EXT_GPIO_EMS, "Emergency Shutdown" }
}};

/**
 * @brief Construct a new Inputs object
 * 
 */
Inputs::Inputs()
{
    if(Inputs::_pcf8575 == NULL) Inputs::_pcf8575 = new PCF8575(PCF8575_ADDRESS, PCF8575_SDA_PIN, PCF8575_SCL_PIN, PCF8575_INT_PIN, Inputs::on_PCF8575_input_changed);
    __instances++;
}

/** 
 * @brief Destructs an Inputs object and cleans up resources
 * 
 */
Inputs::~Inputs()
{
    __instances--;
    if(__instances == 0)
    {
        if(Inputs::_pcf8575 != NULL) delete Inputs::_pcf8575;
        if(Inputs::_extendedGPIOWatcher != NULL)
        {
            vTaskDelete(Inputs::_extendedGPIOWatcher); 
            Inputs::_extendedGPIOWatcher = NULL; 
        }
    }
    if(this->_wheelRunner != NULL)
    {
        vTaskDelete(this->_wheelRunner); 
        this->_wheelRunner = NULL; 
    }
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
    Inputs:_pcf8575->begin();

    if(Inputs::_extendedGPIOWatcher == NULL)
    {
        Logger.Info(F("...   Configure Extended GPIO monitoring task"));
        xTaskCreatePinnedToCore(extended_GPIO_watcher, "extendedGPIOWatcher", 2048, this, 1, &Inputs::_extendedGPIOWatcher, 0);
    }

    Logger.Info(F("...   Setup GPIO pins"));
    pinMode(WHEEL_A, INPUT);
    pinMode(WHEEL_B, INPUT);

    Logger.Info(F("...   Attach event receivers for GPIO"));
    attachInterruptArg(digitalPinToInterrupt(WHEEL_A), Inputs::handle_encoder_change, this, CHANGE);
    attachInterruptArg(digitalPinToInterrupt(WHEEL_B), Inputs::handle_encoder_change, this, CHANGE);

    Logger.Info(F("...   Create various tasks"));
    xTaskCreatePinnedToCore(wheel_runner, "wheelRunner", 2560, this, 1, &_wheelRunner, 0);
    //xTaskCreatePinnedToCore(ems_change_runner, "emsRunner", 1560, this, 1, &_emsChangeRunner, 0);

    Logger.Info(F("...   Done."));
}

/**
 * @brief Get the inputs object
 * 
 * @return reference to a std::array of input_entry_t types.  
 */
std::array<input_entry_t, 16>& Inputs::get_inputs()
{
    return __inputs;
}

/**
 * @brief Registers a command for a specific GPIO
 * 
 * @param gpio  - the gpio that will invoke the command 
 * @param entry - the function to call when the GPIO goes active (low). NULL if no function should be called.
 * @param exit  - the finction to call when the GPIO goes inactuve (high). NULL if no function should be called.
 */
void Inputs::register_command(uint8_t gpio, std::function<void(uint8_t gpio, const char*)> entry, std::function<void(uint8_t gpio, const char*)> exit)
{
    if(gpio < 0 || gpio > 15)
    {   
        Logger.Error_f(F("... GPIO %d is invalid. Should be between 0 and 15. Ignoring...."), gpio);
        return;
    }
    __inputs[gpio].entry = entry;
    __inputs[gpio].exit = exit;
    __inputs[gpio].ext_gpio = gpio;
}

/**
 * @brief Registers a command for a specific GPIO
 * 
 * @param gpio  - the gpio that will invoke the command 
 * @param entry - the function to call when the GPIO goes active (low). NULL if no function should be called.
 * @param exit  - the finction to call when the GPIO goes inactuve (high). NULL if no function should be called.
 * @param cmd   - the title of the command. 
 */
void Inputs::register_command(uint8_t gpio, std::function<void(uint8_t gpio, const char*)> entry, std::function<void(uint8_t gpio, const char*)> exit, String cmd)
{
    if(gpio < 0 || gpio > 15)
    {   
        Logger.Error_f(F("... GPIO %d is invalid. Should be between 0 and 15. Ignoring...."), gpio);
        return;
    }
    __inputs[gpio].entry = entry;
    __inputs[gpio].exit = exit;
    __inputs[gpio].command = cmd;
    __inputs[gpio].ext_gpio = gpio;
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
            
            String s;
            for (int i = 15; i >= 0; --i)
            { 
                s += (button_state & (1u << i)) ? '1' : '0';
                if (turned_on & (1u << i) && __inputs[i].entry != nullptr) __inputs[i].entry(__inputs[i].ext_gpio, __inputs[i].command.c_str());
                if (turned_off & (1u << i) && __inputs[i].exit != nullptr) __inputs[i].exit(__inputs[i].ext_gpio, __inputs[i].command.c_str());
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
        
        // Wait for the notification to come from the event handler
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    }
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
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        // this section is executed for every wheel position change.
        Logger.Info_f(F("Wheel change: position %d, direction %d"), _this->_wheel_position, _this->_direction);
    }
}


/**
 * @brief Event handler handling input change events on the PCF8575 
 *
 */
void IRAM_ATTR Inputs::on_PCF8575_input_changed()
{
    // debounce check to prevent double button presses. The PCF8575 can be a bit noisy and this has been 
    // found to be a reliable way to prevent it.
    BaseType_t xHigherPriorityTaskToken = pdFALSE;
    volatile uint32_t lastDebounceTime = 0; // Last debounce time volatile 
    uint32_t currentTime = millis(); 

    if ((currentTime - lastDebounceTime) > 250 && Inputs::_extendedGPIOWatcher != NULL) 
    {     
        vTaskNotifyGiveFromISR(Inputs::_extendedGPIOWatcher, &xHigherPriorityTaskToken); 
        portYIELD_FROM_ISR(xHigherPriorityTaskToken);
                
        // Update the last debounce time 
        lastDebounceTime = currentTime; 
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