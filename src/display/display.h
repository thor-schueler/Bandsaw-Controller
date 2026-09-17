// Copyright (c) Thor Schueler. All rights reserved.
// SPDX-License-Identifier: MIT

#ifndef _DISPLAY_H_
#define _DISPLAY_H_

#include "Arduino.h"
#include <LovyanGFX.hpp>
#include "src/logging/SerialLogger.h"
#include <functional>

#define SCLK_PIN        18
#define MOSI_PIN        23
#define MISO_PIN        19
#define DC_PIN          21
#define CS_PANEL_PIN    5  
#define CS_TOUCH_PIN    4
#define TOUCH_IRQ_PIN   22

#define SPI_BUS_WRITE_FREQUENCY 40000000
#define SPI_BUS_READ_FREQUENCY  4000000
#define SPI_BUS_TOUCH_FREQUENCY 2000000

#define DISPLAY_WIDTH  320
#define DISPLAY_HEIGHT 480

#define TOUCH_CALIBRATION_X_MIN 175
#define TOUCH_CALIBRATION_X_MAX 3800
#define TOUCH_CALIBRATION_Y_MIN 3900
#define TOUCH_CALIBRATION_Y_MAX 250

#define ACTIVE_BUTTON_WIDTH 51
#define ACTIVE_BUTTON_HEIGHT 32
#define TAB_WIDTH 10
#define TAB_HEIGHT 32
extern const uint16_t background[] PROGMEM;
extern const uint16_t ems[] PROGMEM;
extern const uint16_t active_button[] PROGMEM;
extern const uint16_t active_tab[] PROGMEM;
extern const uint16_t inactive_button[] PROGMEM;
extern const uint16_t inactive_tab[] PROGMEM;
extern const uint16_t pending_tab[] PROGMEM;
extern const uint16_t start_icon[] PROGMEM;
extern const uint16_t engage_icon[] PROGMEM;
extern const uint16_t home_icon[] PROGMEM;
extern const uint16_t air_icon[] PROGMEM;
extern const uint16_t lube_icon[] PROGMEM;
extern const uint16_t light_icon[] PROGMEM;
extern const uint16_t settings_icon[] PROGMEM;

#define HOMING_X 85
#define HOMING_Y 90
#define HOMING_W 265
#define HOMING_H 160

static const struct
{
uint16_t x;
uint16_t y;
} targets[] =
{
{ 62, 32 },
{ 118, 58 },
{ 152, 95 },
{ 201, 71 },
{ 234, 42 },
{ 184, 121 }
};


/**
 * @brief Structure to define the area and behavior of a touch function. 
 * 
 */
typedef struct {
  uint16_t icon_x1;
  uint16_t icon_y1;
  uint16_t icon_x2;
  uint16_t icon_y2;
  uint16_t tab_x1;
  uint16_t tab_y1;
  uint16_t tab_x2;
  uint16_t tab_y2;
  const uint16_t* icon;
  uint8_t gpio;
  bool enable;
} touch_area_t;

typedef enum TOUCH_TAB_STATE{
  ON,
  OFF,
  WAITING
} touch_tab_state_t;

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
     * @brief Draws the image background and static overlays.
     * 
     */
    void draw_canvas();

    /**
     * @brief Set the icon and background for a button
     * 
     * @param gpio - the gpio number associated with the button
     * @param on - the desired state of the tab (on, off or pending)
     * 
     * @remarks - the value of gpio reflects an actual GPIO if less than 64. Above 64 the value is asusmed to be logically 
     * only and not associated with a physical pin.
     */
    void set_button(uint8_t gpio, touch_tab_state_t state);

    /**
     * @brief Set the button tab on a particular button
     * 
     * @param gpio - the gpio number associated with the button
     * @param on - the desired state of the tab (on, off or pending)
     * 
     * @remarks - the value of gpio reflects an actual GPIO if less than 64. Above 64 the value is asusmed to be logically 
     * only and not associated with a physical pin.
     */
    void set_button_tab(uint8_t gpio, touch_tab_state_t state);

    /**
     * @brief Manages the EMS overlay.
     * 
     * @param active - true to activate the EMS overlay, false to deactivate it. 
     */
    void ems_overlay(bool active);

    void draw_homing_frame(uint8_t frame);

  protected:

    /**
     * @brief Task function managing the display
     * @param args - pointer to task arguments
     */
    static void touch_runner(void* args);

    lgfx::Panel_ST7796 _panel;
    lgfx::Bus_SPI _bus;
    lgfx::Touch_XPT2046 _touch;

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
