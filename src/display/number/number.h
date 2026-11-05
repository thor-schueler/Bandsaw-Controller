// Copyright (c) Thor Schueler. All rights reserved.
// SPDX-License-Identifier: MIT

#ifndef _NUMBER_H_
#define _NUMBER_H_

#include <Arduino.h>
#include "src/display/display.h"
#include "src/display/control/control.h"

#define ENTRY_HEIGHT 20
#define ENTRY_WIDTH 50
#define ENTRY_PADDING 5

template<typename T>
class Number : public IControl<T>
{
    static_assert(std::is_same_v<T, uint8_t> || std::is_same_v<T, uint16_t> || std::is_same_v<T, uint32_t>, "Unsupported control type");

    public:
        /**
         * @brief Creates a new instance of Number. 
         * @param name - the name of the variable the control is bound to
         * @param description - the description of the control. Used to render informative text 
         */
        Number(String name, String description) : IControl<T>(name, description) {}
       
        /**
         * @brief Loads the control and initializes the view state
         */        
        void load(T value) override { _value = value; _has_changes = false; }
        
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
         * @brief Processes the wheel movement. Only implement this for controls that can respond to wheel 
         * movemenet
         * 
         * @param direction - the direction of move 
         * @param steps - the number of steps taken
         */
        void process_wheel_movement(int direction, int steps) override;

        /**
         * @brief Determines if the control has changes
         */
        bool has_changes() override { return _has_changes; }

        /**
         * @brief Gets the underlying data value 
         */
        T value() override { return _value; }
        
        /**
         * @brief Gets the height of the control.
         */
        uint8_t height() override { return ENTRY_HEIGHT; }

    private:
        T _value = 0;
        bool _has_changes = false;

        uint8_t volatile_x1 = 0;
        uint8_t volatile_y1 = 0;
        uint8_t volatile_x2 = 0;
        uint8_t volatile_y2 = 0;

};

#include "number.ipp"

#endif // _NUMBER_H_