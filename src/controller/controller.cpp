// Copyright (c) Thor Schueler. All rights reserved.
// SPDX-License-Identifier: MIT
// IMPORTANT: LIBRARY MUST BE SPECIFICALLY CONFIGURED FOR EITHER TFT SHIELD
// OR BREAKOUT BOARD USAGE.

#include "controller.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

/** 
 * @brief Construct a new Controller object 
 */
Controller::Controller()
{
    this->_display = new Display();
}

/** 
  * @brief Destroy the Controller object 
  */
Controller::~Controller() 
{
    if(this->_display != NULL) this->_display;
}

/**
 * @brief Start the controller execution flow, which sets up various tasks and starts various components.
 * 
 */
void Controller::begin() 
{
    this->_display->begin();
    this->_display->setTouchCallback(std::bind(&Controller::touch_callback, this, std::placeholders::_1));
}


void Controller::touch_callback(const char* command)
{
    this->_touch_command = String(command);
}