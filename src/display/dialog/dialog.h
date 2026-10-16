// Copyright (c) Thor Schueler. All rights reserved.
// SPDX-License-Identifier: MIT

#ifndef _DIALOG_H_
#define _DIALOG_H_

#include "Arduino.h"
#include <LovyanGFX.hpp>
#include "src/logging/SerialLogger.h"
#include <atomic>

#define D_HEIGHT 174
#define D_WIDTH 340
#define SHOW_X 0
#define SHOW_Y 56
#define SPRITE_H 58

#define PROCEED_X 62
#define PROCEED_Y 138
#define PROCEED_W 59
#define PROCEED_H 17
#define CANCEL_X 226
#define CANCEL_Y 138
#define CANCEL_W 59
#define CANCEL_H 17

#define T_B1_X 22
#define T_B1_Y 188
#define T_B1_W 150
#define T_B1_H 40
#define T_B2_X 180
#define T_B2_Y 188
#define T_B2_W 150
#define T_B2_H 40


extern const uint16_t dialog_sprite[] PROGMEM;
extern const uint16_t proceed_sprite[] PROGMEM;
extern const uint16_t cancel_sprite[] PROGMEM;

/**
 * @brief Creates a modal dialog in the UI. 
 * 
 */
class Dialog
{
    public: 

        /**
         * @brief Construct a new Dialog object
         * 
         * @param parent - Pointer to the parent, which should be an LGFX_Device or derived class.
         */
        Dialog(LGFX_Device *parent);

        /**
         * @brief Destroy the Dialog object
         * 
         */
        ~Dialog();

        /**
         * @brief Shows the dialog
         * 
         * @param message - Message to display in the body of the dialog. 
         * @return true - if the user taps the Proceed button
         * @return false - if the user taps the Cancel button
         * @remark This is a blocking call. All other display and touch activity is suspended. 
         */
        bool show(const char *message);

        /**
         * @brief Can be use to exit the dialog. Essentially calling hide() meassages show() and instructs it to exist. 
         * Show will return false in this case. 
         */
        void hide();

    private:

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
        void draw_wrapped_text(LGFX_Sprite& sprite, const char* text, int x, int y, int maxWidth);

        LGFX_Device* _parent;
        LGFX_Sprite _sprite;
        std::atomic<bool> _request_exit{false};
        std::atomic<bool> _showing{false};
        volatile SemaphoreHandle_t _mutex = NULL;
        
};


#endif // _DIALOG_H_