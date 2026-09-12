// Copyright (c) Thor Schueler. All rights reserved.
// SPDX-License-Identifier: MIT

#ifndef _DISPLAY_H_
#define _DISPLAY_H_

#include "Arduino.h"
#include <LovyanGFX.hpp>
#include "../logging/SerialLogger.h"
#include <functional>

#define SCLK_PIN        18
#define MOSI_PIN        23
#define MISO_PIN        19
#define DC_PIN          21
#define CS_PANEL_PIN    5  
#define CS_TOUCH_PIN    4
#define TOUCH_IRQ_PIN   22

#define SPI_BUS_WRITE_FREQUENCY 40000000
#define SPI_BUS_READ_FREQUENCY 16000000
#define SPI_BUS_TOUCH_FREQUENCY 2000000

#define DISPLAY_WIDTH  320
#define DISPLAY_HEIGHT 480

#define TOUCH_CALIBRATION_X_MIN 175
#define TOUCH_CALIBRATION_X_MAX 3800
#define TOUCH_CALIBRATION_Y_MIN 3900
#define TOUCH_CALIBRATION_Y_MAX 250

#define ACTIVE_BUTTON_WIDTH 51
#define ACTIVE_BUTTON_HEIGHT 32
extern const uint16_t background[] PROGMEM;
extern const uint16_t active_button[] PROGMEM;
extern const uint16_t active_button_tab[] PROGMEM;
extern const uint16_t inactive_button[] PROGMEM;
extern const uint16_t inactive_button_tab[] PROGMEM;
extern const uint16_t start_icon[] PROGMEM;
extern const uint16_t engage_icon[] PROGMEM;
extern const uint16_t home_icon[] PROGMEM;
extern const uint16_t settings_icon[] PROGMEM;

typedef struct {
  uint16_t x1;
  uint16_t y1;
  uint16_t x2;
  uint16_t y2;
  String command;
  const uint16_t* icon;
} touch_area_t;

/**
 * @brief The Display class is a wrapper around the LovyanGFX library that provides an interface for 
 * initializing and interacting with the display and touch panel. It sets up the SPI bus, configures the 
 * display panel, and initializes the touch panel. It also allows for registering a callback function to 
 * handle touch events.
 *
 * It's main responsibility is to manage the various screens and the touch events that occur on those screens. It is also responsible for initializing the display 
 * and touch panel, managing the screen sequence and layouts, and providing methods to set the touch callback function. 
 */
class Display : public lgfx::LGFX_Device {
public:

    /**
     * @brief Construct a new Display object
     * 
     */
    Display();

    /**
     * @brief Destroy the Display object
     * 
     */
    ~Display();

    /**
     * @brief Initialize display and touch panel, set background
     * 
     */
    void begin();

    /**
     * @brief Set the Touch Callback method to call when a relevant touch event occurs
     * 
     * @param callback - The callback function to call when a touch event occurs
     */
    //void setTouchCallback(void (*callback)(const char*));
    void setTouchCallback(std::function<void(const char*)> callback);
  
  protected:

    /**
     * @brief Task function managing the display
     * @param args - pointer to task arguments
     */
    static void touch_runner(void* args);

    lgfx::Panel_ST7796 _panel;
    lgfx::Bus_SPI _bus;
    lgfx::Touch_XPT2046 _touch;
    std::function<void(const char*)> _callback = NULL;

  private:

    /**
     * @brief Process the touch interrupt and call the callback if set
     * 
     * @remark This method is really to determine whether the touch is relevant and 
     * what downstream action needs to be invoked. For this to work, each screen must
     * register certain areas on the screen and the associated command that should be invoked. 
     * The _callback function will be called with the relevant command to invoke if it is set. 
     * 
     */
    void IRAM_ATTR processTouchInterrupt();

    TaskHandle_t _touchRunner;
};


#endif //_DISPLAY_H_
