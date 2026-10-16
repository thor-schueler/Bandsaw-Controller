// Copyright (c) Thor Schueler. All rights reserved.
// SPDX-License-Identifier: MIT

#include "dialog.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <FunctionalInterrupt.h>

/**
 * @brief Construct a new Dialog object
 * 
 * @param parent - Pointer to the parent, which should be an LGFX_Device or derived class.
 */
Dialog::Dialog(LGFX_Device *parent) : _sprite(parent)
{
    this->_parent = parent;
    this->_mutex = xSemaphoreCreateMutex(); 
    this->_sprite.setColorDepth(16);
}

/**
 * @brief Destroy the Dialog object
 * 
 */
Dialog::~Dialog()
{
    this->hide();
    if(this->_sprite.getBuffer() != nullptr) _sprite.deleteSprite();
    if(this->_mutex != NULL)
    {
        vSemaphoreDelete(this->_mutex);
        this->_mutex = NULL;
    }
}

/**
 * @brief Shows the dialog
 * 
 * @param message - Message to display in the body of the dialog. 
 * @return true - if the user taps the Proceed button
 * @return false - if the user taps the Cancel button
 * @remark This is a blocking call. All other display and touch activity is suspended. 
 */
bool Dialog::show(const char *message)
{
    bool status = false;
    uint16_t tx = UINT16_MAX;
    uint16_t ty = UINT16_MAX;
    this->_showing.store(true);
    this->_request_exit.store(false);
    if (xSemaphoreTake(this->_mutex, portMAX_DELAY) == pdTRUE)
    { 
        if(this->_sprite.getBuffer() == nullptr) this->_sprite.createSprite(D_WIDTH, SPRITE_H); 
        
        //Logger.Info_f("Largest block: %u", heap_caps_get_largest_free_block(MALLOC_CAP_DMA));
        //Logger.Info_f("Free heap: %u", esp_get_free_heap_size());
        //Logger.Info_f("Largest block: %u", heap_caps_get_largest_free_block(MALLOC_CAP_DMA));
        //Logger.Info_f("Free heap: %u", esp_get_free_heap_size());
        //Logger.Info_f("Sprite buffer: %p", this->_sprite.getBuffer());
        for(uint8_t i=0; i< (D_HEIGHT/SPRITE_H); i++)
        {
            this->_sprite.fillSprite(TFT_BLACK);
            this->_sprite.pushImage(0, 0, D_WIDTH, SPRITE_H, (lgfx::rgb565_t*)(dialog_sprite + D_WIDTH * SPRITE_H * i));
            if(i == 1)
            {
                this->_sprite.setTextSize(1);
                this->_sprite.setTextColor(TFT_WHITE, TFT_BLACK);
                this->_sprite.setFont(&fonts::FreeSans12pt7b);
                this->draw_wrapped_text(this->_sprite, message, 50, 0, D_WIDTH - 50 -20);
            }
            if((i+1) * SPRITE_H >= D_HEIGHT)
            {
                this->_sprite.pushImage(PROCEED_X, PROCEED_Y - i * SPRITE_H, PROCEED_W, PROCEED_H, (lgfx::rgb565_t*)proceed_sprite);
                this->_sprite.pushImage(CANCEL_X, CANCEL_Y - i * SPRITE_H, CANCEL_W, CANCEL_H, (lgfx::rgb565_t*)cancel_sprite);
            }
            this->_sprite.pushSprite(SHOW_X, SHOW_Y + i* SPRITE_H);
            this->_sprite.waitDMA();
        }
       
        while(!this->_request_exit.load())
        {
            // poll the touch interface here for a proceed/cancel
            if(_parent->getTouch(&tx, &ty))
            {
                if(ty < T_B1_Y || ty > (T_B1_Y + T_B1_H) || tx < T_B1_X || tx > (T_B2_X + T_B2_W) || (tx > (T_B1_X + T_B1_W) && tx < T_B2_W )) continue;
                else if(tx > T_B1_X && tx < T_B2_X) { status = true; break; }
                else if(tx > T_B2_X) break;    
            }
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        this->_sprite.fillSprite(TFT_BLACK);
        for(uint8_t i=0; i< (D_HEIGHT/SPRITE_H); i++)
        {
            this->_sprite.pushSprite(SHOW_X, SHOW_Y + i * SPRITE_H);
            this->_sprite.waitDMA();
        }
        xSemaphoreGive(this->_mutex);
    }
    this->_showing.store(false);
    return status;
}

/**
 * @brief Can be use to exit the dialog. Essentially calling hide() meassages show() and instructs it to exist. 
 * Show will return false in this case. 
 */
void Dialog::hide() 
{ 
    if(_showing.load()) 
    { 
        _request_exit.store(true); 
        while(_showing.load()) vTaskDelay(pdMS_TO_TICKS(10)); 
        this->_request_exit.store(false); 
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
void Dialog::draw_wrapped_text(LGFX_Sprite& sprite, const char* text, int x, int y, int maxWidth)
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
