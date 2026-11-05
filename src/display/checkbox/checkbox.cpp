// Copyright (c) Thor Schueler. All rights reserved.
// SPDX-License-Identifier: MIT

#include "checkbox.h"
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
uint8_t Checkbox::show(LGFX_Sprite &sprite, uint16_t top_x, uint16_t top_y, uint16_t width) 
{
    IBase::show(sprite, top_x, top_y, width);
    this->volatile_x = top_x+2;
    this->volatile_y = top_y+2;

    sprite.drawRect(volatile_x, volatile_y, CHKBOX_HEIGHT, CHKBOX_HEIGHT, BORG_GREEN);
    if(_value) sprite.fillRect(volatile_x + CHK_OFFSET, volatile_y + CHK_OFFSET, CHKBOX_HEIGHT - 2*CHK_OFFSET, CHKBOX_HEIGHT - 2*CHK_OFFSET, BORG_GREEN);

    uint8_t h = this->draw_wrapped_text(sprite, _description.c_str(), volatile_x + CHKBOX_HEIGHT + 2*CHK_PADDING, volatile_y+2, sprite.width()-35) + CHK_PADDING;
    h = (h > (CHKBOX_HEIGHT + CHK_PADDING) ? h : (CHKBOX_HEIGHT + CHK_PADDING)); 
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
void Checkbox::redraw_volatiles(LGFX_Sprite &sprite) 
{
    sprite.fillRect(volatile_x, volatile_y, CHKBOX_HEIGHT, CHKBOX_HEIGHT, TFT_BLACK);
    sprite.drawRect(volatile_x, volatile_y, CHKBOX_HEIGHT, CHKBOX_HEIGHT, BORG_GREEN);
    if(_value) sprite.fillRect(volatile_x + CHK_OFFSET, volatile_y + CHK_OFFSET, CHKBOX_HEIGHT - 2*CHK_OFFSET, CHKBOX_HEIGHT - 2*CHK_OFFSET, BORG_GREEN);
    this->_has_changes = false;
}

/**
 * @brief Process the touch event for this control
 * 
 * @param x - the x-coordinate for the touch event
 * @param y - the y-coordinate for the touch event
 */
void Checkbox::process_touch(uint16_t x, uint16_t y) 
{
    if(this->_x1 > x || this->_x2 < x || this->_y1 > y || this->_y2 < y) return;
        // touch event is not for this control....

    this->_value = !this->_value;
    this->_has_changes = true;
    if(this->on_focus != nullptr) this->on_focus(this, 0x00);
    if(this->on_change != nullptr) this->on_change(this);
}
