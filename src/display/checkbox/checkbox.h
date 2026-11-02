// Copyright (c) Thor Schueler. All rights reserved.
// SPDX-License-Identifier: MIT

#ifndef _CHECKBOX_H_
#define _CHECKBOX_H_

#include <Arduino.h>
#include "src/display/display.h"
#include "src/display/control.h"

#define CHKBOX_HEIGHT 10

class Checkbox : public IControl<bool>
{
    public:
        /**
         * @brief Creates a new instance of control. 
         */
        Checkbox(String description) : IControl(description) {}
       
        /**
         * @brief Loads the control and initializes the view state
         */        
        void load(bool value) override { _value = value; }
        
        /**
         * @brief Renders the control into the viewport
         * @param sprite - the sprite to render into
         * @param top_x - Upper left x coordinate
         * @param top_y - Upper left y coordinate
         * @param width - The width available to render the control into
         * @return uint8_t the height required by the control. Can be used by the caller
         * to calculate the position of the next control
         */    
        uint8_t show(LGFX_Sprite &sprite, uint16_t top_x, uint16_t top_y, uint16_t width) override;
        bool has_changes() override { return _has_changes; }
        bool value() override { return _value; }
        uint8_t height() override { return CHKBOX_HEIGHT; }

    private:
        bool _value;
        bool _has_changes = false;

};

#endif // _CHECKBOX_H_