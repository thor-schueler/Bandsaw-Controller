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
    Logger.Info("drawing checkbox");
    sprite.drawRect(top_x+2, top_y+2, 10, 10, BORG_GREEN);
    if(!_value) sprite.drawRect(top_x+3, top_y+3, 8, 8, TFT_BLACK);

    uint8_t h = this->draw_wrapped_text(sprite, _description.c_str(), top_x+18, top_y+3, sprite.width()-25) + 5;
    return h > 15 ? h : 15; 
}


