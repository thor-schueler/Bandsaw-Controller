// Copyright (c) Thor Schueler. All rights reserved.
// SPDX-License-Identifier: MIT

#ifndef _INPUTS_H_
#define _INPUTS_H_

#include "Arduino.h"
#include <functional>
#include "PCF8575.h"
#include "src/logging/SerialLogger.h"

#define EXPANDER_IRQ_PIN 34
#define EXPANDER_I2C_SDA_PIN 27
#define EXPANDER_I2C_SCL_PIN 26

#define PCF8575_ADDRESS 0x20
#define PCF8575_SDA_PIN 26
#define PCF8575_SCL_PIN 27
#define PCF8575_INT_PIN 34

#define WHEEL_A 39
#define WHEEL_B 36

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

/**
 * @brief defines an input entry, which determines what methods to call when 
 * a GPIO turns on or off. 
 * 
 */
typedef struct input_entry
{
    std::function<void(uint8_t gpio, const char*)> entry;
    std::function<void(uint8_t gpio, const char*)> exit;
    int ext_gpio;
    String command;
    bool can_be_paused;

} input_entry_t;


/**
 * @brief This class is repsonsible for watching and processing the various inputs of the bandsaw 
 * and initiate the appropriate actions in the controller, display and motion modules.
 * 
 */
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
     * @brief Get the inputs object
     * 
     * @return reference to a std::array of input_entry_t types.  
     */
    static std::array<input_entry_t, 16>& get_inputs();

    /**
     * @brief Initialize teh GPIO extender, configure interrupts and start monitoring
     * 
     */    
    void begin();

    /**
     * @brief Starts the extended GPIO monitoring task
     * 
     */
    void start_monitoring();

    /**
     * @brief Pauses Monitoring for all commands that are allowed to pause
     * 
     */
    void pause_monitoring();

    /**
     * @brief Resumes Monitoring for commands that are paused
     * 
     */    
    void resume_monitoring();

    /**
     * @brief Registers a command for a specific GPIO
     * 
     * @param gpio  - the gpio that will invoke the command 
     * @param entry - the function to call when the GPIO goes active (low). NULL if no function should be called.
     * @param exit  - the function to call when the GPIO goes inactuve (high). NULL if no function should be called.
     * @param allow_pause - the command monitoring can be paused for this command
     */
    void register_command(uint8_t gpio, std::function<void(uint8_t gpio, const char*)> entry, std::function<void(uint8_t gpio, const char*)> exit, bool allow_pause);


    /**
     * @brief Registers a command for a specific GPIO
     * 
     * @param gpio  - the gpio that will invoke the command 
     * @param entry - the function to call when the GPIO goes active (low). NULL if no function should be called.
     * @param exit  - the finction to call when the GPIO goes inactuve (high). NULL if no function should be called.
     * @param cmd   - the title of the command. 
     * @param allow_pause - the command monitoring can be paused for this command
     */
    void register_command(uint8_t gpio, std::function<void(uint8_t gpio, const char*)> entry, std::function<void(uint8_t gpio, const char*)> exit, String cmd, bool allow_pause);

    /**
     * @brief Reads a specific GPIO on the PCF8575 extender.
     * 
     * @param gpio - GPIO to read
     * @return uint8_t - the state of the GPIO
     */
    uint8_t digitalReadEx(uint8_t gpio);


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
     * @brief Task function managing wheel movements. This task runs an endless blocking loop,
     * waiting for notification from handle_encoder_change upon which it will process
     * and execute the appropriate action.
     * @param args - pointer to task arguments 
     */
    static void wheel_runner(void* args);


    /**
     * @brief Event handler handling input change events on the PCF8575 
     *
     */
    static void IRAM_ATTR on_PCF8575_input_changed();

    /**
     * @brief Event handler watching the Quadradure encoder GPIOs.
     * @param arg - argumnent passed to the handler, expected to be the instance of the calling object and can 
     * be cast to Inputs*
     */
    static void IRAM_ATTR handle_encoder_change(void* arg);    

    inline static PCF8575* _pcf8575 = NULL;
    inline static TaskHandle_t _extendedGPIOWatcher = NULL;
    TaskHandle_t _wheelRunner;
    int16_t _wheel_encoded = 0x0;
    int16_t _wheel_position = 0x0;
    int8_t _direction = 0;
    bool _pause = false;

};


#endif // _INPUTS_H_