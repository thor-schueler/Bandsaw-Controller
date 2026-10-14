// Copyright (c) Thor Schueler. All rights reserved.
// SPDX-License-Identifier: MIT

#include "version.h"
#include "display.h"
#include "src/inputs/inputs.h"
#include "src/config/config.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <FunctionalInterrupt.h>

/**
 * @brief Toggles the settings screen on an off.
 * 
 */
void Display::toggle_settings()
{
    LGFX_Sprite controls(this);
    if(_screen != SCREENS::SETTINGS)
    {
        _screen = SCREENS::SETTINGS;
        this->pause_tasks();
        vTaskDelay(pdMS_TO_TICKS(100));
        this->draw_canvas();
        this->set_workarea_title(settings_title, settings_title_size, "");
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
    {
        _screen = SCREENS::MAIN;
        this->draw_canvas();
        this->resume_tasks();
    }
}