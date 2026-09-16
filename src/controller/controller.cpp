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
    this->_inputs = new Inputs();
}

/** 
  * @brief Destroy the Controller object 
  */
Controller::~Controller() 
{
    if(this->_display != NULL) delete this->_display;
    if(this->_inputs != NULL) delete this->_inputs;
    this->_display = NULL;
    this->_inputs = NULL;
}

/**
 * @brief Start the controller execution flow, which sets up various tasks and starts various components.
 * 
 */
void Controller::begin() 
{
    this->_display->begin();

    this->_inputs->register_command(EXT_GPIO_START_PIN, std::bind(&Controller::switch_on, this, std::placeholders::_1, std::placeholders::_2), nullptr);
    this->_inputs->register_command(EXT_GPIO_ENGAGE_PIN, std::bind(&Controller::switch_on, this, std::placeholders::_1, std::placeholders::_2), nullptr);
    this->_inputs->register_command(EXT_GPIO_HOME_PIN, std::bind(&Controller::switch_on, this, std::placeholders::_1, std::placeholders::_2), nullptr);
    this->_inputs->register_command(EXT_GPIO_LIGHT_COLD, std::bind(&Controller::bright_lights_on, this, std::placeholders::_1, std::placeholders::_2), std::bind(&Controller::lights_off, this, std::placeholders::_1, std::placeholders::_2));
    this->_inputs->register_command(EXT_GPIO_LIGHT_WARM, std::bind(&Controller::warm_lights_on, this, std::placeholders::_1, std::placeholders::_2), std::bind(&Controller::lights_off, this, std::placeholders::_1, std::placeholders::_2));
    this->_inputs->register_command(EXT_GPIO_AIR_ON, std::bind(&Controller::switch_on, this, std::placeholders::_1, std::placeholders::_2), std::bind(&Controller::switch_off, this, std::placeholders::_1, std::placeholders::_2));
    this->_inputs->register_command(EXT_GPIO_AIR_AUTO, std::bind(&Controller::switch_on, this, std::placeholders::_1, std::placeholders::_2), std::bind(&Controller::switch_off, this, std::placeholders::_1, std::placeholders::_2));
    this->_inputs->register_command(EXT_GPIO_LUBE_ON, std::bind(&Controller::switch_on, this, std::placeholders::_1, std::placeholders::_2), std::bind(&Controller::switch_off, this, std::placeholders::_1, std::placeholders::_2));
    this->_inputs->register_command(EXT_GPIO_LUBE_AUTO, std::bind(&Controller::switch_on, this, std::placeholders::_1, std::placeholders::_2), std::bind(&Controller::switch_off, this, std::placeholders::_1, std::placeholders::_2));
    this->_inputs->register_command(EXT_GPIO_EMS, std::bind(&Controller::switch_on, this, std::placeholders::_1, std::placeholders::_2), std::bind(&Controller::switch_off, this, std::placeholders::_1, std::placeholders::_2));
    this->_inputs->begin();

    pinMode(LIGHT_STRIP_PIN, OUTPUT);
    digitalWrite(LIGHT_STRIP_PIN, LOW);
}

void Controller::warm_lights_on(uint8_t gpio, const char* command)
{
    this->_display->set_button(gpio, TOUCH_TAB_STATE::ON);
    this->_display->set_button_tab(gpio, TOUCH_TAB_STATE::WAITING);
    if(this->_enable_backlight) digitalWrite(LIGHT_STRIP_PIN, HIGH);
}

void Controller::bright_lights_on(uint8_t gpio, const char* command)
{
    this->_display->set_button(gpio, TOUCH_TAB_STATE::ON);    
    this->_display->set_button_tab(gpio, TOUCH_TAB_STATE::ON);
    if(this->_enable_backlight) digitalWrite(LIGHT_STRIP_PIN, HIGH);
}

void Controller::lights_off(uint8_t gpio, const char* command)
{
    digitalWrite(LIGHT_STRIP_PIN, LOW);
    this->_display->set_button(gpio, TOUCH_TAB_STATE::OFF);
    this->_display->set_button_tab(gpio, TOUCH_TAB_STATE::OFF);
}

void Controller::switch_on(uint8_t gpio, const char* command)
{
    Logger.Info_f(F("Command on: %s, gpio %d"), command, gpio);
    this->_display->set_button_tab(gpio, TOUCH_TAB_STATE::ON);
}

void Controller::switch_off(uint8_t gpio, const char* command)
{
    Logger.Info_f(F("Command off: %s, gpio %d"), command, gpio);
    this->_display->set_button_tab(gpio, TOUCH_TAB_STATE::OFF);
}