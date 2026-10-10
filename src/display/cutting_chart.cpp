// Copyright (c) Thor Schueler. All rights reserved.
// SPDX-License-Identifier: MIT

#include "display.h"
#include "src/inputs/inputs.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <FunctionalInterrupt.h>

struct POINT 
{
    uint8_t speed;
    uint8_t sg_raw;
    uint8_t sg_smooth;
    uint8_t sg_d;
};

POINT max_data {1, 1, 1, 1};
POINT data[CUTTING_W - CUTTING_CHART_OFFSET - 2*CUTTING_CHART_MARGIN];
uint32_t counter = CUTTING_W - CUTTING_CHART_OFFSET - 2*CUTTING_CHART_MARGIN;
bool first_time = true;

/**
 * @brief Starts the homing animation. Once started, the animation will run until terminated by calling 
 * Display::homing_complete()
 * 
 * @param metrics_function - The function to call to get current cutting metrics
 */
void Display::start_cutting_chart(std::function<uint32_t()> metrics_function)
{
    if(first_time) { this->reset_cutting_data(); first_time = false; }

    if(this->_cutting_chart != NULL) return;                        // animation taskis already running.
    Cut_TaskArgs *args = new Cut_TaskArgs{ this, std::move(metrics_function) };
    xTaskCreatePinnedToCore(cutting_chart_runner, "cuttingChartRunner", 2560, args, 1, &_cutting_chart, 1);
}

/**
 * @brief Draws the charts are for the cutting metrics graph
 * 
 * @param sprite - Sprite to draw into
 * @param offset - Offset of the graph area from the top of the sprite
 */
void Display::draw_cutting_chart_bg(LGFX_Sprite *sprite, uint8_t offset)
{
    //
    // Grid
    //
    sprite->fillRect(0,offset, CUTTING_W, CUTTING_H, LCARS_GRAY);
    for(int x = 0; x < CUTTING_W; x += CUTTING_GRID) sprite->drawFastVLine(x, offset, CUTTING_H, LCARS_GRID);
    for(int y = offset; y < CUTTING_H + offset; y += CUTTING_GRID) sprite->drawFastHLine(0, y, CUTTING_W, LCARS_GRID);
}

/**
 * @brief Draws the chart area for the automated cutting feed. 
 * 
 * @param sprite - Sprite to draw into.
 */
void Display::draw_cutting_chart_area(LGFX_Sprite *sprite)
{
    this->draw_cutting_chart_bg(sprite, 0);
    this->draw_cutting_chart_bg(sprite, CUTTING_H + CUTTING_GRID);
}

/**
 * @brief Draws the chart for the automated cutting feed. 
 * 
 * @param sprite - Sprite to draw into.
 */
void Display::draw_cutting_chart(LGFX_Sprite *sprite)
{
    float speed_factor = static_cast<float>(CUTTING_H - CUTTING_CHART_MARGIN*2 - CUTTING_CHART_OFFSET) / static_cast<float>(max_data.speed);
    float smooth_factor = static_cast<float>(CUTTING_H - CUTTING_CHART_MARGIN*2 - CUTTING_CHART_OFFSET) / static_cast<float>(max_data.sg_smooth);
    float raw_factor = static_cast<float>(CUTTING_H - CUTTING_CHART_MARGIN*2 - CUTTING_CHART_OFFSET) / static_cast<float>(max_data.sg_raw);
    float d_factor = static_cast<float>(CUTTING_H - CUTTING_CHART_MARGIN*2 - CUTTING_CHART_OFFSET) / static_cast<float>(max_data.sg_d);
    
    for(uint8_t i=1; i < CUTTING_W - CUTTING_CHART_OFFSET - 2*CUTTING_CHART_MARGIN; i++)
    {
        int32_t y1 = CUTTING_H - CUTTING_CHART_MARGIN - CUTTING_CHART_OFFSET - 1 - (speed_factor * data[i-1].speed);
        int32_t y2 = CUTTING_H - CUTTING_CHART_MARGIN - CUTTING_CHART_OFFSET - 1 - (speed_factor * data[i].speed);
        if(speed_factor * data[i].speed != 0 && speed_factor * data[i-1].speed != 0)
        {
            sprite->drawLine(CUTTING_CHART_MARGIN + CUTTING_CHART_OFFSET + i-1, y1 ,CUTTING_CHART_MARGIN + CUTTING_CHART_OFFSET + i, y2 , BORG_GREEN);
        }
        if(CUTTING_W - CUTTING_CHART_OFFSET - 2*CUTTING_CHART_MARGIN - i-1 == counter)
        {
            sprite->fillCircle(CUTTING_CHART_MARGIN + CUTTING_CHART_OFFSET + i, y1, 2, LCARS_ORANGE);
            sprite->drawFastVLine(CUTTING_CHART_MARGIN + CUTTING_CHART_OFFSET + i, y1,  speed_factor * data[i-1].speed + 1, LCARS_ORANGE);
        }

        y1 = CUTTING_H + CUTTING_GRID + CUTTING_H - CUTTING_CHART_MARGIN - 1 - CUTTING_CHART_OFFSET - (raw_factor * data[i-1].sg_raw);
        y2 = CUTTING_H + CUTTING_GRID + CUTTING_H - CUTTING_CHART_MARGIN - 1 - CUTTING_CHART_OFFSET - (raw_factor * data[i].sg_raw);
        if(raw_factor * data[i].sg_raw != 0 && raw_factor * data[i-1].sg_raw != 0)
        {    
            sprite->drawLine(CUTTING_CHART_MARGIN + CUTTING_CHART_OFFSET + i-1, y1 ,CUTTING_CHART_MARGIN + CUTTING_CHART_OFFSET + i, y2 , LCARS_POLAR);
        }

        y1 = CUTTING_H + CUTTING_GRID + CUTTING_H - CUTTING_CHART_MARGIN - 1 - CUTTING_CHART_OFFSET - (d_factor * data[i-1].sg_d);
        y2 = CUTTING_H + CUTTING_GRID + CUTTING_H - CUTTING_CHART_MARGIN - 1 - CUTTING_CHART_OFFSET - (d_factor * data[i].sg_d);
        if(d_factor * data[i].sg_d != 0 && d_factor * data[i-1].sg_d != 0)
        {    
            sprite->drawLine(CUTTING_CHART_MARGIN + CUTTING_CHART_OFFSET + i-1, y1 ,CUTTING_CHART_MARGIN + CUTTING_CHART_OFFSET + i, y2 , LCARS_BLUE);
        }

        y1 = CUTTING_H + CUTTING_GRID + CUTTING_H - CUTTING_CHART_MARGIN - 1 - CUTTING_CHART_OFFSET - (smooth_factor * data[i-1].sg_smooth);
        y2 = CUTTING_H + CUTTING_GRID + CUTTING_H - CUTTING_CHART_MARGIN - 1 - CUTTING_CHART_OFFSET - (smooth_factor * data[i].sg_smooth);
        if(smooth_factor * data[i].sg_smooth != 0 && smooth_factor * data[i-1].sg_smooth != 0)
        {    
            sprite->drawLine(CUTTING_CHART_MARGIN + CUTTING_CHART_OFFSET + i-1, y1 ,CUTTING_CHART_MARGIN + CUTTING_CHART_OFFSET + i, y2 , LCARS_ORANGE);
        }
        if(CUTTING_W - CUTTING_CHART_OFFSET - 2*CUTTING_CHART_MARGIN - i-1 == counter)
        {
            sprite->fillCircle(CUTTING_CHART_MARGIN + CUTTING_CHART_OFFSET + i, y1, 2, LCARS_ORANGE);
            sprite->drawFastVLine(CUTTING_CHART_MARGIN + CUTTING_CHART_OFFSET + i, y1,  smooth_factor * data[i-1].sg_smooth + 1, LCARS_ORANGE);
        }
    }
}

/**
 * @brief Adds a data point to the dataset, rotates the set by one. Oldest point is discarded
 * 
 * @param data_point the datapoint....
 *          - bit 0...7     : Feed speed in 10 thou IPM
 *          - bit 8...15    : Stallguard value (stallguard theoretically gooes to 1023, but any meaningfull value is going to be below 255)
 *          - bit 16...23   : Stallguard smoothed value
 *          - bit 24...31   : Stallguard derivative
 */
void Display::add_data_point(uint32_t data_point)
{
    POINT p;

    // decode datapoint into struct
    p.speed = static_cast<uint8_t>((data_point & 0x000000ff));
    p.sg_raw = static_cast<uint8_t>((data_point >> 8) & 0x000000ff);
    p.sg_smooth = static_cast<uint8_t>((data_point >> 16) & 0x000000ff);
    p.sg_d = static_cast<uint8_t>((data_point >> 24) & 0x000000ff);

    // update max if necessary
    if(p.speed > max_data.speed) max_data.speed = p.speed;
    if(p.sg_raw > max_data.sg_raw) max_data.sg_raw = p.sg_raw;
    if(p.sg_smooth > max_data.sg_smooth) max_data.sg_smooth = p.sg_smooth;
    if(p.sg_d > max_data.sg_d) max_data.sg_d = p.sg_d;            

    // add point to data array
    if(counter > 0)
    {
        data[CUTTING_W - CUTTING_CHART_OFFSET - 2 * CUTTING_CHART_MARGIN - counter--] = p;
    }
    else
    {
        // move data to the rigth by one point.    
        // memmove(&data[1], &data[0], (HOMING_W-CHART_OFFSET-2*CHART_MARGIN - 1) * sizeof(POINT));
        memmove(&data[0], &data[1], (CUTTING_W - CUTTING_CHART_OFFSET - 2 * CUTTING_CHART_MARGIN - 1) * sizeof(POINT));

        // add new datapoint
        data[CUTTING_W - CUTTING_CHART_OFFSET - 2 * CUTTING_CHART_MARGIN - 1] = p;
    }
}

/**
 * @brief Reset the current cutting data
 * 
 */
void Display::reset_cutting_data()
{
    memset(data, 0, sizeof(data)); 
    counter = CUTTING_W - CUTTING_CHART_OFFSET - 2*CUTTING_CHART_MARGIN;
}

/**
 * @brief Calculates the maximum values in the data series for scaling
 */
void Display::calculate_data_max()
{
    max_data = { 1, 1, 1, 1};
    for(uint8_t i=0; i<CUTTING_W - CUTTING_CHART_OFFSET - 2*CUTTING_CHART_MARGIN; i++)
    {
        if(data[i].sg_d > max_data.sg_d) max_data.sg_d = data[i].sg_d;
        if(data[i].sg_raw > max_data.sg_raw) max_data.sg_raw = data[i].sg_raw;
        if(data[i].sg_smooth > max_data.sg_smooth) max_data.sg_smooth = data[i].sg_smooth;
        if(data[i].speed > max_data.speed) max_data.speed = data[i].speed;
    }
}
