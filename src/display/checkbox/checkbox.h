// Copyright (c) Thor Schueler. All rights reserved.
// SPDX-License-Identifier: MIT

#ifndef _CHECKBOX_H_
#define _CHECKBOX_H_

#include <Arduino.h>
#include "src/display/display.h"
#include "src/display/control/control.h"

#define CHKBOX_HEIGHT 20
#define CHK_OFFSET 3
#define CHK_PADDING 5

class Checkbox : public IControl<bool>
{
    public:
        /**
         * @brief Creates a new instance of Checkbox. 
         * @param name - the name of the variable the control is bound to
         * @param description - the description of the control. Used to render informative text 
         */
        Checkbox(String name, String description) : IControl(name, description) {}
       
        /**
         * @brief Loads the control and initializes the view state
         */        
        void load(bool value) override { _value = value; _has_changes = false; }
        
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

        /**
         * @brief Redraws the volatile elements of the control into the sprite
         * 
         * @param sprite - Reference to the sprite to draw into 
         * @remark The redrawing happens using information the should be stored in the control. 
         */
        void redraw_volatiles(LGFX_Sprite &sprite) override;

        /**
         * @brief Process the touch event for this control
         * 
         * @param x - the x-coordinate for the touch event
         * @param y - the y-coordinate for the touch event
         */
        void process_touch(uint16_t x, uint16_t y) override;

        /**
         * @brief Determines if the control has changes
         */
        bool has_changes() override { return _has_changes; }

        /**
         * @brief Gets the underlying data value 
         */
        bool value() override { return _value; }

        /**
         * @brief Gets the height of the control.
         */
        uint8_t height() override { return (_y2 - _y1) > (CHKBOX_HEIGHT + CHK_PADDING) ? (_y2 - _y1) : (CHKBOX_HEIGHT + CHK_PADDING); }

    private:
        bool _value = false;
        bool _has_changes = false;

        uint8_t volatile_x = 0;
        uint8_t volatile_y = 0;
};

#endif // _CHECKBOX_H_