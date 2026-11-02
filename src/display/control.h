// Copyright (c) Thor Schueler. All rights reserved.
// SPDX-License-Identifier: MIT

#ifndef _CONTROL_H_
#define _CONTROL_H_

#include <Arduino.h>
#include "display.h"

class IBase
{
    public:
        IBase() = default;
        virtual ~IBase() = default;

        IBase(const IBase&) = delete;
        IBase& operator=(const IBase&) = delete;

        /**
         * @brief Renders the control into the viewport
         * @param sprite - the sprite to render into
         * @param top_x - Upper left x coordinate
         * @param top_y - Upper left y coordinate
         * @param width - The width available to render the control into
         * @return uint8_t the height required by the control. Can be used by the caller
         * to calculate the position of the next control
         */
        virtual uint8_t show(LGFX_Sprite &sprite, uint16_t top_x, uint16_t top_y, uint16_t width) = 0;

        /**
         * @brief Gets the height of the control.
         */
        virtual uint8_t height() = 0;

        /**
         * @brief Copies the value of the control to a destination
         * @param dst - void pointer to the location where to store a copy of the value
         */
        virtual void copy_to(void* dst) = 0;

        /**
         * @brief Loads the value of the control from a source
         * @param src - void point to the location where to load the data from
         */
        virtual void copy_from(const void* src) = 0;

    protected:
    
        /**
         * @brief Draws word wrapped text into the sprite
         * 
         * @param sprite - The sprite hosting the canvas to draw into
         * @param text - The text to draw
         * @param x - The x coordinate for the start
         * @param y - The y coordinate for the end
         * @param maxWidth - The width to fit the text into 
         * 
         * @return uint8_t height of the text block that has been rendered.
         * @remarks The method will not check for vertical overrun. Vertical overrung will simply be truncated.
         */
        static uint8_t draw_wrapped_text(LGFX_Sprite& sprite, const char* text, int x, int y, int maxWidth);

};

/**
 * @brief defines an interface for a display control class for the bandsaw controller
 */
template<typename T>
class IControl : public IBase
{
    public:

        /**
         * @brief Destructs an instance and cleans up allocated resources
         */
        virtual ~IControl() = default;

        /**
         * @brief Loads the control and initializes the view state
         */
        virtual void load(T value) = 0;

        /**
         * @brief Determines if the control has changes
         */
        virtual bool has_changes() = 0;

        /**
         * @brief Gets the underlying data value 
         */
        virtual T value() = 0;

        /**
         * @brief Copies the value of the control to a destination
         * @param dst - void pointer to the location where to store a copy of the value
         */
        void copy_to(void* dst) override
        {
            *static_cast<T*>(dst) = value();
        }

        /**
         * @brief Loads the value of the control from a source
         * @param src - void point to the location where to load the data from
         */
        void copy_from(const void* src) override
        {
            const auto* value = static_cast<const T*>(src);
            load(*value);
        }

        /**
         * @brief Delegate to call when the control value changes
         */
        std::function <void(T value)>on_change = nullptr;

    protected: 

        /**
         * @brief Creates a new instance of control. 
         * @param display - 
         */
        IControl(String description) : _description(description) {}

        String _description;
};

#endif // _CONTROL_H_
