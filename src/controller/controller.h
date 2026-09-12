// Copyright (c) Thor Schueler. All rights reserved.
// SPDX-License-Identifier: MIT

#ifndef _CONTROLLER_H_
#define _CONTROLLER_H_

#include "Arduino.h"
#include "src/display/display.h"
#include "src/logging/SerialLogger.h"

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

    private:

        void touch_callback(const char* command);

        Display* _display;
        String _touch_command;
};




#endif // _CONTROLLER_H_