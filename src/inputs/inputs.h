// Copyright (c) Thor Schueler. All rights reserved.
// SPDX-License-Identifier: MIT

#ifndef _INPUTS_H_
#define _INPUTS_H_

#include "Arduino.h"
#include <functional>
#include <atomic>
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

typedef struct inputs 
{
    input_entry_t* inputs;
    uint8_t max_inputs;
    uint8_t max_phys_inputs;
} inputs_t;


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
     * @brief Get the inputs object for a screen.
     * @param screen - the screen for which to retrieve the inputs.
     * 
     * @return reference to a std::array of input_entry_t types.  
     */
    static inputs_t& get_inputs(uint8_t screen);

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
     * @param screen - the screen index for which to register the command 
     * @param gpio  - the gpio that will invoke the command 
     * @param entry - the function to call when the GPIO goes active (low). NULL if no function should be called.
     * @param exit  - the function to call when the GPIO goes inactuve (high). NULL if no function should be called.
     * @param allow_pause - the command monitoring can be paused for this command
     */
    void register_command(uint8_t screen, uint8_t gpio, std::function<void(uint8_t gpio, const char*)> entry, std::function<void(uint8_t gpio, const char*)> exit, bool allow_pause);


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
    void register_command(uint8_t screen, uint8_t gpio, std::function<void(uint8_t gpio, const char*)> entry, std::function<void(uint8_t gpio, const char*)> exit, String cmd, bool allow_pause);

    /**
     * @brief Registers a callback function for wheel events. There can be multiple callbacks registered.
     * @param callback - the function to call when wheel events occur. Callback is called with two parameters: 
     * the direction of the wheel movement and the number of steps moved.
     */
    void register_wheel_callback(const std::function<void(int, int)>& callback) { this->_wheel_callbacks.push_back(callback); };

    /**
     * @brief Requests a task to stop and waits for it to acknowledge exit.
     * 
     * The task owns the actual shutdown path and clears its handle only after it has
     * exited. This prevents stale task handles and race-prone polling on a handle that may
     * be invalid while the task is still running.
     */
    void request_task_stop(TaskHandle_t& task_handle, std::atomic<bool>& stop_flag, const char* task_name, uint32_t timeout_ms = 2000);

    /**
     * @brief Reads a specific GPIO on the PCF8575 extender.
     * 
     * @param gpio - GPIO to read
     * @return uint8_t - the state of the GPIO
     */
    uint8_t digitalReadEx(uint8_t gpio);

    /**
     * @brief Logs the usage of the various task stacks to the console
     * 
     */
    void log_stack_usage();    

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
     * @brief Event handler watching the Quadradure encoder GPIOs.
     * @param arg - argumnent passed to the handler, expected to be the instance of the calling object and can 
     * be cast to Inputs*
     */
    static void IRAM_ATTR handle_encoder_change(void* arg);    

    /**
     * @brief Event handler handling input change events on the PCF8575 
     *
     */
    void IRAM_ATTR on_PCF8575_input_changed();

    std::vector<std::function<void(int, int)>> _wheel_callbacks;
    inline static PCF8575* _pcf8575 = NULL;
    TaskHandle_t _extendedGPIOWatcher = NULL;
    TaskHandle_t _wheelRunner = NULL;
    int16_t _wheel_encoded = 0x0;
    int16_t _wheel_position = 0x0;
    int8_t _direction = 0;
    std::atomic<bool> _pause {false};
    std::atomic<bool> _wheelRunner_stop_requested {false};
    std::atomic<bool> _extended_gpio_watcher_stop_requested {false};

};


#endif // _INPUTS_H_