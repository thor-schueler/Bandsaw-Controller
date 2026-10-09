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
        this->_screen = SCREENS::EMS;
        if (xSemaphoreTake(this->_display_mutex, portMAX_DELAY) == pdTRUE)
        {  
            // reinitialize display to deal with any EMI state corruption. 
            this->init();
            this->setRotation(1);           // Landscape
            this->fillScreen(TFT_BLACK);

            this->pushImage(0, 0, 480, 320, (lgfx::rgb565_t*)ems);
            xSemaphoreGive(this->_display_mutex);
        } 
    }
    else
    {
        this->_screen = SCREENS::MAIN;
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
        uint8_t x = FEED_X_OFFSET;
        overlay.pushImage(0, 0, 127, 143, (lgfx::rgb565_t*)speeds_and_feeds);
        overlay.setTextColor(LCARS_ORANGE, TFT_BLACK);
        overlay.fillRect(6, 31, 104, 50, TFT_BLACK);

        x = this->write_fnumber_into_sprite(overlay, speed, x, FEED_Y_OFFSET, D_WIDTH, D_HEIGHT, DDOT_WIDTH);
        overlay.pushImage(x + 5, FEED_Y_OFFSET, DIPM_WIDTH, D_HEIGHT, (lgfx::rgb565_t*)DIPM, TFT_BLACK);
        if(this->_has_alerts) overlay.pushImage(62, 85, ALERTS_BADGE_W, ALERTS_BADGE_H, (lgfx::rgb565_t*)alerts_active);
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
    overlay.deleteSprite();
}

/**
 * @brief manage the action overlay
 * 
 * @param active - true to activate the action overlay, false to deactivate it. 
 * @param image - A pointer to an image for the overlay. Could be an icon or a font bitmap
 * @param image_size - The number of elements in the image.
 * @param title - Title string to use
 * @param bg - A pointer to the background image for the overlay. When nullptr, the default background will be used.
 */
void Display::actions_overlay(bool active, const uint16_t* image, size_t image_size, String title, const uint16_t* bg)
{
    LGFX_Sprite overlay(this);
    overlay.setColorDepth(16);
    overlay.createSprite(127, 70);
    overlay.fillSprite(TFT_BLACK);
    if(active) 
    {
        overlay.pushImage(0, 0, 127, 70, (lgfx::rgb565_t*)(bg == nullptr ? action : bg));
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
    overlay.deleteSprite();
}

/**
 * @brief Writes a number into a sprite using the LCARS front digits
 * 
 * @param sprite - reference to teh sprite
 * @param number - number to write
 * @param x - x coordinate to write at
 * @param y - y coordinate to write at
 * @param cw - character width
 * @param ch - character height
 * 
 * @returns An unsigned integer containing the x coordinate of the end of the drawn number
 */
uint16_t Display::write_unumber_into_sprite(LGFX_Sprite &sprite, uint16_t number, uint16_t x, uint16_t y, uint8_t cw, uint8_t ch)
{
    char __buf[16];
    uint16_t __x = x;
    snprintf(__buf, sizeof(__buf), "%u", number);
    for(char* __p = __buf; *__p; ++__p)
    {
        lgfx::rgb565_t* __img = nullptr;
        switch(*__p)
        {
            case '0': __img = ch == SD_HEIGHT ? (lgfx::rgb565_t*)SD0 : (lgfx::rgb565_t*)D0; break;
            case '1': __img = ch == SD_HEIGHT ? (lgfx::rgb565_t*)SD1 : (lgfx::rgb565_t*)D1; break;
            case '2': __img = ch == SD_HEIGHT ? (lgfx::rgb565_t*)SD2 : (lgfx::rgb565_t*)D2; break;
            case '3': __img = ch == SD_HEIGHT ? (lgfx::rgb565_t*)SD3 : (lgfx::rgb565_t*)D3; break;
            case '4': __img = ch == SD_HEIGHT ? (lgfx::rgb565_t*)SD4 : (lgfx::rgb565_t*)D4; break;
            case '5': __img = ch == SD_HEIGHT ? (lgfx::rgb565_t*)SD5 : (lgfx::rgb565_t*)D5; break;
            case '6': __img = ch == SD_HEIGHT ? (lgfx::rgb565_t*)SD6 : (lgfx::rgb565_t*)D6; break;
            case '7': __img = ch == SD_HEIGHT ? (lgfx::rgb565_t*)SD7 : (lgfx::rgb565_t*)D7; break;
            case '8': __img = ch == SD_HEIGHT ? (lgfx::rgb565_t*)SD8 : (lgfx::rgb565_t*)D8; break;
            case '9': __img = ch == SD_HEIGHT ? (lgfx::rgb565_t*)SD9 : (lgfx::rgb565_t*)D9; break;
        }
        if(__img == nullptr) continue;
        sprite.pushImage(__x, y, cw, ch, __img, TFT_BLACK);
         __x += cw - (cw == SD_WIDTH ? 2 : 2);
    }
    return __x;
}

/**
 * @brief Writes a number into a sprite using the LCARS front digits
 * 
 * @param sprite - reference to teh sprite
 * @param number - number to write
 * @param x - x coordinate to write at
 * @param y - y coordinate to write at
 * @param cw - character width
 * @param ch - character height
 * 
 * @returns An unsigned integer containing the x coordinate of the end of the drawn number
 */
uint16_t Display::write_fnumber_into_sprite(LGFX_Sprite &sprite, float number, uint16_t x, uint16_t y, uint8_t cw, uint8_t ch, uint8_t sw)
{
    char __buf[16];
    uint16_t __x = x;
    uint16_t __xx = cw;
    snprintf(__buf, sizeof(__buf), number < 10 ? "%.2f" : "%.1f", number);
    for(char* __p = __buf; *__p; ++__p)
    {
        lgfx::rgb565_t* __img = nullptr;
        switch(*__p)
        {
            case '0': __img = ch == SD_HEIGHT ? (lgfx::rgb565_t*)SD0 : (lgfx::rgb565_t*)D0; __xx = cw; break;
            case '1': __img = ch == SD_HEIGHT ? (lgfx::rgb565_t*)SD1 : (lgfx::rgb565_t*)D1; __xx = cw; break;
            case '2': __img = ch == SD_HEIGHT ? (lgfx::rgb565_t*)SD2 : (lgfx::rgb565_t*)D2; __xx = cw; break;
            case '3': __img = ch == SD_HEIGHT ? (lgfx::rgb565_t*)SD3 : (lgfx::rgb565_t*)D3; __xx = cw; break;
            case '4': __img = ch == SD_HEIGHT ? (lgfx::rgb565_t*)SD4 : (lgfx::rgb565_t*)D4; __xx = cw; break;
            case '5': __img = ch == SD_HEIGHT ? (lgfx::rgb565_t*)SD5 : (lgfx::rgb565_t*)D5; __xx = cw; break;
            case '6': __img = ch == SD_HEIGHT ? (lgfx::rgb565_t*)SD6 : (lgfx::rgb565_t*)D6; __xx = cw; break;
            case '7': __img = ch == SD_HEIGHT ? (lgfx::rgb565_t*)SD7 : (lgfx::rgb565_t*)D7; __xx = cw; break;
            case '8': __img = ch == SD_HEIGHT ? (lgfx::rgb565_t*)SD8 : (lgfx::rgb565_t*)D8; __xx = cw; break;
            case '9': __img = ch == SD_HEIGHT ? (lgfx::rgb565_t*)SD9 : (lgfx::rgb565_t*)D9; __xx = cw; break;
            case '.': __img = ch == SD_HEIGHT ? (lgfx::rgb565_t*)DDot : (lgfx::rgb565_t*)DDot; __xx = sw; break;
        }
        if(__img == nullptr) continue;
        sprite.pushImage(__x, y, __xx, ch, __img, TFT_BLACK);
         __x += __xx - (cw == SD_WIDTH ? 0 : 2);
    }
    return __x;
}

/**
 * @brief Draws the blade running indiator animation into the status bar.
 * 
 * @param sprite Sprite to draw into
 * @param x - the x coordinate
 * @param y - the y coordinate
 * @return uint16_t An unsigned integater denoting the x coordinate after the action.
 */
uint16_t Display::draw_blade_running_status(LGFX_Sprite &sprite, uint16_t x, uint16_t y)
{
    const static uint8_t GLYPH_W = 24;
    static bool first = true;
    static uint16_t s[GLYPH_W * STATUS_GLYPH_H];

    // evaluate whether the blade is running
    if(this->_status & 0x0100 != 0x0100) 
    {   
        first = true;                                                           // blade running flag is not set, so reset the 
        return x;                                                               // index indicator and return original x
    }

    // load the image into the sprite
    if(first)
    {
        memcpy(s, blade_running, GLYPH_W * STATUS_GLYPH_H *sizeof(uint16_t));   // first time drawing, load the image from 
        first = false;
    }
    else
    {
        for (int y = 0; y < STATUS_GLYPH_H; y++) 
        {
            uint16_t *row = &s[y * GLYPH_W];                                    // the image is 24px wide
            uint16_t tmp = row[GLYPH_W-1];                                      // save the last pixel in the row
            memmove(&row[1], &row[0], (GLYPH_W - 1) * sizeof(uint16_t));        // move one pixel to right
            row[0] = tmp;                                                       // cirlce previous last pixel to front
        }
    }
    sprite.pushImage(x, y, GLYPH_W, STATUS_GLYPH_H, (lgfx::rgb565_t*)s);
    return x + GLYPH_W + STATUS_GLYPH_SPACE;
}

/**
 * @brief Draws the lead screw running indiator animation into the status bar.
 * 
 * @param sprite Sprite to draw into
 * @param x - the x coordinate
 * @param y - the y coordinate
 * @return uint16_t An unsigned integater denoting the x coordinate after the action.
 */
uint16_t Display::draw_screw_running_status(LGFX_Sprite &sprite, uint16_t x, uint16_t y)
{
    const static uint8_t GLYPH_W = 26;
    const static uint8_t GLYPH_S_W = 19;
    static bool first = true;
    static uint16_t s[GLYPH_W * STATUS_GLYPH_H];

    // evaluate whether the blade is running
    if(this->_status & 0x0200 != 0x0200) 
    {   
        first = true;                                                           // lead screw running flag is not set, so reset the 
        return x;                                                               // index indicator and return original x
    }

    // load the image into the sprite
    if(first)
    {
        memcpy(s, feed_running_screw, GLYPH_W * STATUS_GLYPH_H *sizeof(uint16_t));   // first time drawing, load the image from 
        first = false;
    }
    else
    {
        for (int y = 0; y < STATUS_GLYPH_H; y++) 
        {
            uint16_t *row = &s[y * GLYPH_W];                                    // the image is 24px wide
            uint16_t tmp = row[GLYPH_W-1];                                      // save the last pixel in the row
            memmove(&row[1], &row[0], (GLYPH_W - 1) * sizeof(uint16_t));        // move one pixel to right
            row[0] = tmp;                                                       // cirlce previous last pixel to front
        }
    }
    sprite.pushImage(x, y, GLYPH_S_W, STATUS_GLYPH_H, (lgfx::rgb565_t*)feed_running);
    sprite.pushImage(x + GLYPH_S_W, y, GLYPH_W, STATUS_GLYPH_H, (lgfx::rgb565_t*)s);
    return x + GLYPH_S_W + GLYPH_W + STATUS_GLYPH_SPACE;
}

/**
 * @brief Draws the coolant running indiator animation into the status bar.
 * 
 * @param sprite Sprite to draw into
 * @param x - the x coordinate
 * @param y - the y coordinate
 * @return uint16_t An unsigned integater denoting the x coordinate after the action.
 */
 uint16_t Display::draw_coolant_running_status(LGFX_Sprite &sprite, uint16_t x, uint16_t y)
 {
    const static uint8_t GLYPH_W = 15;
    static bool first = true;
    static uint16_t s[GLYPH_W * STATUS_GLYPH_H];

    // evaluate whether the blade is running
    if(this->_status & 0x0400 != 0x0400) 
    {   
        first = true;                                                           // lead screw running flag is not set, so reset the 
        return x;                                                               // index indicator and return original x
    }

    // load the image into the sprite
    if(first)
    {
        memcpy(s, coolant_running, GLYPH_W * STATUS_GLYPH_H *sizeof(uint16_t)); // first time drawing, load the image from 
        first = false;
    }
    else
    {

        size_t row_size = GLYPH_W * sizeof(uint16_t);
        uint16_t tmp[GLYPH_W]; 
        memcpy(tmp, &(s[(STATUS_GLYPH_H - 1) * GLYPH_W]), row_size);            // save the last row
        memmove(&(s[GLYPH_W]), s, (STATUS_GLYPH_H - 1) * row_size);             // move rows 1 row down
        memcpy(s, tmp, row_size);                                               // copy last row to first.
    }
    sprite.pushImage(x, y, GLYPH_W, STATUS_GLYPH_H, (lgfx::rgb565_t*)s);
    return x + GLYPH_W + STATUS_GLYPH_SPACE;
 }

/**
 * @brief Draws the air blast running indiator animation into the status bar.
 * 
 * @param sprite Sprite to draw into
 * @param x - the x coordinate
 * @param y - the y coordinate
 * @return uint16_t An unsigned integater denoting the x coordinate after the action.
 */
uint16_t Display::draw_air_running_status(LGFX_Sprite &sprite, uint16_t x, uint16_t y)
{
    const static uint8_t GLYPH_W = 15;
    static bool first = true;
    static uint16_t s[GLYPH_W * STATUS_GLYPH_H];

    // evaluate whether the blade is running
    if(this->_status & 0x0800 != 0x0800) 
    {   
        first = true;                                                           // lead screw running flag is not set, so reset the 
        return x;                                                               // index indicator and return original x
    }

    // load the image into the sprite
    if(first)
    {
        memcpy(s, air_running, GLYPH_W * STATUS_GLYPH_H *sizeof(uint16_t));     // first time drawing, load the image from 
        first = false;
    }
    else
    {

        size_t row_size = GLYPH_W * sizeof(uint16_t);
        uint16_t tmp[GLYPH_W]; 
        memcpy(tmp, &(s[(STATUS_GLYPH_H - 1) * GLYPH_W]), row_size);            // save the last row
        memmove(&(s[GLYPH_W]), s, (STATUS_GLYPH_H - 1) * row_size);             // move rows 1 row down
        memcpy(s, tmp, row_size);                                               // copy last row to first.
    }
    sprite.pushImage(x, y, GLYPH_W, STATUS_GLYPH_H, (lgfx::rgb565_t*)s);
    return x + GLYPH_W + STATUS_GLYPH_SPACE;
}

/**
 * @brief Draws the stall guard status into the status bar.
 * 
 * @param sprite Sprite to draw into
 * @param x - the x coordinate
 * @param y - the y coordinate
 * @return uint16_t An unsigned integater denoting the x coordinate after the action.
 */
uint16_t Display::draw_stallguard_status(LGFX_Sprite &sprite, uint16_t x, uint16_t y)
{
    const static uint8_t GLYPH_L_W = 70;
    const static uint8_t GLYPH_W = 83;
    uint16_t _s = this->_status & 0x3000;

    // evaluate whether the blade is running
    if(_s!= 0x1000 && _s != 0x2000 && _s != 0x3000) 
    {   
        return x;                                                               // stall guard not active
    }

    uint8_t sg = uint8_t(_status & 0x00FFu);
    sprite.pushImage(x, y, GLYPH_L_W, STATUS_GLYPH_H, (lgfx::rgb565_t*)stall_margin);
    x = this->write_unumber_into_sprite(sprite, sg, x + GLYPH_L_W, y, SD_WIDTH, SD_HEIGHT);
    x += STATUS_GLYPH_SPACE;
    if(_s == 0x1000) sprite.pushImage(x, y, GLYPH_W, STATUS_GLYPH_H, (lgfx::rgb565_t*)margin_safe);
    if(_s == 0x2000) sprite.pushImage(x, y, GLYPH_W, STATUS_GLYPH_H, (lgfx::rgb565_t*)margin_stall_risk);
    if(_s == 0x3000) sprite.pushImage(x, y, GLYPH_W, STATUS_GLYPH_H, (lgfx::rgb565_t*)margin_stall);
    return x + GLYPH_W + STATUS_GLYPH_SPACE;
}

/**
 * @brief Status bar task function, respsonsible for drawing the taskbar and keeping it up-to-date
 * 
 * @param args task arguments
 */
void Display::status_bar_runner(void *args)
{
    Display *_this = static_cast<Display *>(args);
    LGFX_Sprite sb(_this);
    sb.setColorDepth(16);
    sb.createSprite(STATUS_W, STATUS_GLYPH_H);
    sb.fillSprite(TFT_BLACK);

    bool has_static_image = false;
    for(;;)
    {
        vTaskDelay(pdMS_TO_TICKS(50));        
        if(_this->_paused) continue;    // do not draw taskbar when paused
        if(_this->_screen == SCREENS::MAIN)
        {
            if(_this->_has_toasts) continue;                                        // don't draw if a toast is showing
            if(has_static_image && _this->_status & 0xff00 == 0x0000) continue;     // don't draw if static image and no action

            sb.fillSprite(TFT_BLACK);
            if(_this->_status & 0xff00 == 0x0000)
            {
                // no perations are running, use the status bar to display a simple message
                sb.setTextColor(BORG_GREEN, TFT_BLACK);
                sb.setFont(&fonts::Font2);
                sb.drawString(F("Move the carriage using the wheel"), 0, 0);
                has_static_image = true;
            }
            else
            {
                uint16_t x = _this->draw_blade_running_status(sb, 0,0);
                x = _this->draw_screw_running_status(sb, x, 0);
                x = _this->draw_coolant_running_status(sb, x, 0);
                x = _this->draw_air_running_status(sb, x, 0);
                x = _this->draw_stallguard_status(sb, x, 0);
            }
            if (xSemaphoreTake(_this->_display_mutex, portMAX_DELAY) == pdTRUE)
            { 
                sb.pushSprite(88, 281, TFT_PLUM);
                xSemaphoreGive(_this->_display_mutex);
            }
        }
        else if(_this->_screen == SCREENS::EMS) continue;
        else if(_this->_screen == SCREENS::ALERTS) continue;
        else if(_this->_screen == SCREENS::SETTINGS) continue;
    }
    sb.deleteSprite();
    _this->_statusRunner = NULL;
    vTaskDelete(NULL);
}
