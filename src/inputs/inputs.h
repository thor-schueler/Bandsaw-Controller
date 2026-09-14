// Copyright (c) Thor Schueler. All rights reserved.
// SPDX-License-Identifier: MIT

#ifndef _INPUTS_H_
#define _INPUTS_H_

#include "Arduino.h"
#include <functional>
#include "PCF8575.h"
#include "../logging/SerialLogger.h"

#define EXPANDER_IRQ_PIN 34
#define EXPANDER_I2C_SDA_PIN 27
#define EXPANDER_I2C_SCL_PIN 26

#define PCF8575_ADDRESS 0x20
#define PCF8575_SDA_PIN 26
#define PCF8575_SCL_PIN 27
#define PCF8575_INT_PIN 34

#define WHEEL_A 36
#define WHEEL_B 39

#define EXT_GPIO_START_PIN  1
#define EXT_GPIO_ENGAGE_PIN 0
#define EXT_GPIO_HOME_PIN 8
#define EXT_GPIO_LUBE_AUTO 7
#define EXT_GPIO_LUBE_ON 6
#define EXT_GPIO_AIR_AUTO 5
#define EXT_GPIO_AIR_ON 4
#define EXT_GPIO_LIGHT_WARM 3
#define EXT_GPIO_LIGHT_COLD 2
#define EXT_GPIO_EMS 15

class Inputs {

  public: 
    
    /**
     * @brief Construct a new Inputs object
     * 
     */
    Inputs();

    /** 
     * @brief Destructs an Inputs object and cleans up resources
     * 
     */
    ~Inputs();

    /**
     * @brief Initialize teh GPIO extender, configure interrupts and start monitoring
     * 
     */    
    void begin();

  private: 

    /**
     * @brief Task function to process changes to the inputs on the PCF8575
     * GPIO extender. This funtions runs an endless blocking loop, waiting for notification 
     * from on_PCF8575_input_changed upon which it evaluates the inputs and performs
     * the appropriate actions. 
     * @param args - pointer to task arguments
     */
    static void extended_GPIO_watcher(void* args);

    /**
     * @brief Event handler handling input change events on the PCF8575 
     *
     */
    static void on_PCF8575_input_changed();

    inline static PCF8575* pcf8575 = NULL;
    inline static TaskHandle_t extendedGPIOWatcher = NULL;

};


#endif // _INPUTS_H_