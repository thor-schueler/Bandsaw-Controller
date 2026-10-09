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

#define SPI_BUS_WRITE_FREQUENCY 20000000
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
#define SD_HEIGHT 20
#define SD_WIDTH 9
#define DDOT_WIDTH 7
#define DIPM_WIDTH 33
#define FEED_X_OFFSET 18
#define FEED_Y_OFFSET 42

#define STATUS_X 79
#define STATUS_Y 281
#define ALERTS_BADGE_W 50
#define ALERTS_BADGE_H 27
#define ALERTS_PAGE_SIZE 6
#define ALERTS_DETAIL_H 100
#define TOASTTIMEOUT 3'000'000

#define STATUS_GLYPH_H 20
#define STATUS_GLYPH_SPACE 10
#define STATUS_W 320

#pragma region asset pointers
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
extern const uint16_t action_green[] PROGMEM;
extern const uint16_t action_blue[] PROGMEM;
extern const uint16_t homing[] PROGMEM;
extern const uint16_t homing_title[] PROGMEM;
extern const uint16_t manual_feeding_title[] PROGMEM;
extern const uint16_t cutting_title[] PROGMEM;
extern const uint16_t blade_on[] PROGMEM;
extern const uint16_t cutting[] PROGMEM;
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
extern const uint16_t SD0[] PROGMEM;
extern const uint16_t SD1[] PROGMEM;
extern const uint16_t SD2[] PROGMEM;
extern const uint16_t SD3[] PROGMEM;
extern const uint16_t SD4[] PROGMEM;
extern const uint16_t SD5[] PROGMEM;
extern const uint16_t SD6[] PROGMEM;
extern const uint16_t SD7[] PROGMEM;
extern const uint16_t SD8[] PROGMEM;
extern const uint16_t SD9[] PROGMEM;
extern const uint16_t DDot[] PROGMEM;
extern const uint16_t DIPM[] PROGMEM;
extern const uint16_t alerts_active[] PROGMEM;
extern const uint16_t alerts_inactive[] PROGMEM;
extern const uint16_t alert_toast[] PROGMEM;
extern const uint16_t alert_toast_inactive[] PROGMEM;
extern const uint16_t alerts_controls[] PROGMEM;
extern const uint16_t alerts_title[] PROGMEM;
extern const uint16_t status[] PROGMEM;
extern const uint16_t blade_running[] PROGMEM;
extern const uint16_t feed_running[] PROGMEM;
extern const uint16_t feed_running_screw[] PROGMEM;
extern const uint16_t coolant_running[] PROGMEM;
extern const uint16_t air_running[] PROGMEM;
extern const uint16_t stall_margin[] PROGMEM;
extern const uint16_t margin_stall_risk[] PROGMEM;
extern const uint16_t margin_stall[] PROGMEM;
extern const uint16_t margin_safe[] PROGMEM;

extern const size_t homing_title_size;
extern const size_t homing_size;
extern const size_t manual_feeding_title_size;
extern const size_t cutting_title_size;
extern const size_t blade_on_size; 
extern const size_t cutting_size;
extern const size_t alerts_title_size;

static constexpr uint16_t status_width = 401;
static constexpr uint16_t status_height = 21;
#pragma endregion

typedef enum SCREENS 
{
  MAIN = 0,
  ALERTS = 1,
  SETTINGS = 2, 
  EMS = 3
} screens_t;

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

typedef struct
{
    touch_area_t* areas;
    uint8_t count;
} touch_screen_t;

/**
 * @brief Type to represent the touch state
 * 
 */
typedef enum TOUCH_TAB_STATE{
  ON,
  OFF,
  WAITING
} touch_tab_state_t;

typedef enum STATUS_OP {
  SET,
  CLEAR
} status_op_t;

static constexpr uint8_t HOMING_W = 255;
static constexpr uint8_t HOMING_H = 170;
static constexpr uint8_t CUTTING_W = 255;
static constexpr uint8_t CUTTING_H = 75;
static constexpr uint8_t CUTTING_GRID = 15;
static constexpr uint8_t CUTTING_CHART_MARGIN = CUTTING_GRID;
static constexpr uint8_t CUTTING_CHART_OFFSET = 0;

static constexpr uint8_t CHART_MARGIN = 0;
static constexpr uint8_t CHART_OFFSET = 17;
static constexpr uint8_t CHART_GRID = 17;
static constexpr int POLAR_PLOT_OFFSET = 90;

static constexpr uint16_t BORG_GREEN   = 0x3666;
static constexpr uint16_t LCARS_POLAR  = 0x4228;
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
     * @brief Clears the alerts cache
     */
    void clear_alerts();

    /**
     * @brief Adds an alert to alerts list. There is a maximum of 20 alerts. If the list is full
     * the last alert will be dropped.
     * 
     * @param s - Alert to add
     */
    void add_alert(String s);

    /**
     * @brief Displays an alert toast in the UI
     * 
     * @param alert - Alert to display
     * @param add_to_alert - Whether to add the alert to alerts list. Defaults to True.
     * @param timeout - timeout for the toast in seconds. Defaults to 3'000'000. 
     */
    void show_toast(String alert, bool add_to_alert=true, uint32_t timeout=TOASTTIMEOUT);

    /**
     * @brief Hides the toast from the UI
     * 
     */
    void hide_toast();

    /**
     * @brief Draws the taskbar.
     * 
     */
    void draw_status_bar();

    /**
     * @brief Sets or clears the flags of the status value.
     * @param op - the operation to perform. SET or CLEAR.
     * @param value - the bit mask for the flags
     * @remark the bit mask has the following meaning:
     *        bit 1 - blade is running
     *        bit 2 - feed is engaged
     *        bit 3 - coolant is engaged
     *        bit 4 - airblast is engaged
     *        bit 5,6 - stall guard magins: 
     *            00 - stall guard off
     *            01 - stall guard margin safe
     *            10 - stall guard margin at risk
     *            11 - stall guard stall detected
     */
    void set_status_flags(status_op_t op, uint8_t value) 
    {
      if(op == STATUS_OP::SET) this->_status |= ((uint16_t)value) << 8; else this->_status &= ~(((uint16_t)value) << 8);;
    }

    /**
     * @brief Sets the value portion of the status value. The value portion contains 
     * the stall guard value. 
     * 
     * @param value - value to set
     */
    void set_status_value(uint8_t value) { this->_status = (this->_status & 0xFF00) | value; }

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
     * @param speed - the initial speed (expected in IPM)
     * @param monitor - true to continuously monitor and update the speed
     * @param get_speed - fucntion to call to obtain speed when monitoring.
     */
    void feeds_and_speeds_overlay(bool active, float speed=0, bool monitor=true, std::function<float()> get_speed=NULL);    

    /**
     * @brief manage the action overlay
     * 
     * @param active - true to activate the action overlay, false to deactivate it. 
     * @param image - A pointer to an image for the overlay. Could be an icon or a font bitmap
     * @param image_size - The number of elements in the image.
     * @param title - Title string to use
     * @param bg - A pointer to the background image for the overlay. When nullptr, the default background will be used.
     */
    void actions_overlay(bool active, const uint16_t* title_image = nullptr, size_t title_image_size = 0, String title = "", const uint16_t* bg = nullptr);  

    /**
     * @brief Starts the homing animation. Once started, the animation will run until terminated by calling 
     * Display::homing_complete()
     * 
     */
    void start_homing_animation();

    /**
     * @brief Starts the feeding animation. Once started, the animation will run until terminated by calling 
     * Display::feeding_complete()
     * 
     * @param get_speed - fucntion to call to obtain speed when monitoring.
     */
    void start_feed_animation(std::function<float()> get_speed=NULL);

    /**
     * @brief Starts the cutting with autofeed. Once started, the chart will run until terminated by calling 
     * Display::cutting_complete()
     * 
     * @param metrics_function - The function to call to get current cutting metrics
     */
    void start_cutting_chart(std::function<uint32_t()> metrics_function);
    
    /**
     * @brief Pauses task processing. This will result in touch no longer being processed and 
     * running animations being stopped. New animations will not start. 
     * 
     */
    void pause_tasks() { this->_paused = true; }; 

    /**
     * @brief Resume task processing. This will result in touch being processed again and 
     * animations can now start. 
     * 
     */
    void resume_tasks() { this->_paused = false; }; 

    /**
     * @brief should be called when the homing is complete to terminate the homing animation.
     * @param toast - true to show a toast indicating that the homing is complete, false to not show a toast.
     */
    void homing_complete(bool toast = true) 
    { 
      if(this->_homing_animation != NULL) this->_homing_animation_break = true; 
      if(toast) this->show_toast(F("Homing completed successfully."), false);
    };

    /**
     * @brief should be called when the feeding is complete to terminate the feeding animation.
     * 
     */
    void feeding_complete() { if(this->_feed_animation != NULL) this->_feed_animation_break = true; };


    /**
     * @brief Suspends the cutting chart. This is useful to freeze the chart on screen, especially after
     * the cutting has completed. 
     * 
     * @param suspend - true to suspend (default), false to resume the chart. 
     */
    void suspend_cutting_chart(bool suspend = true) { this->_suspend_cutting_chart = suspend; }

    /**
     * @brief should be called when the cutting with autofeed is complete to terminate the cutting chart.
     * @param toast - true to show a toast indicating that the cutting is complete, false to not show a toast.  
     */
    void cutting_complete(bool toast = true) 
    { 
      if(this->_cutting_chart != NULL) this->_cutting_chart_break = true; 
      if(toast) this->show_toast(F("Automated cutting completed successfully."), false);
    };

    /**
     * @brief Toggles the alert screen on an off.
     * 
     */
    void toggle_alerts();

    /**
     * @brief Pages the alerts page forward or backward
     * 
     * @param forward - true to page forward, false to page backward
     */
    void page_alerts(bool forward);

    /**
     * @brief Processes wheel movement events and takes the appropriate actions depending on the display state.
     * @param direction - the direction of the wheel movement.
     * @param steps - the number of steps moved.
     */
    void process_wheel_movement(int direction, int steps);

    /**
     * @brief Logs the usage of the various task stacks to the console
     * 
     */
    void log_stack_usage();

  protected:

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

    /**
     * @brief Task function monitoring the speed and updating the feeds and speeds panel
     * @param args - pointer to task arguments
     */
    static void fas_runner(void* args);

    /**
     * @brief Task function runnign the feed animation
     * @param args - pointer to task arguments
     */    
    static void feed_animation_runner(void* args);

    /**
     * @brief Task function runnign the cutting chart during the cutting operation
     * @param args - pointer to task arguments
     */    
    static void cutting_chart_runner(void* args);

    /**
     * @brief Task function running the slide in for a toast. 
     * 
     * @param args - pointer to task arguments 
     */
    static void slide_toast_in(void *args);

    /**
     * @brief Runs the status of the badge icon
     * 
     * @param args - pointer to task arguments 
     */
    static void alerts_badge_runner(void *args);

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

    /**
     * @brief Intenral method to Write the speeds and feeds overlay into the display
     * 
     * @param active - true to activate the EMS overlay, false to deactivate it.
     * @param speed - the speed (expected in IPM) 
     */
    void fas_overlay(bool active, float speed);

    /**
     * @brief Writes a number into a sprite using the LCARS front digits
     * 
     * @param sprite - reference to teh sprite
     * @param number - number to write
     * @param x - x coordinate to write at
     * @param y - y coordinate to write at
     * @param cw - character width
     * @param ch - character height
     * 
     * @returns An unsigned integer containing the x coordinate of the end of the drawn number
     */
    uint16_t write_unumber_into_sprite(LGFX_Sprite &sprite, uint16_t number, uint16_t x, uint16_t y, uint8_t cw, uint8_t ch);

    /**
     * @brief Writes a number into a sprite using the LCARS front digits
     * 
     * @param sprite - reference to teh sprite
     * @param number - number to write
     * @param x - x coordinate to write at
     * @param y - y coordinate to write at
     * @param cw - character width
     * @param ch - character height
     * 
     * @returns An unsigned integer containing the x coordinate of the end of the drawn number
     */
    uint16_t write_fnumber_into_sprite(LGFX_Sprite &sprite, float number, uint16_t x, uint16_t y, uint8_t cw, uint8_t ch, uint8_t sw);

    /**
     * @brief Draws the grid on hte background
     * 
     * @param sprite - Sprite to draw into
     * 
     */
    void draw_grid(LGFX_Sprite *sprite);

    #pragma region homing animation methods
    /**
     * @brief Draws the background of a homing frame
     * 
     * @param sprite - Sprite to draw into
     * 
     */
    void draw_homing_background(LGFX_Sprite *sprite);

    /**
     * @brief Draws a frame for the homing animation displayed during the homing cycle
     * 
     * @param sprite - Sprite to draw into
     * @param frame - the frame index to draw.
     */  
    void draw_homing_frame(LGFX_Sprite *sprite, uint8_t frame);

    /**
     * @brief Draws the scanning line of the homing animation
     * 
     * @param sprite - Sprite to draw into
     * @param frame - the frame index of the animation sequence
     */
    void draw_homing_scanner(LGFX_Sprite *sprite, uint8_t frame);
    
    /**
     * @brief Draws the moving carriage that is being homed.
     * 
     * @param sprite - Sprite to draw into
     * @param frame - the frame index of the animation sequence
     */
    void draw_homing_carriage(LGFX_Sprite *sprite, uint8_t frame);
    
    /**
     * @brief Draws the homing reticle
     * 
     * @param sprite - Sprite to draw into
     * @param frame - the frame index of the animation sequence
     */
    void draw_homing_reticle(LGFX_Sprite *sprite, uint8_t frame);
    
    /**
     * @brief Draws the homing status into the frame
     * 
     * @param sprite - Sprite to draw into
     * @param frame - the frame index of the animation sequence
     */
    void draw_homing_status(LGFX_Sprite *sprite, uint8_t frame);
    #pragma endregion

    #pragma region homing animation methods
    /**
     * @brief updates the feed plot data
     * 
     * @param get_speed - fucntion to call to obtain speed when monitoring.
     */
    void update_manual_feed_data(std::function<float()> get_speed);

    /**
     * @brief Draws the feed plot frame. 
     * 
     * @param sprite - Sprite to draw inot
     */
    void draw_manual_feed_plot(LGFX_Sprite* sprite);

    /**
     * @brief Resets the feed plot data. 
     * 
     */
    void reset_feed_data();    
    #pragma endregion

    #pragma region automated feed methods
    /**
     * @brief Draws the chart area for the automated cutting feed. 
     * 
     * @param sprite - Sprite to draw into.
     */
    void draw_cutting_chart_area(LGFX_Sprite *sprite);

    /**
     * @brief Draws the chart for the automated cutting feed. 
     * 
     * @param sprite - Sprite to draw into.
     */
    void draw_cutting_chart(LGFX_Sprite *sprite);

    /**
     * @brief Draws the charts are for the cutting metrics graph
     * 
     * @param sprite - Sprite to draw into
     * @param offset - Offset of the graph area from the top of the sprite
     */
    void draw_cutting_chart_bg(LGFX_Sprite *sprite, uint8_t offset);

    /**
     * @brief Adds a data point to the dataset, rotates the set by one. Oldest point is discarded
     * 
     * @param data_point the datapoint....
     *          - bit 0...7     : Feed speed in 10 thou IPM
     *          - bit 8...15    : Stallguard value (stallguard theoretically gooes to 1023, but any meaningfull value is going to be below 255)
     *          - bit 16...23   : Stallguard smoothed value
     *          - bit 24...31   : Stallguard derivative
     */
    void add_data_point(uint32_t data_point);
    
    /**
     * @brief Reset the current cutting data
     * 
     */
    void reset_cutting_data();

    /**
     * @brief Calculates the maximum values in the data series for scaling
     */
    void calculate_data_max();
    #pragma endregion

    #pragma region alert methods
    /**
     * @brief Truncates a string with an ellipsis until it fits into a certain space.
     * 
     * @param sprite - The sprite hosting the canvas to draw into
     * @param text - The text to operate on 
     * @param width - The width for the string to fit into
     * @return String - the truncated string
     */
    String truncate_string(lgfx::LGFX_Sprite &sprite, const char* text, int width);

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

    /**
     * @brief Writes the alerts to the display starting from the specified index.
     * 
     * @param start - The starting index of the alerts to display.
     * @param size - The number of alerts to display.
     * @param index - The index of the alert to highlight
     */
    void write_alerts(uint8_t start, uint8_t size, uint8_t index);
    #pragma endregion

    #pragma region status bar method   
    /**
     * @brief Draws the blade running indiator animation into the status bar.
     * 
     * @param sprite Sprite to draw into
     * @param x - the x coordinate
     * @param y - the y coordinate
     * @return uint16_t An unsigned integater denoting the x coordinate after the action.
     */
    uint16_t draw_blade_running_status(LGFX_Sprite &sprite, uint16_t x, uint16_t y);

    /**
     * @brief Draws the lead screw running indiator animation into the status bar.
     * 
     * @param sprite Sprite to draw into
     * @param x - the x coordinate
     * @param y - the y coordinate
     * @return uint16_t An unsigned integater denoting the x coordinate after the action.
     */
    uint16_t draw_screw_running_status(LGFX_Sprite &sprite, uint16_t x, uint16_t y);

    /**
     * @brief Draws the coolant running indiator animation into the status bar.
     * 
     * @param sprite Sprite to draw into
     * @param x - the x coordinate
     * @param y - the y coordinate
     * @return uint16_t An unsigned integater denoting the x coordinate after the action.
     */
    uint16_t draw_coolant_running_status(LGFX_Sprite &sprite, uint16_t x, uint16_t y);

    /**
     * @brief Draws the air blast running indiator animation into the status bar.
     * 
     * @param sprite Sprite to draw into
     * @param x - the x coordinate
     * @param y - the y coordinate
     * @return uint16_t An unsigned integater denoting the x coordinate after the action.
     */
    uint16_t draw_air_running_status(LGFX_Sprite &sprite, uint16_t x, uint16_t y);

    /**
     * @brief Draws the stall guard status into the status bar.
     * 
     * @param sprite Sprite to draw into
     * @param x - the x coordinate
     * @param y - the y coordinate
     * @return uint16_t An unsigned integater denoting the x coordinate after the action.
     */
    uint16_t draw_stallguard_status(LGFX_Sprite &sprite, uint16_t x, uint16_t y);

    /**
     * @brief Status bar task function, respsonsible for drawing the taskbar and keeping it up-to-date
     * 
     * @param args task arguments
     */
    static void status_bar_runner(void *args);


    #pragma endregion

    volatile bool _paused = false;
    volatile bool _homing_animation_break = false;
    volatile bool _feed_animation_break = false;
    volatile bool _fas_break = false;
    volatile bool _use_manual_feed_smoothing = false;
    volatile bool _cutting_chart_break = false;
    volatile bool _toasting_break = false;
    volatile bool _has_alerts = false;
    volatile bool _has_toasts = false;
    volatile bool _suspend_cutting_chart = false;
    volatile uint16_t _status = 0x0000;
    volatile SemaphoreHandle_t _display_mutex;
    TaskHandle_t _touchRunner = NULL;
    TaskHandle_t _homing_animation = NULL;
    TaskHandle_t _fas_runner = NULL;
    TaskHandle_t _feed_animation = NULL;
    TaskHandle_t _cutting_chart = NULL;
    TaskHandle_t _toastRunner = NULL;
    TaskHandle_t _alertBadgeRunner = NULL;
    TaskHandle_t _statusRunner = NULL;
    esp_timer_handle_t toast_timer = NULL;
    screens_t _screen = SCREENS::MAIN;
};

struct Toast_Task_Args
{
    Display* self;
    LGFX_Sprite* toast_sprite;
    uint32_t timeout;
};

struct Cut_TaskArgs
{
    Display* self;
    std::function<uint32_t()> metrics_function;
};

struct FAS_TaskArgs
{
    Display* self;
    std::function<float()> speed_function;
};
using Feed_TaskArgs = FAS_TaskArgs;

#endif //_DISPLAY_H_
