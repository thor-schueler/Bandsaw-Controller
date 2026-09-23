// Copyright (c) Thor Schueler. All rights reserved.
// SPDX-License-Identifier: MIT

#include "version.h"
#include "display.h"
#include "src/inputs/inputs.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <FunctionalInterrupt.h>


// -----------------------------------------------------------------------------
// Display.h
// -----------------------------------------------------------------------------

static constexpr uint16_t FEED_HISTORY_SAMPLES = 360;
static constexpr float    FEED_HISTORY_MAX_IPM = 20.0f;

float     _feedHistory[FEED_HISTORY_SAMPLES] = {0};
uint16_t  _feedHistoryIndex = 0;
uint32_t  _lastFeedSampleMs = 0;

float     _feedDisplaySpeed = 0.0f;


// -----------------------------------------------------------------------------
// Display.cpp
// -----------------------------------------------------------------------------

void Display::update_manual_feed_plot()
{
    uint32_t now = millis();

    if(now - _lastFeedSampleMs < 100)
        return;

    _lastFeedSampleMs = now;

    //float speed = _motion->feed_rate_ipm();
    float speed = 4.76;

    // Smooth display response
    _feedDisplaySpeed =
        (_feedDisplaySpeed * 0.90f) +
        (speed * 0.10f);

    _feedHistory[_feedHistoryIndex] =
        _feedDisplaySpeed;

    _feedHistoryIndex =
        (_feedHistoryIndex + 1) %
        FEED_HISTORY_SAMPLES;
}


void Display::draw_manual_feed_plot(LGFX_Sprite* sprite)
{
    constexpr int CX = 135;
    constexpr int CY = 80;

    constexpr int MAX_R = 60;

    constexpr int R1 = 15;
    constexpr int R2 = 30;
    constexpr int R3 = 45;
    constexpr int R4 = 60;

    // ---------------------------------------------------------
    // Overlay on existing grid
    // ---------------------------------------------------------

    sprite->drawCircle(CX, CY, R1, LCARS_GRID);
    sprite->drawCircle(CX, CY, R2, LCARS_GRID);
    sprite->drawCircle(CX, CY, R3, LCARS_GRID);
    sprite->drawCircle(CX, CY, R4, LCARS_GRID);

    sprite->drawFastHLine(
        CX - MAX_R,
        CY,
        MAX_R * 2,
        LCARS_GRID);

    sprite->drawFastVLine(
        CX,
        CY - MAX_R,
        MAX_R * 2,
        LCARS_GRID);

    // ---------------------------------------------------------
    // Polar history trace
    // ---------------------------------------------------------

    int prevX = 0;
    int prevY = 0;
    bool first = true;

    for(uint16_t age = 0; age < FEED_HISTORY_SAMPLES; age++)
    {
        uint16_t index =
            (_feedHistoryIndex + age) %
            FEED_HISTORY_SAMPLES;

        uint16_t prevIndex =
            (index + FEED_HISTORY_SAMPLES - 1) %
            FEED_HISTORY_SAMPLES;

        float speed = _feedHistory[index];
        float last  = _feedHistory[prevIndex];

        float radius =
            constrain(
                speed,
                0.0f,
                FEED_HISTORY_MAX_IPM)
            /
            FEED_HISTORY_MAX_IPM
            *
            MAX_R;

        float angleDeg = (float)age;
        float angleRad = angleDeg * DEG_TO_RAD;

        int x =
            CX + cosf(angleRad) * radius;

        int y =
            CY + sinf(angleRad) * radius;

        uint16_t colour = TFT_CYAN;

        float dv = speed - last;

        if(dv > 0.20f)
            colour = TFT_GREEN;
        else if(dv < -0.20f)
            colour = TFT_ORANGE;

        if(!first)
        {
            sprite->drawLine(
                prevX,
                prevY,
                x,
                y,
                colour);
        }

        prevX = x;
        prevY = y;
        first = false;
    }

    // ---------------------------------------------------------
    // Current sweep indicator
    // ---------------------------------------------------------

    float sweepAngle =
        ((float)(FEED_HISTORY_SAMPLES - 1))
        * DEG_TO_RAD;

    int sweepX =
        CX + cosf(sweepAngle) * MAX_R;

    int sweepY =
        CY + sinf(sweepAngle) * MAX_R;

    sprite->drawLine(
        CX,
        CY,
        sweepX,
        sweepY,
        TFT_WHITE);

    sprite->fillCircle(
        sweepX,
        sweepY,
        2,
        TFT_WHITE);

    // ---------------------------------------------------------
    // Numeric feed display
    // ---------------------------------------------------------

    char buf[16];
    snprintf(buf,
             sizeof(buf),
             "%.1f IPM",
             _feedDisplaySpeed);

    sprite->setTextColor(TFT_CYAN);
    sprite->drawString(
        buf,
        CX - 30,
        CY + MAX_R + 10);
}