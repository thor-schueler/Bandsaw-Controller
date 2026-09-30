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
POINT data[HOMING_W-CHART_OFFSET-2*CHART_MARGIN];
uint32_t counter = HOMING_W-CHART_OFFSET-2*CHART_MARGIN;
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
    Cut_TaskArgs *args = new Cut_TaskArgs{ this, metrics_function };
    xTaskCreatePinnedToCore(cutting_chart_runner, "cuttingChartRunner", 2048, args, 1, &_cutting_chart, 1);
}

/**
 * @brief Draws the chart area for the automated cutting feed. 
 * 
 * @param sprite - Sprite to draw into.
 */
void Display::draw_cutting_chart_area(LGFX_Sprite * sprite)
{

    sprite->fillSprite(LCARS_GRAY);

    //
    // Overlay on existing grid
    //
    this->draw_grid(sprite);
    sprite->drawFastHLine(CHART_MARGIN, HOMING_H - CHART_MARGIN - CHART_OFFSET, HOMING_W - 2 * CHART_MARGIN, LCARS_CYAN);
    sprite->drawFastVLine(CHART_MARGIN + CHART_OFFSET, CHART_MARGIN, HOMING_H - 2 * CHART_MARGIN, LCARS_CYAN);
}

/**
 * @brief Draws the chart for the automated cutting feed. 
 * 
 * @param sprite - Sprite to draw into.
 */
void Display::draw_cutting_chart(LGFX_Sprite * sprite)
{
    float factor = static_cast<float>(HOMING_H - CHART_MARGIN*2 - CHART_OFFSET - 17) / static_cast<float>(max_data.speed);
    for(int i=1; i < HOMING_W-CHART_OFFSET-2*CHART_MARGIN; i++)
    {
        int32_t y1 = HOMING_H - CHART_MARGIN - CHART_OFFSET - (factor * data[i-1].speed);
        int32_t y2 = HOMING_H - CHART_MARGIN - CHART_OFFSET - (factor * data[i].speed);
        sprite->drawLine(CHART_MARGIN + CHART_OFFSET + i-1, y1 ,CHART_MARGIN + CHART_OFFSET + i, y2 , BORG_GREEN);

        y1 = HOMING_H - CHART_MARGIN - CHART_OFFSET - (factor * data[i-1].sg_raw);
        y2 = HOMING_H - CHART_MARGIN - CHART_OFFSET - (factor * data[i].sg_raw);
        sprite->drawLine(CHART_MARGIN + CHART_OFFSET + i-1, y1 ,CHART_MARGIN + CHART_OFFSET + i, y2 , LCARS_POLAR);
        
        y1 = HOMING_H - CHART_MARGIN - CHART_OFFSET - (factor * data[i-1].sg_d);
        y2 = HOMING_H - CHART_MARGIN - CHART_OFFSET - (factor * data[i].sg_d);
        sprite->drawLine(CHART_MARGIN + CHART_OFFSET + i-1, y1 ,CHART_MARGIN + CHART_OFFSET + i, y2 , LCARS_BLUE);

        y1 = HOMING_H - CHART_MARGIN - CHART_OFFSET - (factor * data[i-1].sg_smooth);
        y2 = HOMING_H - CHART_MARGIN - CHART_OFFSET - (factor * data[i].sg_smooth);
        sprite->drawLine(CHART_MARGIN + CHART_OFFSET + i-1, y1 ,CHART_MARGIN + CHART_OFFSET + i, y2 , LCARS_ORANGE);
    }

    if(counter > 0){}
    else
    {
        //sprite->drawCircle(HOMING_W-CHART_OFFSET-2*CHART_MARGIN, )
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
        data[HOMING_W-CHART_OFFSET-2*CHART_MARGIN - counter--] = p;
    }
    else
    {
        // move data to the rigth by one point.    
        // memmove(&data[1], &data[0], (HOMING_W-CHART_OFFSET-2*CHART_MARGIN - 1) * sizeof(POINT));
        memmove(&data[0], &data[1], (HOMING_W - CHART_OFFSET - 2 * CHART_MARGIN - 1) * sizeof(POINT));

        // add new datapoint
        data[HOMING_W - CHART_OFFSET - 2 * CHART_MARGIN - 1] = p;
    }
}

/**
 * @brief Reset the current cutting data
 * 
 */
void Display::reset_cutting_data()
{
    memset(data, 0, sizeof(data)); 
    counter = HOMING_W-CHART_OFFSET-2*CHART_MARGIN;
}
