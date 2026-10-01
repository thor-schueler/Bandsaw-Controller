// Copyright (c) Thor Schueler. All rights reserved.
// SPDX-License-Identifier: MIT

#include "version.h"
#include "display.h"
#include "src/inputs/inputs.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <FunctionalInterrupt.h>

/**
 * @brief Hides the toast from the UI
 * 
 */
void Display::hide_toast()
{
    LGFX_Sprite as(this);
    esp_timer_stop(this->toast_timer);
    as.setColorDepth(16);
    as.createSprite(status_width, status_height);
    as.pushImage(0, 0, status_width, status_height, (lgfx::rgb565_t*)alert_toast_inactive);
    if (xSemaphoreTake(this->_display_mutex, portMAX_DELAY) == pdTRUE)
    {  
        as.pushSprite(STATUS_X, STATUS_Y);
        xSemaphoreGive(this->_display_mutex);
    }
}

/**
 * @brief Displays an alert toast in the UI
 * 
 * @param alert - Alert to display
 * @param add_to_alert - Whether to add the alert to alerts list. Defaults to True.
 * @param timeout - timeout for the toast in seconds. Defaults to 10. 
 */
void Display::show_toast(String alert, bool add_to_alert, uint8_t timeout)
{
    LGFX_Sprite as(this);
    as.setColorDepth(16);
    as.createSprite(status_width, status_height);
    as.pushImage(0, 0, status_width, status_height, (lgfx::rgb565_t*)alert_toast);
    as.setTextColor(TFT_WHITE, 0xFA49);
    as.setFont(&fonts::Font2);
    as.drawString(alert.c_str(), 10, 2);
    if (xSemaphoreTake(this->_display_mutex, portMAX_DELAY) == pdTRUE)
    {  
        as.pushSprite(STATUS_X, STATUS_Y);
        xSemaphoreGive(this->_display_mutex);
    }
    esp_timer_start_once(this->toast_timer, 10'000'000); // 10 sec in us
}