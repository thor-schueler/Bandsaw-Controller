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
void Display::start_cutting_chart()
{
    if(this->_cutting_chart != NULL) return;                        // animation taskis already running.
    xTaskCreatePinnedToCore(cutting_chart_runner, "cuttingChartRunner", 2048, this, 1, &_cutting_chart, 1);
}

