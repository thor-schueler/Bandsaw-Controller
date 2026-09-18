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

    this->_inputs->register_command(EXT_GPIO_START_PIN, std::bind(&Controller::toggle_saw_blade, this, std::placeholders::_1, std::placeholders::_2), std::bind(&Controller::toggle_off, this, std::placeholders::_1, std::placeholders::_2), true);
    this->_inputs->register_command(EXT_GPIO_ENGAGE_PIN, std::bind(&Controller::switch_on, this, std::placeholders::_1, std::placeholders::_2), std::bind(&Controller::toggle_off, this, std::placeholders::_1, std::placeholders::_2), true);
    this->_inputs->register_command(EXT_GPIO_HOME_PIN, std::bind(&Controller::home, this, std::placeholders::_1, std::placeholders::_2), std::bind(&Controller::toggle_off, this, std::placeholders::_1, std::placeholders::_2), true);
    this->_inputs->register_command(EXT_GPIO_LIGHT_COLD, std::bind(&Controller::manage_lights, this, std::placeholders::_1, std::placeholders::_2), std::bind(&Controller::manage_lights, this, std::placeholders::_1, std::placeholders::_2), true);
    this->_inputs->register_command(EXT_GPIO_LIGHT_WARM, std::bind(&Controller::manage_lights, this, std::placeholders::_1, std::placeholders::_2), std::bind(&Controller::manage_lights, this, std::placeholders::_1, std::placeholders::_2), true);
    this->_inputs->register_command(EXT_GPIO_AIR_ON, std::bind(&Controller::manage_air, this, std::placeholders::_1, std::placeholders::_2), std::bind(&Controller::manage_air, this, std::placeholders::_1, std::placeholders::_2), true);
    this->_inputs->register_command(EXT_GPIO_AIR_AUTO, std::bind(&Controller::manage_air, this, std::placeholders::_1, std::placeholders::_2), std::bind(&Controller::manage_air, this, std::placeholders::_1, std::placeholders::_2), true);
    this->_inputs->register_command(EXT_GPIO_LUBE_ON, std::bind(&Controller::manage_coolant, this, std::placeholders::_1, std::placeholders::_2), std::bind(&Controller::manage_coolant, this, std::placeholders::_1, std::placeholders::_2), true);
    this->_inputs->register_command(EXT_GPIO_LUBE_AUTO, std::bind(&Controller::manage_coolant, this, std::placeholders::_1, std::placeholders::_2), std::bind(&Controller::manage_coolant, this, std::placeholders::_1, std::placeholders::_2), true);
    this->_inputs->register_command(EXT_GPIO_EMS, std::bind(&Controller::EMS_change, this, std::placeholders::_1, std::placeholders::_2), std::bind(&Controller::EMS_change, this, std::placeholders::_1, std::placeholders::_2), false);
    this->_inputs->begin();

    if(this->_inputs->digitalReadEx(EXT_GPIO_EMS)) this->_display->draw_canvas(); 
                // only build canvas if EMS is not active

    this->_inputs->start_monitoring();

    Logger.Info(F("... Done."));
}

/**
 * @brief Workflow to manage the lights (either warm or cold) off. The lights are actually directly activated
 * via the switch, so this is just reflecting the state on the display
 * 
 * @param gpio - GPIO for the light indicator (either warm or cold, depending on the invocation).
 * @param command - Command name passed in from the input watcher
 * 
 * @remarks - the function signature is a delegate for the input watcher
 */        
void Controller::manage_lights(uint8_t gpio, const char* command)
{
    if(!this->_inputs->digitalReadEx(EXT_GPIO_EMS)) return;
            // do nothing when EMS (active low) is active. 

    uint8_t is_warm = !this->_inputs->digitalReadEx(EXT_GPIO_LIGHT_WARM);
    uint8_t is_cold = !this->_inputs->digitalReadEx(EXT_GPIO_LIGHT_COLD);
                                                // inputs are active low!
    if(!is_warm && ! is_cold)
    {
        digitalWrite(LIGHT_STRIP_PIN, LOW);
        this->_display->set_button(gpio, TOUCH_TAB_STATE::OFF);
        this->_display->set_button_tab(gpio, TOUCH_TAB_STATE::OFF);        
    }
    else if(is_warm)
    {
        this->_display->set_button(gpio, TOUCH_TAB_STATE::ON);    
        this->_display->set_button_tab(gpio, TOUCH_TAB_STATE::WAITING);
        if(this->_enable_backlight) digitalWrite(LIGHT_STRIP_PIN, HIGH);
    }
    else if(is_cold)
    {
        this->_display->set_button(gpio, TOUCH_TAB_STATE::ON);    
        this->_display->set_button_tab(gpio, TOUCH_TAB_STATE::ON);
        if(this->_enable_backlight) digitalWrite(LIGHT_STRIP_PIN, HIGH);
    }

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
    if(!this->_inputs->digitalReadEx(EXT_GPIO_EMS)) return;
            // do nothing when EMS (active low) is active. 

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
    if(!this->_inputs->digitalReadEx(EXT_GPIO_EMS)) return;
            // do nothing when EMS (active low) is active. 

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
    if(!this->_inputs->digitalReadEx(EXT_GPIO_EMS)) return;
            // do nothing when EMS (active low) is active. 

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


/**
 * @brief Handles EMS button changes
 * 
 * @param gpio - GPIO for the start/stop toggle.
 * @param command - Command name passed in from the input watcher
 * 
 * @remarks - the actual EMS shutdown is performed via physical switch. The handler is responsible
 * for auxiliary shutdowns such as air and lubricant and feed
 */
void Controller::EMS_change(uint8_t gpio, const char* command)
{
    uint8_t io = this->_inputs->digitalReadEx(gpio);
    ems_state_t s = this->_motion->manage_ems_state(io);

    if(s == EMS_STATE::SHUTDOWN)
    {
        this->_inputs->pause_monitoring();
        this->_display->pause_tasks();
        this->_display->set_button_tab(EXT_GPIO_START_PIN, TOUCH_TAB_STATE::OFF); 
        this->_display->set_button_tab(EXT_GPIO_HOME_PIN, TOUCH_TAB_STATE::OFF); 
        this->_display->set_button_tab(EXT_GPIO_ENGAGE_PIN, TOUCH_TAB_STATE::OFF); 
        
        this->_display->set_button_tab(EXT_GPIO_LUBE_AUTO, (!this->_inputs->digitalReadEx(EXT_GPIO_LUBE_AUTO) || !this->_inputs->digitalReadEx(EXT_GPIO_LUBE_ON)) ? TOUCH_TAB_STATE::WAITING : TOUCH_TAB_STATE::OFF);
        this->_display->set_button_tab(EXT_GPIO_AIR_AUTO, (!this->_inputs->digitalReadEx(EXT_GPIO_AIR_AUTO) || !this->_inputs->digitalReadEx(EXT_GPIO_AIR_ON)) ? TOUCH_TAB_STATE::WAITING : TOUCH_TAB_STATE::OFF);
        this->_display->ems_overlay(true);
    }
    else
    {
        this->_display->ems_overlay(false);
        this->manage_coolant(EXT_GPIO_LUBE_AUTO, "");       // force re-evaluation of coolant state
        this->manage_air(EXT_GPIO_AIR_AUTO, "");            // force re-evaluation of air state
        this->manage_lights(EXT_GPIO_LIGHT_COLD, "");       // force re-evaluation of light state
        this->_display->resume_tasks();
        this->_inputs->resume_monitoring();
    }
}

/**
 * @brief Initiates the feed carriage homing process
 * 
 * @param gpio - GPIO for the home toggle.
 * @param command - Command name passed in from the input watcher
 */
void Controller::home(uint8_t gpio, const char* command)
{
    static bool term = false;

    if(!this->_inputs->digitalReadEx(EXT_GPIO_EMS)) return;
            // do nothing when EMS (active low) is active. 
        
    this->_display->set_button(gpio, TOUCH_TAB_STATE::ON); 
    this->_display->set_button_tab(gpio, TOUCH_TAB_STATE::ON);

    this->_display->set_workarea_title(homing_title, homing_title_size, "");
    this->_display->start_homing_animation(term);    
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