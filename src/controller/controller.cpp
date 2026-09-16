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
    this->_motion = new Motion();
}

/** 
  * @brief Destroy the Controller object 
  */
Controller::~Controller() 
{
    if(this->_display != nullptr) delete this->_display;
    if(this->_inputs != nullptr) delete this->_inputs;
    if(this->_motion != nullptr) delete this->_motion;
    this->_display = nullptr;
    this->_inputs = nullptr;
    this->_motion = nullptr;
}

/**
 * @brief Start the controller execution flow, which sets up various tasks and starts various components.
 * 
 */
void Controller::begin() 
{
    Logger.Info(F("... Begin input controller execution."));
    Logger.Info(F("...   Setup general GPIO for lights."));
    pinMode(LIGHT_STRIP_PIN, OUTPUT);
    digitalWrite(LIGHT_STRIP_PIN, LOW);

    this->_display->begin();
    this->_motion->begin();

    this->_inputs->register_command(EXT_GPIO_START_PIN, std::bind(&Controller::toggle_saw_blade, this, std::placeholders::_1, std::placeholders::_2), std::bind(&Controller::toggle_off, this, std::placeholders::_1, std::placeholders::_2));
    this->_inputs->register_command(EXT_GPIO_ENGAGE_PIN, std::bind(&Controller::switch_on, this, std::placeholders::_1, std::placeholders::_2), std::bind(&Controller::toggle_off, this, std::placeholders::_1, std::placeholders::_2));
    this->_inputs->register_command(EXT_GPIO_HOME_PIN, std::bind(&Controller::switch_on, this, std::placeholders::_1, std::placeholders::_2), std::bind(&Controller::toggle_off, this, std::placeholders::_1, std::placeholders::_2));
    this->_inputs->register_command(EXT_GPIO_LIGHT_COLD, std::bind(&Controller::bright_lights_on, this, std::placeholders::_1, std::placeholders::_2), std::bind(&Controller::lights_off, this, std::placeholders::_1, std::placeholders::_2));
    this->_inputs->register_command(EXT_GPIO_LIGHT_WARM, std::bind(&Controller::warm_lights_on, this, std::placeholders::_1, std::placeholders::_2), std::bind(&Controller::lights_off, this, std::placeholders::_1, std::placeholders::_2));
    this->_inputs->register_command(EXT_GPIO_AIR_ON, std::bind(&Controller::manage_air, this, std::placeholders::_1, std::placeholders::_2), std::bind(&Controller::manage_air, this, std::placeholders::_1, std::placeholders::_2));
    this->_inputs->register_command(EXT_GPIO_AIR_AUTO, std::bind(&Controller::manage_air, this, std::placeholders::_1, std::placeholders::_2), std::bind(&Controller::manage_air, this, std::placeholders::_1, std::placeholders::_2));
    this->_inputs->register_command(EXT_GPIO_LUBE_ON, std::bind(&Controller::manage_coolant, this, std::placeholders::_1, std::placeholders::_2), std::bind(&Controller::manage_coolant, this, std::placeholders::_1, std::placeholders::_2));
    this->_inputs->register_command(EXT_GPIO_LUBE_AUTO, std::bind(&Controller::manage_coolant, this, std::placeholders::_1, std::placeholders::_2), std::bind(&Controller::manage_coolant, this, std::placeholders::_1, std::placeholders::_2));
    this->_inputs->register_command(EXT_GPIO_EMS, std::bind(&Controller::switch_on, this, std::placeholders::_1, std::placeholders::_2), std::bind(&Controller::switch_off, this, std::placeholders::_1, std::placeholders::_2));
    this->_inputs->begin();


    Logger.Info(F("... Done."));
}

/**
 * @brief Workflow to switch the warm lights on. The lights are actually directly activated
 * via the switch, so this is just reflecting the state on the display
 * 
 * @param gpio - GPIO for the warm light indicator.
 * @param command - Command name passed in from the input watcher
 * 
 * @remarks - the function signature is a delegate for the input watcher
 */
void Controller::warm_lights_on(uint8_t gpio, const char* command)
{
    this->_display->set_button(gpio, TOUCH_TAB_STATE::ON);
    this->_display->set_button_tab(gpio, TOUCH_TAB_STATE::WAITING);
    if(this->_enable_backlight) digitalWrite(LIGHT_STRIP_PIN, HIGH);
}

/**
 * @brief Workflow to switch the bright lights on. The lights are actually directly activated
 * via the switch, so this is just reflecting the state on the display
 * 
 * @param gpio - GPIO for the bright light indicator.
 * @param command - Command name passed in from the input watcher
 * 
 * @remarks - the function signature is a delegate for the input watcher
 */   
void Controller::bright_lights_on(uint8_t gpio, const char* command)
{
    this->_display->set_button(gpio, TOUCH_TAB_STATE::ON);    
    this->_display->set_button_tab(gpio, TOUCH_TAB_STATE::ON);
    if(this->_enable_backlight) digitalWrite(LIGHT_STRIP_PIN, HIGH);
}

/**
 * @brief Workflow to switch the lights (either warm or cold) off. The lights are actually directly activated
 * via the switch, so this is just reflecting the state on the display
 * 
 * @param gpio - GPIO for the light indicator (either warm or cold, depending on the invocation).
 * @param command - Command name passed in from the input watcher
 * 
 * @remarks - the function signature is a delegate for the input watcher
 */   
void Controller::lights_off(uint8_t gpio, const char* command)
{
    digitalWrite(LIGHT_STRIP_PIN, LOW);
    this->_display->set_button(gpio, TOUCH_TAB_STATE::OFF);
    this->_display->set_button_tab(gpio, TOUCH_TAB_STATE::OFF);
}

/**
 * @brief Workflow to toggle the saw blade on or off. 
 * 
 * @param gpio - GPIO for the start/stop toggle.
 * @param command - Command name passed in from the input watcher
 * 
 * @remarks - the function signature is a delegate for the input watcher
 */         
void Controller::toggle_saw_blade(uint8_t gpio, const char* command)
{
    
    if(this->_motion->get_blade_status() == BLADE_STATE::STOPPED)
    {
        this->_display->set_button(gpio, TOUCH_TAB_STATE::ON); 
        if(this->_motion->activate_blade() == BLADE_STATE::RUNNING) this->_display->set_button_tab(gpio, TOUCH_TAB_STATE::ON);
        else this->_display->set_button_tab(gpio, TOUCH_TAB_STATE::OFF);
        
    }
    else 
    {
        this->_display->set_button(gpio, TOUCH_TAB_STATE::ON);    
        if(this->_motion->deactivate_blade() == BLADE_STATE::STOPPED) this->_display->set_button_tab(gpio, TOUCH_TAB_STATE::OFF);
        else this->_display->set_button_tab(gpio, TOUCH_TAB_STATE::ON);
    }

}

/**
 * @brief Workflow to manage air blast. 
 * 
 * @param gpio - GPIO for the start/stop toggle.
 * @param command - Command name passed in from the input watcher
 * 
 * @remarks - the function signature is a delegate for the input watcher
 */ 
void Controller::manage_air(uint8_t gpio, const char* command)
{
    uint8_t is_auto = !this->_inputs->digitalReadEx(EXT_GPIO_AIR_AUTO);
    uint8_t is_on = !this->_inputs->digitalReadEx(EXT_GPIO_AIR_ON);
                                                // inputs are active low!

    air_state_t s = this->_motion->manage_air(is_on, is_auto);
    if(s == AIR_STATE::ON)
    {
        this->_display->set_button(gpio, TOUCH_TAB_STATE::ON); 
        this->_display->set_button_tab(gpio, TOUCH_TAB_STATE::ON);  
    }   
    else if (s == AIR_STATE::OFF)
    {
        this->_display->set_button(gpio, TOUCH_TAB_STATE::OFF);
        this->_display->set_button_tab(gpio, TOUCH_TAB_STATE::OFF); 
    } 
    else 
    {
        this->_display->set_button(gpio, TOUCH_TAB_STATE::ON);
        this->_display->set_button_tab(gpio, TOUCH_TAB_STATE::WAITING);   
    }
}

/**
 * @brief Workflow to manage coolant mist. 
 * 
 * @param gpio - GPIO for the start/stop toggle.
 * @param command - Command name passed in from the input watcher
 * 
 * @remarks - the function signature is a delegate for the input watcher
 */ 
void Controller::manage_coolant(uint8_t gpio, const char* command)
{
    uint8_t is_auto = !this->_inputs->digitalReadEx(EXT_GPIO_LUBE_AUTO);
    uint8_t is_on = !this->_inputs->digitalReadEx(EXT_GPIO_LUBE_ON);
                                                // inputs are active low!

    coolant_state_t s = this->_motion->manage_coolant(is_on, is_auto);
    if(s == COOLANT_STATE::ON)
    {
        this->_display->set_button(gpio, TOUCH_TAB_STATE::ON); 
        this->_display->set_button_tab(gpio, TOUCH_TAB_STATE::ON);  
    }   
    else if (s == COOLANT_STATE::OFF)
    {
        this->_display->set_button(gpio, TOUCH_TAB_STATE::OFF);
        this->_display->set_button_tab(gpio, TOUCH_TAB_STATE::OFF); 
    } 
    else 
    {
        this->_display->set_button(gpio, TOUCH_TAB_STATE::ON);
        this->_display->set_button_tab(gpio, TOUCH_TAB_STATE::WAITING);   
    }
}


/**
 * @brief Toggles a temporary button off.
 * 
 * @param gpio - GPIO for the start/stop toggle.
 * @param command - Command name passed in from the input watcher
 * 
 * @remarks - the function signature is a delegate for the input watcher
 */
void Controller::toggle_off(uint8_t gpio, const char* command)
{
    this->_display->set_button(gpio, TOUCH_TAB_STATE::OFF);
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