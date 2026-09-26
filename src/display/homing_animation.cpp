// Copyright (c) Thor Schueler. All rights reserved.
// SPDX-License-Identifier: MIT

#include "display.h"
#include "src/inputs/inputs.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <FunctionalInterrupt.h>


/**
 * @brief Starts the homing animation. Once started, the animation will run until terminated by calling 
 * Display::homing_complete()
 * 
 */
void Display::start_homing_animation()
{
    if(this->_homing_animation != NULL) return;           // animation taskis already running.
    xTaskCreatePinnedToCore(homeing_animation_runner, "homingAnimationRunner", 2048, this, 1, &_homing_animation, 1);
}

/**
 * @brief Draws a frame for the homing animation displayed during the homing cycle
 * 
 * @param sprite - Sprite to draw into
 * @param frame - the frame index to draw. 0-254 draw actual animation frames. 255 draws a black working canvas
 */
void Display::draw_homing_frame(LGFX_Sprite *sprite, uint8_t frame)
{
    sprite->fillSprite(frame == 255 ? TFT_BLACK : LCARS_GRAY);
    if(frame < 255)
    {
        frame %= 60;
        draw_homing_background(sprite);
        draw_homing_scanner(sprite, frame);
        draw_homing_carriage(sprite, frame);
        draw_homing_reticle(sprite, frame);
        draw_homing_status(sprite, frame);
    }
    if (xSemaphoreTake(this->_display_mutex, portMAX_DELAY) == pdTRUE)
    {  
        sprite->pushSprite(88, 95);
        xSemaphoreGive(this->_display_mutex);
    }
}

/**
 * @brief Draws the grid on hte background
 * 
 * @param sprite - Sprite to draw into
 * 
 */
void Display::draw_grid(LGFX_Sprite *sprite)
{
    //
    // Grid
    //
    for(int x = 0; x < HOMING_W; x += 17) sprite->drawFastVLine(x, 0, HOMING_H, LCARS_GRID);
    for(int y = 0; y < HOMING_H; y += 17) sprite->drawFastHLine(0, y, HOMING_W, LCARS_GRID);
}

/**
 * @brief Draws the background of a homing frame
 * 
 * @param sprite - Sprite to draw into
 * 
 */
void Display::draw_homing_background(LGFX_Sprite *sprite)
{
    //
    // Grid
    //
    this->draw_grid(sprite);

    //
    // Feed axis rail
    //
    sprite->drawFastHLine(20, HOMING_H/2, HOMING_W - 40, LCARS_BLUE);

    //
    // Home indicator
    //
    sprite->fillTriangle(5, HOMING_H/2, 15, HOMING_H/2 - 10, 15, HOMING_H/2 + 10, LCARS_ORANGE);

    //
    // Pulsing beacon ring
    //
    sprite->drawCircle(18, HOMING_H/2, 6, LCARS_ORANGE);
    sprite->setTextColor(LCARS_ORANGE, LCARS_GRAY);
    sprite->drawString("REFERENCE ACQUISITION", 10, 5);
}

/**
 * @brief Draws the scanning line of the homing animation
 * 
 * @param sprite - Sprite to draw into
 * @param frame - the frame index of the animation sequence
 */
void Display::draw_homing_scanner(LGFX_Sprite *sprite, uint8_t frame)
{
    uint8_t sweepFrame = frame % 20;
    int x = (sweepFrame * (HOMING_W - 4)) / 19;

    sprite->drawFastVLine(x - 2, 0, HOMING_H, 0x2C7F);
    sprite->drawFastVLine(x - 1, 0, HOMING_H, 0x43FF);
    sprite->drawFastVLine(x, 0, HOMING_H, LCARS_CYAN);
}

/**
 * @brief Draws the moving carriage that is being homed.
 * 
 * @param sprite - Sprite to draw into
 * @param frame - the frame index of the animation sequence
 */
void Display::draw_homing_carriage(LGFX_Sprite *sprite, uint8_t frame)
{
    int x = 225 - ((220 * frame) / 59);
    sprite->fillRoundRect(x, HOMING_H/2 - 14, 24, 28, 3, LCARS_CYAN);
    sprite->fillCircle(x + 26, HOMING_H/2 - 5, 2, LCARS_ORANGE);
    sprite->fillCircle(x + 26, HOMING_H/2 + 5, 2, LCARS_ORANGE);
}

/**
 * @brief Draws the homing reticle
 * 
 * @param sprite - Sprite to draw into
 * @param frame - the frame index of the animation sequence
 */
void Display::draw_homing_reticle(LGFX_Sprite *sprite, uint8_t frame)
{
    if(frame < 20) return;

    int radius = 8 + ((frame - 20) / 2);
    if(radius > 24) radius = 24;

    constexpr int cx = 18; //152;
    constexpr int cy = HOMING_H / 2; //86;

    sprite->drawCircle(cx, cy, radius, LCARS_ORANGE);
    sprite->drawFastHLine(cx - radius - 6, cy, 5, LCARS_ORANGE);
    sprite->drawFastHLine(cx + radius + 1, cy, 5, LCARS_ORANGE);
    sprite->drawFastVLine(cx, cy - radius - 6, 5, LCARS_ORANGE);
    sprite->drawFastVLine(cx, cy + radius + 1, 5, LCARS_ORANGE);
}

/**
 * @brief Draws the homing status message into the frame
 * 
 * @param sprite - Sprite to draw into
 * @param frame - the frame index of the animation sequence
 */
void Display::draw_homing_status(LGFX_Sprite *sprite, uint8_t frame)
{
    if(frame < 20 || (frame > 30 && frame < 40) || (frame > 50 && frame < 60))
    {
        sprite->setTextColor(LCARS_CYAN, LCARS_GRAY);
        sprite->drawString("SEEKING HOME", 10, 155);
    }
}

