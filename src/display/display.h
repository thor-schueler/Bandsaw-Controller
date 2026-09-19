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

#define ACTIVE_BUTTON_WIDTH 64
#define ACTIVE_BUTTON_HEIGHT 32
#define TAB_WIDTH 10
#define TAB_HEIGHT 32

#define D_HEIGHT 32
#define D_WIDTH 14
#define D0_WIDTH 18
#define D1_WIDTH 12
#define D2_WIDTH 17
#define D3_WIDTH 18
#define D4_WIDTH 20
#define D5_WIDTH 18
#define D6_WIDTH 18
#define D7_WIDTH 17
#define D8_WIDTH 18
#define D9_WIDTH 18
#define DDOT_WIDTH 7
#define DIPM_WIDTH 33
#define FEED_X_OFFSET 18
#define FEED_Y_OFFSET 42

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
extern const uint16_t speeds_and_feeds[] PROGMEM;
extern const uint16_t speeds_and_feeds_inactive[] PROGMEM;
extern const uint16_t action[] PROGMEM;
extern const uint16_t action_inactive[] PROGMEM;
extern const uint16_t homing[] PROGMEM;
extern const uint16_t homing_title[] PROGMEM;
extern const uint16_t D0[] PROGMEM;
extern const uint16_t D1[] PROGMEM;
extern const uint16_t D2[] PROGMEM;
extern const uint16_t D3[] PROGMEM;
extern const uint16_t D4[] PROGMEM;
extern const uint16_t D5[] PROGMEM;
extern const uint16_t D6[] PROGMEM;
extern const uint16_t D7[] PROGMEM;
extern const uint16_t D8[] PROGMEM;
extern const uint16_t D9[] PROGMEM;
extern const uint16_t DDot[] PROGMEM;
extern const uint16_t DIPM[] PROGMEM;
extern const size_t homing_title_size;
extern const size_t homing_size;

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

/**
 * @brief Type to represent the touch state
 * 
 */
typedef enum TOUCH_TAB_STATE{
  ON,
  OFF,
  WAITING
} touch_tab_state_t;

static constexpr int HOMING_W = 255;
static constexpr int HOMING_H = 170;

static constexpr uint16_t LCARS_GRID   = 0x2945;
static constexpr uint16_t LCARS_BLUE   = 0x0418;
static constexpr uint16_t LCARS_CYAN   = 0x75FF;
static constexpr uint16_t LCARS_ORANGE = 0xFD20;
static constexpr uint16_t LCARS_GRAY   = 0x1082;


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
     * @brief Set the workarea title 
     * 
     * @param title_image - A pointer to an image for the title. Could be an icon or a font bitmap
     * @param title_image_size - The number of elements in the image.
     * @param title - Title string to use
     * @remark The title (if present) is written after the image (if present)
     */
    void set_workarea_title(const uint16_t* title_image, size_t title_image_size, String title);

    /**
     * @brief Manages the EMS overlay.
     * 
     * @param active - true to activate the EMS overlay, false to deactivate it. 
     */
    void ems_overlay(bool active);

    /**
     * @brief Write the speeds and feeds overlay into the display
     * 
     * @param active - true to activate the overlay, false to deactivate it. 
     * @param speed - the speed (expected in IPM)
     */
    void feeds_and_speeds_overlay(bool active, float speed);    

    /**
     * @brief manage the action overlay
     * 
     * @param active - true to activate the action overlay, false to deactivate it. 
     * @param image - A pointer to an image for the overlay. Could be an icon or a font bitmap
     * @param image_size - The number of elements in the image.
     * @param title - Title string to use
     */
    void actions_overlay(bool active, const uint16_t* title_image, size_t title_image_size, String title);  

    /**
     * @brief Starts the homing animation. Once started, the animation will run until terminated by calling 
     * Display::homing_complete()
     * 
     */
    void start_homing_animation();

    /**
     * @brief Pauses task processing. This will result in touch no longer being processed and 
     * running animations being stopped. New animations will not start. 
     * 
     */
    inline void pause_tasks() { this->_paused = true; }; 

    /**
     * @brief Resume task processing. This will result in touch being processed again and 
     * animations can now start. 
     * 
     */
    inline void resume_tasks() { this->_paused = false; }; 


    /**
     * @brief should be called when the homing is complete to terminate the homing animation.
     * 
     */
    void homing_complete() { if(this->_homing_animation != NULL) this->_homing_animation_break = true; };

  protected:

    /**
     * @brief Draws a frame for the homing animation displayed during the homing cycle
     * 
     * @param frame - the frame index to draw.
     */  
    void draw_homing_frame(uint8_t frame);

    /**
     * @brief Task function managing the display
     * @param args - pointer to task arguments
     */
    static void touch_runner(void* args);

    /**
     * @brief Task function runnign the homing animation
     * @param args - pointer to task arguments
     */
    static void homeing_animation_runner(void* args);

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

    #pragma region homing animation methods
    /**
     * @brief Draws the background of a homing frame
     * 
     */
    void draw_homing_background();

    /**
     * @brief Draws the scanning line of the homing animation
     * 
     * @param frame - the frame index of the animation sequence
     */
    void draw_homing_scanner(uint8_t frame);
    

    void draw_homing_targets(uint8_t frame);
    
    /**
     * @brief Draws the moving carriage that is being homed.
     * 
     * @param frame - the frame index of the animation sequence
     */
    void draw_homing_carriage(uint8_t frame);
    
    /**
     * @brief Draws the homing reticle
     * 
     * @param frame - the frame index of the animation sequence
     */
    void draw_homing_reticle(uint8_t frame);
    
    /**
     * @brief Draws the homing status into the frame
     * 
     * @param frame - the frame index of the animation sequence
     */
    void draw_homing_status(uint8_t frame);
    #pragma endregion


    LGFX_Sprite* _homingSprite = nullptr;
    bool _paused = false;
    bool _homing_animation_break = false;
    TaskHandle_t _touchRunner = NULL;
    TaskHandle_t _homing_animation = NULL;
    volatile SemaphoreHandle_t _display_mutex;
};


#endif //_DISPLAY_H_
