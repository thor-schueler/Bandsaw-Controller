// Copyright (c) Thor Schueler. All rights reserved.
// SPDX-License-Identifier: MIT

#include "version.h"
#include "display.h"
#include "src/inputs/inputs.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <FunctionalInterrupt.h>

static constexpr uint16_t FEED_HISTORY_SAMPLES = 360;
static constexpr float    FEED_HISTORY_MAX_IPM = 6.0f;
static constexpr int SWEEP_WIDTH = 15;

float     _feedHistory[FEED_HISTORY_SAMPLES] = {0};
float     _feedDisplaySpeed = 0.0f;
uint16_t  _feedHistoryIndex = 0;


/**
 * @brief Helper function to fade a color
 * 
 * @param color - Color to fade
 * @param fade - The factor by which to fade the color
 * @return uint16_t - the faded color in RGB565 format
 */
static uint16_t fade565(uint16_t color, float fade)
{
    fade = constrain(fade, 0.0f, 1.0f);
    uint8_t r = ((color >> 11) & 0x1F) * fade;
    uint8_t g = ((color >> 5) & 0x3F) * fade;
    uint8_t b = ( color & 0x1F) * fade;
    return (r << 11) | (g << 5) | b;
}

/**
 * @brief Starts the feeding animation. Once started, the animation will run until terminated when the value 
 * of Display::_homing_animation_break goes true.
 * 
 * @param get_speed - fucntion to call to obtain speed when monitoring.
 */
void Display::start_feed_animation(std::function<float()> get_speed)
{
    if(this->_feed_animation != NULL) return;           // animation taskis already running.

    Feed_TaskArgs *args = new Feed_TaskArgs{ this, get_speed };
    xTaskCreatePinnedToCore(feed_animation_runner, "feedAnimationRunner", 2048, args, 1, &_feed_animation, 1);
}

/**
 * @brief updates the feed plot data
 * 
 * @param get_speed - fucntion to call to obtain speed when monitoring.
 */
void Display::update_manual_feed_data(std::function<float()> get_speed)
{
    uint32_t now = millis();
    //if(now - _lastFeedSampleMs < 100) return;

    //_lastFeedSampleMs = now;

    float speed = (get_speed == nullptr) ? 0 : get_speed();
    //float speed = 14.76;

    // Smooth display response
    //_feedDisplaySpeed = (_feedDisplaySpeed * 0.90f) + (speed * 0.10f);
    _feedDisplaySpeed = speed;

    _feedHistory[_feedHistoryIndex] = _feedDisplaySpeed;
    _feedHistoryIndex = (_feedHistoryIndex + 1) % FEED_HISTORY_SAMPLES;
}

/**
 * @brief Resets the feed plot data. 
 * 
 */
void Display::reset_feed_data()
{
    _feedHistoryIndex = 0;
    for(int i=0; i<FEED_HISTORY_SAMPLES; i++) _feedHistory[i] = 0.0f;
}

/**
 * @brief Draws the feed plot frame. 
 * 
 * @param sprite - Sprite to draw inot
 */
void Display::draw_manual_feed_plot(LGFX_Sprite* sprite)
{
    constexpr int CX = 135;
    constexpr int CY = 85;

    constexpr int MAX_R = 85;

    constexpr int R1 = 20;
    constexpr int R2 = 40;
    constexpr int R3 = 60;
    constexpr int R4 = 80;

    sprite->fillSprite(LCARS_GRAY);    

    //
    // Overlay on existing grid
    //
    this->draw_grid(sprite);
    sprite->drawCircle(CX, CY, R1, LCARS_POLAR);
    sprite->drawCircle(CX, CY, R2, LCARS_POLAR);
    sprite->drawCircle(CX, CY, R3, LCARS_POLAR);
    sprite->drawCircle(CX, CY, R4, LCARS_POLAR);

    sprite->drawFastHLine(CX - R4, CY, R4 * 2, LCARS_POLAR);
    sprite->drawFastVLine(CX, CY - R4, R4 * 2, LCARS_POLAR);

    //
    // Current sweep indicator
    //
    float sweepAngle = ((float)(FEED_HISTORY_SAMPLES - _feedHistoryIndex + 1)) * DEG_TO_RAD;
    int sweepX = CX + cosf(sweepAngle) * MAX_R;
    int sweepY = CY + sinf(sweepAngle) * MAX_R;
    for(int i = SWEEP_WIDTH; i >= 0; i--)
    {
        float angle = sweepAngle + (i * DEG_TO_RAD);
        float fade = 1.0f - ((float)i / SWEEP_WIDTH);
        uint16_t color = i == 0 ? TFT_WHITE : fade565(BORG_GREEN, fade);
        int x = CX + cosf(angle) * MAX_R;
        int y = CY + sinf(angle) * MAX_R;
        sprite->drawLine(CX, CY, x, y, color);
    }

    //
    // Polar history trace
    //
    int prevX = 0;
    int prevY = 0;
    bool first = true;

    uint16_t sweepAge = (FEED_HISTORY_SAMPLES - _feedHistoryIndex + 1) % FEED_HISTORY_SAMPLES;

    for(uint16_t age = 0; age < FEED_HISTORY_SAMPLES; age++)
    {
        uint16_t index = (_feedHistoryIndex + age) % FEED_HISTORY_SAMPLES;
        uint16_t prevIndex = (index + FEED_HISTORY_SAMPLES - 1) % FEED_HISTORY_SAMPLES;
        float speed = _feedHistory[index];
        float last  = _feedHistory[prevIndex];
        float radius = constrain(speed, 0.0f, FEED_HISTORY_MAX_IPM) / FEED_HISTORY_MAX_IPM * MAX_R;
        float angleDeg = (float)age;
        float angleRad = angleDeg * DEG_TO_RAD;
        int x = CX + cosf(angleRad) * radius;
        int y = CY + sinf(angleRad) * radius;
        uint16_t colour = TFT_CYAN;
        float dv = speed - last;

        if(dv > 0.20f) colour = TFT_GREEN;
        else if(dv < -0.20f) colour = TFT_ORANGE;

        //
        // Brightness follows sweep position
        //
        uint16_t distance = (sweepAge + FEED_HISTORY_SAMPLES - age) % FEED_HISTORY_SAMPLES;
        //float fade = 1.0f - ((float)distance / FEED_HISTORY_SAMPLES);
        float fade = ((float)distance / FEED_HISTORY_SAMPLES);

        // Keep old traces visible
        fade = 0.35f + (0.65f * fade);
        uint16_t fadedColour = fade565(colour, fade);

        if(!first) sprite->drawLine(prevX, prevY, x, y, fadedColour);
        if(age == FEED_HISTORY_SAMPLES - _feedHistoryIndex && radius > 0.0f) sprite->fillCircle(x, y, 2, TFT_GREEN);

        prevX = x;
        prevY = y;
        first = false;
    }
}
