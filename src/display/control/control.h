// Copyright (c) Thor Schueler. All rights reserved.
// SPDX-License-Identifier: MIT

#ifndef _CONTROL_H_
#define _CONTROL_H_

#include <Arduino.h>
#include <LovyanGFX.hpp>

class IBase
{
    public:

        /**
         * @brief Creates a new instance of IBase. Should not be called directly.  
         * @param name - the name of the variable the control is bound to
         * @param description - the description of the control. Used to render informative text 
         */
        IBase(String name, String description) : _description(description), _name(name) {}
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
        virtual uint8_t show(LGFX_Sprite &sprite, uint16_t top_x, uint16_t top_y, uint16_t width) 
        {
            this->_x1 - top_x;
            this->_y1 = top_y;
            this->_x2 = top_x + width;
            return 0;
        }

        /**
         * @brief Redraws the volatile elements of the control into the sprite
         * 
         * @param sprite - Reference to the sprite to draw into 
         * @remark The redrawing happens using information the should be stored in the control. 
         */
        virtual void redraw_volatiles(LGFX_Sprite &sprite) = 0;

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

        /**
         * @brief Process the touch event for this control
         * 
         * @param x - the x-coordinate for the touch event
         * @param y - the y-coordinate for the touch event
         */
        virtual void process_touch(uint16_t x, uint16_t y) = 0;


        /**
         * @brief Gets the name of the control, representing the variable the control is bound to.
         * 
         * @return String 
         */
        String name() { return _name; }

        /**
         * @brief Delegate to call when the control value changes
         * @param src - pointer to the control raising the event
         */
        std::function <void(IBase *src)>on_change = nullptr;

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

        uint16_t _x1 = 0;
        uint16_t _y1 = 0;
        uint16_t _x2 = 0;
        uint16_t _y2 = 0;

        String _description;
        String _name;
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


    protected: 

        /**
         * @brief Creates a new instance of control. 
         * @param name - the name of the variable the control is bound to
         * @param description - the description of the control. Used to render informative text 
         */
        IControl(String name, String description) : IBase(name, description) {}

        
};

#endif // _CONTROL_H_
