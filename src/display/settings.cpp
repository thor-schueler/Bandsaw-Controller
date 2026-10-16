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
    { 3, SETTING_SCREENS::GENERAL, "General", settings_general, 0, 0, nullptr, nullptr},
    { 4, SETTING_SCREENS::JOBS, "Jobs", settings_jobs, 0, 1, nullptr, nullptr},
    { 5, SETTING_SCREENS::TIMEOUTS, "Timeouts", settings_timeouts, 1, 2, nullptr, nullptr}
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

void Display::draw_settings_detail(uint8_t gpio, const char*)
{
    setting_screen_t *ps = nullptr;
    for(auto &s : __screens) if(s.id == gpio) { ps = &s; break; }
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
    section.deleteSprite();
    this->set_workarea_title(settings_title, settings_title_size, " - " + String(ps->name));
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
 * @param gpio - GPIO for the light indicator (either warm or cold, depending on the invocation).
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
 * @param gpio - GPIO for the light indicator (either warm or cold, depending on the invocation).
 * @param command - Command name passed in from the input watcher
 * 
 * @remarks - the function signature is a delegate for the input watcher
 */
void Display::default_settings(uint8_t gpio, const char*)
{
    Logger.Info("Restore Defaults");
}