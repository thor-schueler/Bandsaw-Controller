// Copyright (c) Thor Schueler. All rights reserved.
// SPDX-License-Identifier: MIT

#include "version.h"
#include "display.h"
#include "src/inputs/inputs.h"
#include "src/config/config.h"
#include "dialog/dialog.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <FunctionalInterrupt.h>


std::array<setting_screen_t, 6> __screens = {{    
    { 3, SETTING_SCREENS::GENERAL, "General", settings_general, 0, 0, Config::General(), general_fields, std::size(general_fields), nullptr, nullptr},
    { 4, SETTING_SCREENS::JOBS, "Jobs", settings_jobs, 0, 1, Config::Jobs(), jobs_fields, std::size(jobs_fields), nullptr, nullptr},
    { 5, SETTING_SCREENS::TIMEOUTS, "Timeouts", settings_timeouts, 1, 2, Config::Timeouts(), timeout_fields, std::size(timeout_fields), nullptr, nullptr},
    { 6, SETTING_SCREENS::INVALID, "", nullptr, 0, 0, { 0, 0, 0 }, nullptr, 0, nullptr, nullptr},
    { 7, SETTING_SCREENS::INVALID, "", nullptr, 0, 0, { 0, 0, 0 }, nullptr, 0, nullptr, nullptr},
    { 8, SETTING_SCREENS::INVALID, "", nullptr, 0, 0, { 0, 0, 0 }, nullptr, 0, nullptr, nullptr}
}};
setting_screens_t __current_screen = SETTING_SCREENS::INVALID;
bool __needs_saving = false;

/**
 * @brief Toggles the settings screen on an off.
 * @param force_on - true to force the settings display on. Defaults to false. 
 */
void Display::toggle_settings(bool force_on)
{
    
    if(this->_screen != SCREENS::SETTINGS || (this->_screen == SCREENS::SETTINGS && __current_screen != SETTING_SCREENS::INVALID) || force_on)
    {
        LGFX_Sprite controls(this);
        _screen = SCREENS::SETTINGS;
        this->pause_tasks();
        vTaskDelay(pdMS_TO_TICKS(100));
        this->draw_canvas();
        this->set_workarea_title(settings_title, settings_title_size, "");
        this->draw_settings_canvas();
        controls.setColorDepth(16);
        controls.createSprite(127, 143);
        controls.fillSprite(TFT_BLACK);
        controls.pushImage(0, 0, 127, 143, (lgfx::rgb565_t*)settings_controls);
        if (xSemaphoreTake(this->_display_mutex, portMAX_DELAY) == pdTRUE)
        { 
            controls.pushSprite(353, 58);
            xSemaphoreGive(this->_display_mutex);
        }
        controls.deleteSprite();
    }
    else
    {   if(__needs_saving)
        {
            Dialog d(this);
            Logger.Info("Entering Dialog");
            if(d.show("Discard changes and exit?"))
            {
                Logger.Info("Dialog done - proceed");
                _screen = SCREENS::MAIN;
                this->draw_canvas();
                this->resume_tasks();
                    // we are on the main settings screen. 
                    // return to the main screen
            }
            else
            {
                Logger.Info("Dialog done - cancel");
                this->toggle_settings(true);
            }
        }
        else
        {
            _screen = SCREENS::MAIN;
            this->draw_canvas();
            this->resume_tasks();
                // we are on the main settings screen. 
                // return to the main screen
        }
    }
    __current_screen = SETTING_SCREENS::INVALID;
}

/** 
 * @brief Draws the main canvas of the settings screen
 */
void Display::draw_settings_canvas()
{
    LGFX_Sprite section(this);
    section.setColorDepth(16);
    section.createSprite(59,59);

    for (const auto& s : __screens) 
    {
        if(s.section == SETTING_SCREENS::INVALID) continue;
        if(s.icon == nullptr) continue;  

        section.pushImage(0, 0, 59, 59, (lgfx::rgb565_t *)s.icon);
        if (xSemaphoreTake(this->_display_mutex, portMAX_DELAY) == pdTRUE)
        { 
            section.pushSprite(96 + s.icon_column * 87, 100 + s.icon_row * 75);
            xSemaphoreGive(this->_display_mutex);
        }
    }

    section.deleteSprite();
}

/**
 * @brief Draws teh detail screen for a particular setting section
 * @param id - ID of the settings screen
 * @param command - Command name passed in from the input watcher
 * 
 * @remarks - the function signature is a delegate for the input watcher
 */
void Display::draw_settings_detail(uint8_t id, const char*)
{
    setting_screen_t *ps = nullptr;
    for(auto &s : __screens) if(s.id == id) { ps = &s; break; }
    __current_screen = ps->section;

    LGFX_Sprite section(this);
    section.setColorDepth(16);
    section.createSprite(HOMING_W, HOMING_H);
    section.fillSprite(TFT_BLACK);
    if (xSemaphoreTake(this->_display_mutex, portMAX_DELAY) == pdTRUE)
    { 
        section.pushSprite(88, 95);
        xSemaphoreGive(this->_display_mutex);
    }    

    // draw the controls for the various sections here...
    settings_t settings = ps->settings;
    const field_info_t* fields = ps->fields;
    const size_t size = ps->fields_size; 
    this->draw_settings(settings, fields, size);

    section.deleteSprite();
    this->set_workarea_title(settings_title, settings_title_size, " - " + String(ps->name));
}

/**
 * @brief Draws the settings controls for a certain settings section
 * @param value - settings enumeration. Must derive from settings_t
 * @param fields - the fields enumeration for the settings enumeration
 * @param fields_size - the number of fields in fields
 */
template<typename T>
void Display::draw_settings(const T& value, const field_info_t* fields, size_t fields_size)
{
    static_assert(std::is_base_of_v<settings_t, T>, "T must derive from settings_t"); 
    for (size_t i = 0; i < fields_size; ++i)
    {
        const field_info_t& field = fields[i];
        const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&value) + field.offset;
        switch(field.type)
        {
            case field_type_t::bool_t:
                Logger.Info_f(field.format_string, field.name, *reinterpret_cast<const bool*>(ptr) ? "true" : "false");
                break;
            case field_type_t::uint8_t:
                Logger.Info_f(field.format_string, field.name, *reinterpret_cast<const uint8_t*>(ptr));
                break; 
            case field_type_t::uint16_t:
                Logger.Info_f(field.format_string, field.name, *reinterpret_cast<const uint16_t*>(ptr));
                break;     
            case field_type_t::uint32_t:
                Logger.Info_f(field.format_string, field.name, *reinterpret_cast<const uint32_t*>(ptr));
                break;            
            case field_type_t::float_t:
                Logger.Info_f(field.format_string, field.name, *reinterpret_cast<const float*>(ptr));
                break;
            default:
                Logger.Info_f(F("   %s: %u (unknown type, assuming one byte value)"), field.name, *ptr);
        }
    }
}

/**
 * @brief Get the settings screens object containing information about the settings screens
 * 
 * @return std::array<settings_screen_t, 6> 
 */
std::array<setting_screen_t, 6>& Display::get_settings_screens() { return __screens; }

/**
 * @brief Gets the current settings screens object containing information about the settings screen
 * 
 * @return settings_screen_t& 
 */
const setting_screens_t& Display::get_current_settings_screen() { return __current_screen; }


/**
 * @brief Save the settings to NVMS
 * @param id - Comamnd id
 * @param command - Command name passed in from the input watcher
 * 
 * @remarks - the function signature is a delegate for the input watcher
 */
void Display::save_settings(uint8_t gpio, const char*)
{
    Logger.Info("Save settings");
}

/**
 * @brief Apply the defaults to the settings
 * @param id - Command ID.
 * @param command - Command name passed in from the input watcher
 * 
 * @remarks - the function signature is a delegate for the input watcher
 */
void Display::default_settings(uint8_t gpio, const char*)
{
    Logger.Info("Restore Defaults");
}

/**
 * @brief Process touch input for settings detail screens
 * @param x - touch x coordinate
 * @param y - touch y coordinate
 */
void Display::process_settings_screen_touch(uint16_t x, uint16_t y)
{

}