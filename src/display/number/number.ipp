// Copyright (c) Thor Schueler. All rights reserved.
// SPDX-License-Identifier: MIT

#include "number.h"
#include <stdlib.h>

/**
 * @brief Renders the control into the viewport
 * @param sprite - the sprite to render into
 * @param top_x - Upper left x coordinate
 * @param top_y - Upper left y coordinate
 * @param width - The width available to render the control into
 * @return uint8_t the height required by the control. Can be used by the caller
 * to calculate the position of the next control
 */    
template<typename T>
uint8_t Number<T>::show(LGFX_Sprite &sprite, uint16_t top_x, uint16_t top_y, uint16_t width) 
{
    IBase::show(sprite, top_x, top_y, width);
    this->_y2 = top_y + ENTRY_HEIGHT;
    this->volatile_x1 = top_x + width - ENTRY_WIDTH - 10;
    this->volatile_y1 = top_y+2;

    uint8_t h = this->draw_wrapped_text(sprite, this->_description.c_str(), top_x, volatile_y1+2, volatile_x1 );
    int32_t l = sprite.textWidth(String(this->_value));

    sprite.drawRect(volatile_x1+5, volatile_y1, ENTRY_WIDTH, ENTRY_HEIGHT, BORG_GREEN);   
    sprite.drawNumber(this->_value, volatile_x1+ENTRY_WIDTH+2-l, volatile_y1+3);

    h = (h > (ENTRY_HEIGHT + ENTRY_PADDING) ? h : (ENTRY_HEIGHT + ENTRY_PADDING)); 
    this->_y2 = top_y + h;
    this->_has_changes = false;
    return h; 
}

/**
 * @brief Redraws the volatile elements of the control into the sprite
 * 
 * @param sprite - Reference to the sprite to draw into 
 * @remark The redrawing happens using information the should be stored in the control. 
 */
template<typename T>
void Number<T>::redraw_volatiles(LGFX_Sprite &sprite) 
{
    int32_t l = sprite.textWidth(String(this->_value));
    sprite.fillRect(volatile_x1+5, volatile_y1, ENTRY_WIDTH, ENTRY_HEIGHT, TFT_BLACK);
    sprite.drawRect(volatile_x1+5, volatile_y1, ENTRY_WIDTH, ENTRY_HEIGHT, BORG_GREEN);
    sprite.drawNumber(this->_value, volatile_x1+ENTRY_WIDTH+2-l, volatile_y1+3);
    this->_has_changes = false;
}

/**
 * @brief Process the touch event for this control
 * 
 * @param x - the x-coordinate for the touch event
 * @param y - the y-coordinate for the touch event
 */
template<typename T>
void Number<T>::process_touch(uint16_t x, uint16_t y) 
{
    if(this->_x1 > x || this->_x2 < x || this->_y1 > y || this->_y2 < y) return;
        // touch event is not for this control....

    if(this->on_focus != nullptr) this->on_focus(this, 0x10);
}

/**
 * @brief Processes the wheel movement. 
 * 
 * @param direction - the direction of move 
 * @param steps - the number of steps taken
 */
template<typename T>
void Number<T>::process_wheel_movement(int direction, int steps) 
{
    //this->_value += direction * steps;
    direction > 0 ? this->_value++ : this->_value--;
    this->_has_changes = true;
    if(this->on_change != nullptr) this->on_change(this);
}
