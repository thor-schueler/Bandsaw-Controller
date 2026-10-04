// Copyright (c) Thor Schueler. All rights reserved.
// SPDX-License-Identifier: MIT

#include "version.h"
#include "display.h"
#include "src/inputs/inputs.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <FunctionalInterrupt.h>

constexpr uint8_t MAX_ALERTS = 20;
constexpr uint8_t MAX_ALERT_LEN = 64;
bool _has_alerts = false;
char _alerts[MAX_ALERTS][MAX_ALERT_LEN];

/**
 * @brief Adds an alert to alerts list. There is a maximum of 20 alerts. If the list is full
 * the last alert will be dropped.
 * 
 * @param s - Alert to add
 */
void Display::add_alert(String s)
{
    static bool initialized = false;

    if(!initialized) { memset(_alerts, 0, sizeof(_alerts)); initialized = true; }
    memmove(&_alerts[1], &_alerts[0], (MAX_ALERTS - 1) * MAX_ALERT_LEN);        // rotate the elements by one. Last element drops out
    strncpy(_alerts[0], s.c_str(), MAX_ALERT_LEN - 1);
    _alerts[0][MAX_ALERT_LEN - 1] = '\0';
    _has_alerts = true;
}

/**
 * @brief Hides the toast from the UI
 * 
 */
void Display::hide_toast()
{
    LGFX_Sprite as(this);
    if(esp_timer_is_active(this->toast_timer))  esp_timer_stop(this->toast_timer);
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
    LGFX_Sprite *as = new LGFX_Sprite(this);
    as->setColorDepth(16);
    as->createSprite(status_width, status_height);
    as->pushImage(0, 0, status_width, status_height, (lgfx::rgb565_t*)alert_toast);
    as->setTextColor(TFT_WHITE, 0xFA49);
    as->setFont(&fonts::Font2);
    as->drawString(alert.c_str(), 10, 2);

    if(this->_toastRunner != NULL)
    {
        /// remove toast runner task and clean up before starting a new one.
        this->_toasting_break = true;
        if(esp_timer_is_active(this->toast_timer)) esp_timer_stop(this->toast_timer);
        while(this->_toastRunner != NULL) vTaskDelay(1);
        this->hide_toast();
    }
    Toast_Task_Args *args = new Toast_Task_Args{ .self = this, .toast_sprite = as};
    xTaskCreate(slide_toast_in, "SlideToastIn", 2048, args, 5, &_toastRunner);
    if(this->_toastRunner == NULL)
    {
        if (xSemaphoreTake(this->_display_mutex, portMAX_DELAY) == pdTRUE)
        {  
            as->pushSprite(STATUS_X, STATUS_Y);
            xSemaphoreGive(this->_display_mutex);
        }
        esp_timer_start_once(this->toast_timer, TOASTTIMEOUT);
        as->deleteSprite();
        delete as;
        delete args;
    }
    if(add_to_alert) this->add_alert(alert);
}

/**
 * @brief Task function running the slide in for a toast. 
 * 
 * @param args - pointer to task arguments 
 */
void Display::slide_toast_in(void *args)
{
    Toast_Task_Args *_args = reinterpret_cast<Toast_Task_Args *>(args);
    Display *_this = _args->self;
    LGFX_Sprite *_s = _args->toast_sprite;

    Logger.Info(F("... Toast task started."));    
    for(int i=479; i >= STATUS_X; i=i-4)
    {
        if (xSemaphoreTake(_this->_display_mutex, portMAX_DELAY) == pdTRUE)
        {  
            _s->pushSprite(i, STATUS_Y);
            xSemaphoreGive(_this->_display_mutex);
        }
        if(_this->_toasting_break) break;
        vTaskDelay(1);
    }
    if(!_this->_toasting_break) esp_timer_start_once(_this->toast_timer, TOASTTIMEOUT);
    
    _s->deleteSprite();
    delete _s;
    delete _args;
    _this->_toastRunner = NULL;
    _this->_toasting_break = false;
    Logger.Info(F("... Toast task completed."));
    vTaskDelete(NULL);
}

/**
 * @brief Runs the status of the badge icon
 * 
 * @param args - pointer to task arguments 
 */
void Display::alerts_badge_runner(void *args)
{
    Display *_this = reinterpret_cast<Display *>(args);
    LGFX_Sprite s(_this);

    s.setColorDepth(16);
    s.createSprite(ALERTS_BADGE_W, ALERTS_BADGE_H);
    Logger.Info(F("... Alerts badge monitoring task started."));   
    for(;;)
    {
        if(_this->_paused) { vTaskDelay(pdMS_TO_TICKS(50)); continue; }
        if(_has_alerts) s.pushImage(0, 0, ALERTS_BADGE_W, ALERTS_BADGE_H, (lgfx::rgb565_t*)alerts_active);
        else s.pushImage(0, 0, ALERTS_BADGE_W, ALERTS_BADGE_H, (lgfx::rgb565_t*)alerts_inactive);
        if (xSemaphoreTake(_this->_display_mutex, portMAX_DELAY) == pdTRUE)
        {  
            s.pushSprite(415, 143);
            xSemaphoreGive(_this->_display_mutex);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    s.deleteSprite();
    Logger.Info(F("... Alerts badge monitoring task completed.")); 
    _this->_alertBadgeRunner = NULL;
    vTaskDelete(NULL);
}

/**
 * @brief Toggles the alert screen on an off.
 * 
 */
void Display::toggle_alerts()
{
    LGFX_Sprite controls(this);

    if(!_screen == SCREENS::ALERTS)
    {
        _has_alerts = false;
        _screen = SCREENS::ALERTS;
        this->pause_tasks();
        vTaskDelay(pdMS_TO_TICKS(100));
        this->draw_canvas();
        controls.setColorDepth(16);
        controls.createSprite(127, 143);
        controls.fillSprite(TFT_BLACK);
        controls.pushImage(0, 0, 127, 143, (lgfx::rgb565_t*)alerts_controls);

    }
    else
    {
        _screen = SCREENS::MAIN;
        this->draw_canvas();
        this->resume_tasks();
    }
}
