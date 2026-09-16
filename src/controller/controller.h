// Copyright (c) Thor Schueler. All rights reserved.
// SPDX-License-Identifier: MIT

#ifndef _CONTROLLER_H_
#define _CONTROLLER_H_

#include "Arduino.h"
#include "src/display/display.h"
#include "src/logging/SerialLogger.h"
#include "src/inputs/inputs.h"
#include "src/motion/motion.h"

#define LIGHT_STRIP_PIN 32

/**
 * @brief  This is the main class for the bandsaw controller, coordinating and managing the various 
 * components of the system. 
 * 
 */
class Controller 
{

    public:
        /** 
         * @brief Construct a new Controller object 
         */
        Controller();

        /** 
         * @brief Destroy the Controller object 
         */
        ~Controller();

        /** 
         * @brief Initialize the controller and its components and start the execution flow 
         */
        void begin();

        String get_touch_command()
        {
            String command = this->_touch_command;
            this->_touch_command = "";
            return command;
        }

    protected:

        /**
         * @brief Workflow to switch the warm lights on. The lights are actually directly activated
         * via the switch, so this is just reflecting the state on the display
         * 
         * @param gpio - GPIO for the warm light indicator.
         * @param command - Command name passed in from the input watcher
         * 
         * @remarks - the function signature is a delegate for the input watcher
         */
        void warm_lights_on(uint8_t gpio, const char* command);

        /**
         * @brief Workflow to switch the bright lights on. The lights are actually directly activated
         * via the switch, so this is just reflecting the state on the display
         * 
         * @param gpio - GPIO for the bright light indicator.
         * @param command - Command name passed in from the input watcher
         * 
         * @remarks - the function signature is a delegate for the input watcher
         */        
        void bright_lights_on(uint8_t gpio, const char* command);

        /**
         * @brief Workflow to switch the lights (either warm or cold) off. The lights are actually directly activated
         * via the switch, so this is just reflecting the state on the display
         * 
         * @param gpio - GPIO for the light indicator (either warm or cold, depending on the invocation).
         * @param command - Command name passed in from the input watcher
         * 
         * @remarks - the function signature is a delegate for the input watcher
         */        
        void lights_off(uint8_t gpio, const char* command);

        /**
         * @brief Workflow to toggle the saw blade on or off. 
         * 
         * @param gpio - GPIO for the start/stop toggle.
         * @param command - Command name passed in from the input watcher
         * 
         * @remarks - the function signature is a delegate for the input watcher
         */         
        void toggle_saw_blade(uint8_t gpio, const char* command);

        /**
         * @brief Workflow to manage air blast. 
         * 
         * @param gpio - GPIO for the start/stop toggle.
         * @param command - Command name passed in from the input watcher
         * 
         * @remarks - the function signature is a delegate for the input watcher
         */ 
        void manage_air(uint8_t gpio, const char* command);

        /**
         * @brief Workflow to manage coolant mist. 
         * 
         * @param gpio - GPIO for the start/stop toggle.
         * @param command - Command name passed in from the input watcher
         * 
         * @remarks - the function signature is a delegate for the input watcher
         */ 
        void manage_coolant(uint8_t gpio, const char* command);


    private:

        void switch_on(uint8_t gpio, const char* command);
        void switch_off(uint8_t gpio, const char* command);

        /**
         * @brief toggles a temporary button off.
         * 
         * @param gpio - GPIO for the start/stop toggle.
         * @param command - Command name passed in from the input watcher
         * 
         * @remarks - the function signature is a delegate for the input watcher
        */
        void toggle_off(uint8_t gpio, const char* command);

        Display* _display = nullptr;
        Inputs* _inputs = nullptr;
        Motion* _motion = nullptr;
        String _touch_command;
        bool _enable_backlight = true;
        bool _is_EMS_active = false;
};




#endif // _CONTROLLER_H_