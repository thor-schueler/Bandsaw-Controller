// Copyright (c) Thor Schueler. All rights reserved.
// SPDX-License-Identifier: MIT

#include "version.h"
#include "display.h"
#include "src/inputs/inputs.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <FunctionalInterrupt.h>

constexpr uint8_t MAX_ALERTS = 24;
constexpr uint8_t MAX_ALERT_LEN = 64;
uint8_t _count = 0;
uint8_t _start = 0;
uint8_t _index = 0;
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
    if(_count < MAX_ALERTS-1) _count++;
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
    this->_has_toasts = false;
}

/**
 * @brief Displays an alert toast in the UI
 * 
 * @param alert - Alert to display
 * @param add_to_alert - Whether to add the alert to alerts list. Defaults to True.
 * @param timeout - timeout for the toast in microseconds. Defaults to 3'000'000. 
 */
void Display::show_toast(String alert, bool add_to_alert, uint32_t timeout)
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
        while(this->_toastRunner != NULL) vTaskDelay(1);
        if(esp_timer_is_active(this->toast_timer)) esp_timer_stop(this->toast_timer);
        this->hide_toast();
    }
    this->_has_toasts = true;
    Toast_Task_Args *args = new Toast_Task_Args{ .self = this, .toast_sprite = as, .timeout = timeout};
    xTaskCreate(slide_toast_in, "SlideToastIn", 2048, args, 5, &_toastRunner);
    if(this->_toastRunner == NULL)
    {
        if (xSemaphoreTake(this->_display_mutex, portMAX_DELAY) == pdTRUE)
        {  
            as->pushSprite(STATUS_X, STATUS_Y);
            xSemaphoreGive(this->_display_mutex);
        }
        esp_timer_start_once(this->toast_timer, timeout);
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
    if(!_this->_toasting_break) esp_timer_start_once(_this->toast_timer, _args->timeout);
    
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
        if(_this->_has_alerts) s.pushImage(0, 0, ALERTS_BADGE_W, ALERTS_BADGE_H, (lgfx::rgb565_t*)alerts_active);
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
    if(_screen != SCREENS::ALERTS)
    {
        _has_alerts = false;
        _screen = SCREENS::ALERTS;
        this->pause_tasks();
        vTaskDelay(pdMS_TO_TICKS(100));
        this->draw_canvas();
        this->set_workarea_title(alerts_title, alerts_title_size, "");
        controls.setColorDepth(16);
        controls.createSprite(127, 143);
        controls.fillSprite(TFT_BLACK);
        controls.pushImage(0, 0, 127, 143, (lgfx::rgb565_t*)alerts_controls);
        if (xSemaphoreTake(this->_display_mutex, portMAX_DELAY) == pdTRUE)
        { 
            controls.pushSprite(353, 58);
            xSemaphoreGive(this->_display_mutex);
        }
        controls.deleteSprite();
        _start = 0;
        _index = 0;
        this->write_alerts(_start, ALERTS_PAGE_SIZE, _index);
    }
    else
    {
        _screen = SCREENS::MAIN;
        this->draw_canvas();
        this->resume_tasks();
    }
}

/**
 * @brief Clears the alerts cache
 */
void Display::clear_alerts()
{
    memset(_alerts, 0, sizeof(_alerts));
    _has_alerts = false;
    _count = 0;
    _start = 0;
    _index = 0;
    this->write_alerts(_start, ALERTS_PAGE_SIZE, _index);
}

/**
 * @brief Writes the alerts to the display starting from the specified index.
 * 
 * @param start - The starting index of the alerts to display.
 * @param size - The number of alerts to display.
 * @param index - The index of the alert to highlight
 */
void Display::write_alerts(uint8_t start, uint8_t size, uint8_t index)
{
    LGFX_Sprite s(this);
    s.setColorDepth(16);
    s.createSprite(HOMING_W, HOMING_H);
    s.fillSprite(TFT_BLACK);
    s.setTextColor(BORG_GREEN, TFT_BLACK);
    s.setFont(&fonts::Font0);
    
    if(start >= _count)
    {
        s.setTextSize(2);
        this->draw_wrapped_text(s, "There are no alerts to display.", 5, 15, HOMING_W-10);
        if (xSemaphoreTake(this->_display_mutex, portMAX_DELAY) == pdTRUE)
        {  
            s.pushSprite(88, 95);
            xSemaphoreGive(this->_display_mutex);
        }
    }
    else
    {
        s.setTextSize(1);
        for(uint8_t i = start; i < start + size && i < MAX_ALERTS && i < _count; i++) 
        {
            if( i - start != index) s.setTextColor(BORG_GREEN, TFT_BLACK);
            else
            {
                s.fillRect(2, (i-start)* 15, HOMING_W, 15, TFT_BLACK);
                s.setTextColor(LCARS_ORANGE, TFT_BLACK);
            }
            s.drawString(this->truncate_string(s, _alerts[i], HOMING_W-10), 5, 5 + (i-start)* 15); 
        }
        s.drawFastHLine(5, ALERTS_DETAIL_H, HOMING_W-10, BORG_GREEN);
        if(index < size && start + index < _count)
        {
            s.setTextSize(2);
            s.setTextColor(LCARS_ORANGE, TFT_BLACK);
            this->draw_wrapped_text(s, _alerts[start+index], 5, ALERTS_DETAIL_H+15, HOMING_W-10);
            s.setTextSize(1);
        }
        if (xSemaphoreTake(this->_display_mutex, portMAX_DELAY) == pdTRUE)
        {  
            s.pushSprite(88, 95);
            xSemaphoreGive(this->_display_mutex);
        }

        uint8_t page_number = start / ALERTS_PAGE_SIZE + 1;
        uint8_t total_pages = _count / ALERTS_PAGE_SIZE + 1;
        s.deleteSprite();
        s.createSprite(50, 20);
        s.setTextColor(BORG_GREEN, TFT_BLACK);
        s.drawString("Page " + String(page_number) + "/" + String(total_pages), 0,0);
        if (xSemaphoreTake(this->_display_mutex, portMAX_DELAY) == pdTRUE)
        {  
            s.pushSprite(390, 250);
            xSemaphoreGive(this->_display_mutex);
        }
    }
    s.deleteSprite();
}

/**
 * @brief Pages the alerts page forward or backward
 * 
 * @param forward - true to page forward, false to page backward
 */
void Display::page_alerts(bool forward)
{
    if((forward && (_start + ALERTS_PAGE_SIZE >= _count)) || (!forward && (_start < ALERTS_PAGE_SIZE))) this->show_toast("There are no more alerts pages to show", false);
    else
    {
        if(forward) _start += ALERTS_PAGE_SIZE;
        if(!forward) _start -= ALERTS_PAGE_SIZE;
        _index = 0;
        this->write_alerts(_start, ALERTS_PAGE_SIZE, _index);
    }
}

/**
 * @brief Draws word wrapped text into the sprite
 * 
 * @param sprite - The sprite hosting the canvas to draw into
 * @param text - The text to draw
 * @param x - The x coordinate for the start
 * @param y - The y coordinate for the end
 * @param maxWidth - The width to fit the text into 
 * 
 * @remarks The method will not check for vertical overrun. Vertical overrung will simply be truncated.
 */
void Display::draw_wrapped_text(LGFX_Sprite& sprite, const char* text, int x, int y, int maxWidth)
{
    String t(text);
    String line;
    int lineHeight = sprite.fontHeight();
    int currentY = y;
    int start = 0;

    while (start < t.length())
    {
        int end = t.indexOf(' ', start);
        if (end == -1) end = t.length();

        String word = t.substring(start, end);
        String candidate = line.length() ? line + " " + word : word;

        if (sprite.textWidth(candidate) <= maxWidth) line = candidate;
        else
        {
            sprite.drawString(line, x, currentY);
            currentY += lineHeight + 2;
            line = word;
        }
        start = end + 1;
    }
    if (line.length()) sprite.drawString(line, x, currentY);
}

/**
 * @brief Truncates a string with an ellipsis until it fits into a certain space.
 * 
 * @param sprite - The sprite hosting the canvas to draw into
 * @param text - The text to operate on 
 * @param width - The width for the string to fit into
 * @return String - the truncated string
 */
String Display::truncate_string(lgfx::LGFX_Sprite &sprite, const char* text, int width)
{
    String result(text);
    if (sprite.textWidth(result) <= width || result.length() < 3) return result;

    result.remove(result.length() - 3);
    while (result.length())
    {
        if (sprite.textWidth(result + "...") <= width) return result + "...";
        result.remove(result.length() - 1);
    }
    return "...";
}

/**
 * @brief Processes wheel movement events and takes the appropriate actions depending on the display state.
 * @param direction - the direction of the wheel movement.
 * @param steps - the number of steps moved.
 */
void Display::process_wheel_movement(int direction, int steps) 
{ 
    if(this->_screen == SCREENS::ALERTS) 
    {
        if(_count > 0)
        {
            uint8_t bounds = _count - _start < ALERTS_PAGE_SIZE ? _count - _start : ALERTS_PAGE_SIZE;
            _index = (bounds + _index + direction) % bounds;
            this->write_alerts(_start, ALERTS_PAGE_SIZE, _index);
        }
    }
}
