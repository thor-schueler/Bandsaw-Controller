// Copyright (c) Thor Schueler. All rights reserved.
// SPDX-License-Identifier: MIT

#include "display.h"
#include "src/inputs/inputs.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <FunctionalInterrupt.h>


/**
 * @brief Manages the EMS overlay.
 * 
 * @param active - true to activate the EMS overlay, false to deactivate it. 
 */
void Display::ems_overlay(bool active)
{
    if(active)
    {
        if (xSemaphoreTake(this->_display_mutex, portMAX_DELAY) == pdTRUE)
        {         
            this->pushImage(0, 0, 480, 320, (lgfx::rgb565_t*)ems);
            xSemaphoreGive(this->_display_mutex);
        } 
    }
    else
    {
        this->draw_canvas();
    }
}

/**
 * @brief Write the speeds and feeds overlay into the display
 * 
 * @param active - true to activate the EMS overlay, false to deactivate it.
 * @param speed - the initial speed (expected in IPM) 
 * @param monitor - true to continuously monitor and update the speed
 * @param monitor - true to continuously monitor and update the speed
 * @param get_speed - fucntion to call to obtain speed when monitoring.
 */
void Display::feeds_and_speeds_overlay(bool active, float speed, bool monitor, std::function<float()> get_speed)
{
    if(active)
    {
        if(!monitor) this->fas_overlay(active, speed);
        else
        {
            if(_fas_runner != NULL) Logger.Info(F("... Feeds and Speeds Monitoring task already exists. Skipping..."));
            else
            {
                Logger.Info(F("... Creating Feeds and Speeds Monitoring task."));
                this->_fas_break = false;
                FAS_TaskArgs* args = new FAS_TaskArgs { this, std::move(get_speed) };
                xTaskCreatePinnedToCore(fas_runner, "Feeds and Speeds Watcher", 4096, args, 1, &_fas_runner, 1);
            }
        }
    }
    else
    {
        if(_fas_runner == NULL) this->fas_overlay(active, speed);   // no fas runner, so treat this as a one time invocation
        else
        {
            Logger.Info(F("... Sending termination request to Feeds and Speeds Monitoring task."));
            this->_fas_break = true;
        }
    }
}

/**
 * @brief Intenral method to Write the speeds and feeds overlay into the display
 * 
 * @param active - true to activate the EMS overlay, false to deactivate it.
 * @param speed - the speed (expected in IPM) 
 */
void Display::fas_overlay(bool active, float speed)
{
    LGFX_Sprite overlay(this);
    overlay.setColorDepth(16);
    overlay.createSprite(127, 143);
    overlay.fillSprite(TFT_BLACK);
    if(active) 
    {
        char buf[16];
        uint8_t xx = 0;
        uint8_t x = FEED_X_OFFSET;
        snprintf(buf, sizeof(buf), speed < 10 ? "%.2f" : "%.1f", speed);
        overlay.pushImage(0, 0, 127, 143, (lgfx::rgb565_t*)speeds_and_feeds);
        overlay.setTextColor(LCARS_ORANGE, TFT_BLACK);
        overlay.fillRect(6, 31, 104, 50, TFT_BLACK);

        for(char* p = buf; *p; ++p)
        {
            lgfx::rgb565_t* img = nullptr;
            switch(*p)
            {
                case '0': img = (lgfx::rgb565_t*)D0; xx = D_WIDTH; break;
                case '1': img = (lgfx::rgb565_t*)D1; xx = D_WIDTH; break;
                case '2': img = (lgfx::rgb565_t*)D2; xx = D_WIDTH; break;
                case '3': img = (lgfx::rgb565_t*)D3; xx = D_WIDTH; break;
                case '4': img = (lgfx::rgb565_t*)D4; xx = D_WIDTH; break;
                case '5': img = (lgfx::rgb565_t*)D5; xx = D_WIDTH; break;
                case '6': img = (lgfx::rgb565_t*)D6; xx = D_WIDTH; break;
                case '7': img = (lgfx::rgb565_t*)D7; xx = D_WIDTH; break;
                case '8': img = (lgfx::rgb565_t*)D8; xx = D_WIDTH; break;
                case '9': img = (lgfx::rgb565_t*)D9; xx = D_WIDTH; break;
                case '.': img = (lgfx::rgb565_t*)DDot; xx = DDOT_WIDTH; break;
            }
            if(img == nullptr) continue;
            overlay.pushImage(x, FEED_Y_OFFSET, xx, D_HEIGHT, img, TFT_BLACK);
            x += xx - 2;
        }
        overlay.pushImage(x + 5, FEED_Y_OFFSET, DIPM_WIDTH, D_HEIGHT, (lgfx::rgb565_t*)DIPM, TFT_BLACK);
    }
    else
    {
        overlay.pushImage(0, 0, 127, 143, (lgfx::rgb565_t*)speeds_and_feeds_inactive);
    }
    if (xSemaphoreTake(this->_display_mutex, portMAX_DELAY) == pdTRUE)
    { 
        overlay.pushSprite(353, 58);
        xSemaphoreGive(this->_display_mutex);
    }
}

/**
 * @brief manage the action overlay
 * 
 * @param active - true to activate the action overlay, false to deactivate it. 
 * @param image - A pointer to an image for the overlay. Could be an icon or a font bitmap
 * @param image_size - The number of elements in the image.
 * @param title - Title string to use
 */
void Display::actions_overlay(bool active, const uint16_t* image, size_t image_size, String title)
{
    LGFX_Sprite overlay(this);
    overlay.setColorDepth(16);
    overlay.createSprite(127, 70);
    overlay.fillSprite(TFT_BLACK);
    if(active) 
    {
        overlay.pushImage(0, 0, 127, 70, (lgfx::rgb565_t*)action);
        if(image != nullptr) overlay.pushImage(0, 0, image_size/70, 70, (lgfx::rgb565_t*)image, TFT_BLACK);
        if(!title.isEmpty()) 
        {
            overlay.setTextColor(TFT_WHITE);
            overlay.setFont(&fonts::FreeSans9pt7b);
            overlay.drawCenterString(title, 60, 26);
        } 
    }
    else
    {
        overlay.pushImage(0, 0, 127, 70, (lgfx::rgb565_t*)action_inactive);
    }
    if (xSemaphoreTake(this->_display_mutex, portMAX_DELAY) == pdTRUE)
    { 
        overlay.pushSprite(353, 201);
        xSemaphoreGive(this->_display_mutex);
    }
}

