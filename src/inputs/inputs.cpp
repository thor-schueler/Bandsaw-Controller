// Copyright (c) Thor Schueler. All rights reserved.
// SPDX-License-Identifier: MIT

#include "inputs.h"

uint8_t __instances = 0;
input_entry_t __inputs[] = {
    { NULL, NULL, 0, "" },
    { NULL, NULL, 1, "" },
    { NULL, NULL, 2, "" },
    { NULL, NULL, 3, "" },
    { NULL, NULL, 4, "" },
    { NULL, NULL, 5, "" },
    { NULL, NULL, 6, "" },
    { NULL, NULL, 7, "" },
    { NULL, NULL, 8, "" },
    { NULL, NULL, 9, "" },
    { NULL, NULL, 10, "" },
    { NULL, NULL, 11, "" },
    { NULL, NULL, 12, "" },
    { NULL, NULL, 13, "" },
    { NULL, NULL, 14, "" },
    { NULL, NULL, 15, "" }
};


Inputs::Inputs()
{
    if(Inputs::pcf8575 == NULL) Inputs::pcf8575 = new PCF8575(PCF8575_ADDRESS, PCF8575_SDA_PIN, PCF8575_SCL_PIN, PCF8575_INT_PIN, Inputs::on_PCF8575_input_changed);
    __instances++;
}

Inputs::~Inputs()
{
    __instances--;
    if(__instances == 0)
    {
        if(Inputs::pcf8575 != NULL) delete Inputs::pcf8575;
        if(Inputs::extendedGPIOWatcher != NULL)
        {
            vTaskDelete(Inputs::extendedGPIOWatcher); 
            Inputs::extendedGPIOWatcher = NULL; 
        }
    }
}

/**
 * @brief Initialize teh GPIO extender, configure interrupts and start monitoring
 * 
 */    
void Inputs::begin()
{
    Logger.Info(F("... Setup Extended GPIO"));
    for(int i=0; i<16; i++) Inputs::pcf8575->pinMode(i, INPUT);
    Inputs::pcf8575->begin();

    if(Inputs::extendedGPIOWatcher == NULL)
    {
        Logger.Info(F("... Configure Extended GPIO monitoring task"));
        xTaskCreatePinnedToCore(extended_GPIO_watcher, "extendedGPIOWatcher", 2048, this, 1, &Inputs::extendedGPIOWatcher, 0);
    }
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
    bool isFirst = true;
    uint16_t button_state = 0x0000;
    Inputs *_this = reinterpret_cast<Inputs *>(args);
    Logger.Info(F("... Extended GPIO Watcher has started."));
    for (;;) 
    { 
        // Wait for the notification to come from the event handler
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        PCF8575::DigitalInput di = Inputs::pcf8575->digitalReadAll();
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

        if(bs != button_state || isFirst)
        {
            button_state = bs;
            isFirst=false;
            String s;
            for (int i = 15; i >= 0; --i) s += (button_state & (1u << i)) ? '1' : '0';
            Logger.Info_f(F("button state: %04X, %s"), button_state, s.c_str());
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/**
 * @brief Event handler handling input change events on the PCF8575 
 *
 */
void Inputs::on_PCF8575_input_changed()
{

    // debounce check to prevent double button presses. The PCF8575 can be a bit noisy and this has been 
    // found to be a reliable way to prevent it.
    BaseType_t xHigherPriorityTaskToken = pdFALSE;
    volatile uint32_t lastDebounceTime = 0; // Last debounce time volatile 
    uint32_t currentTime = millis(); 

    if ((currentTime - lastDebounceTime) > 250 && Inputs::extendedGPIOWatcher != NULL) 
    {     
        vTaskNotifyGiveFromISR(Inputs::extendedGPIOWatcher, &xHigherPriorityTaskToken); 
        portYIELD_FROM_ISR(xHigherPriorityTaskToken);
                
        // Update the last debounce time 
        lastDebounceTime = currentTime; 
    }
}