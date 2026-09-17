// Copyright (c) Thor Schueler. All rights reserved.
// SPDX-License-Identifier: MIT

#include "version.h"
#include "display.h"
#include "src/inputs/inputs.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <FunctionalInterrupt.h>


touch_area_t touch_areas[] = {
  { 13, 55, 63, 86, 64, 55, 73, 86, start_icon, EXT_GPIO_START_PIN, true },
  { 13, 89, 63, 120, 64, 89, 73, 120, engage_icon, EXT_GPIO_ENGAGE_PIN, true },
  { 13, 123, 63, 154, 64, 123, 73, 154, home_icon, EXT_GPIO_HOME_PIN, true },
  { 13, 157, 63, 188, 64, 157, 73, 188, lube_icon, EXT_GPIO_LUBE_ON, false},
  { 13, 157, 63, 188, 64, 157, 73, 188, lube_icon, EXT_GPIO_LUBE_AUTO, false},
  { 13, 191, 63, 222, 64, 191, 73, 222, air_icon, EXT_GPIO_AIR_ON, false},
  { 13, 191, 63, 222, 64, 191, 73, 222, air_icon, EXT_GPIO_AIR_AUTO, false},
  { 13, 225, 63, 256, 64, 225, 73, 256, light_icon, EXT_GPIO_LIGHT_COLD, false},      
  { 13, 225, 63, 256, 64, 225, 73, 256, light_icon, EXT_GPIO_LIGHT_WARM, false},
  { 13, 259, 63, 290, 64, 259, 73, 290, settings_icon, 64, true },
  { 359, 143, 413, 193, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, NULL, UINT8_MAX, true },
  { 416, 143, 464, 193, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, NULL, UINT8_MAX, true },
};


/**
 * @brief Construct a new Display object
 * 
 */
Display::Display() {
    ///
    /// SPI BUS (shared for display + touch)
    ///
    {
      auto cfg = _bus.config();
      cfg.spi_host   = SPI2_HOST;
      cfg.spi_mode   = 0;
      cfg.freq_write = SPI_BUS_WRITE_FREQUENCY;
      cfg.freq_read  = SPI_BUS_READ_FREQUENCY;
      cfg.pin_sclk   = SCLK_PIN;
      cfg.pin_mosi   = MOSI_PIN;
      cfg.pin_miso   = MISO_PIN;
      cfg.pin_dc     = DC_PIN;
      _bus.config(cfg);
      _panel.setBus(&_bus);
    }

    ///
    /// DISPLAY PANEL CONFIG
    ///
    {
      auto cfg = _panel.config();
      cfg.pin_cs        = CS_PANEL_PIN;
      cfg.pin_rst       = -1;       // Reset tied is tied to high
      cfg.pin_busy      = -1;       // Not used
      cfg.memory_width  = DISPLAY_WIDTH;
      cfg.memory_height = DISPLAY_HEIGHT;
      cfg.panel_width   = DISPLAY_WIDTH;
      cfg.panel_height  = DISPLAY_HEIGHT;
      cfg.offset_x      = 0;
      cfg.offset_y      = 0;
      cfg.offset_rotation = 0;
      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits  = 1;
      cfg.readable      = true;
      cfg.invert        = false;
      cfg.rgb_order     = false;
      cfg.dlen_16bit    = false;
      cfg.bus_shared    = true;     // IMPORTANT: shared SPI bus
      _panel.config(cfg);
    }

    ///
    /// TOUCH CONFIG (XPT2046)
    ///
    {
      auto cfg = _touch.config();
      cfg.spi_host   = SPI2_HOST;
      cfg.freq       = SPI_BUS_TOUCH_FREQUENCY;
      cfg.pin_sclk   = SCLK_PIN;
      cfg.pin_mosi   = MOSI_PIN;
      cfg.pin_miso   = MISO_PIN;
      cfg.pin_cs     = CS_TOUCH_PIN;
      cfg.pin_int    = TOUCH_IRQ_PIN;
      cfg.x_min      = TOUCH_CALIBRATION_X_MIN; 
      cfg.x_max      = TOUCH_CALIBRATION_X_MAX;
      cfg.y_min      = TOUCH_CALIBRATION_Y_MIN;
      cfg.y_max      = TOUCH_CALIBRATION_Y_MAX;
      cfg.bus_shared = true;
      _touch.config(cfg);
      _panel.setTouch(&_touch);
    }
    setPanel(&_panel);
}

/**
 * @brief Destroy the Display object
 * 
 */
Display::~Display()
{
    if(this->_touchRunner != NULL) { vTaskDelete(this->_touchRunner); this->_touchRunner == NULL; }
}

/**
 * @brief Initialize display and touch panel, set background
 * 
 */
void Display::begin() {
    Logger.Info(F("... Initializing display controller..."));
    Logger.Info(F("....Generating Mutexes"));
    _display_mutex = xSemaphoreCreateBinary();  xSemaphoreGive(_display_mutex);

    Logger.Info(F("...   Set touch IRQ input pin"));
    pinMode(TOUCH_IRQ_PIN, INPUT_PULLUP); 
   
    Logger.Info(F("...   Initialize display"));
    if (xSemaphoreTake(this->_display_mutex, portMAX_DELAY) == pdTRUE)
    { 
        this->init();
        this->setRotation(1);           // Landscape
        this->fillScreen(TFT_BLACK);
        xSemaphoreGive(this->_display_mutex);
    }

    Logger.Info(F("...   Setup various tasks"));
    xTaskCreatePinnedToCore(touch_runner, "touchRunner", 2048, this, 1, &_touchRunner, 0);

    Logger.Info(F("...   Regsiter Touch interrupts"));
    uint8_t ctrl = 0b10010000;
    attachInterrupt(digitalPinToInterrupt(TOUCH_IRQ_PIN), std::bind(&Display::processTouchInterrupt, this), FALLING);
    if (xSemaphoreTake(this->_display_mutex, portMAX_DELAY) == pdTRUE)
    {
        lgfx::spi::beginTransaction(SPI2_HOST, SPI_BUS_TOUCH_FREQUENCY, 0);
        lgfx::gpio_lo(CS_TOUCH_PIN);
        lgfx::spi::writeBytes(SPI2_HOST, &ctrl, 1);   // PD1=0, PD0=0 → IRQ enabled
        lgfx::gpio_hi(CS_TOUCH_PIN);
        lgfx::spi::endTransaction(SPI2_HOST);
        xSemaphoreGive(this->_display_mutex);
    }
    Logger.Info(F("...   Touch Interrupt on XPT2046 activated"));
    Logger.Info(F("...   ST7796S + XPT2046 Initialization complete."));
    Logger.Info(F("...   Done."));
}

/**
 * @brief Draws the image background and static overlays.
 * 
 */
void Display::draw_canvas()
{
    if (xSemaphoreTake(this->_display_mutex, portMAX_DELAY) == pdTRUE)
    {
        this->setTextColor(TFT_WHITE);
        this->setTextSize(1);

        this->pushImage(0, 0, 480, 320, (lgfx::rgb565_t*)background);
        this->setCursor(370, 5);
        this->printf("%d.%d.%d", FW_VERSION_MAJOR, FW_VERSION_MINOR, FW_BUILD_NUMBER);
        xSemaphoreGive(this->_display_mutex);
    }    
}

/**
 * @brief Set the button tab on a particular button
 * 
 * @param gpio - the gpio number associated with the button
 * @param on - the desired state of the tab (on, off or pending)
 * 
 * @remarks - the value of gpio reflects an actual GPIO if less than 64. Above 64 the value is asusmed to be logically 
 * only and not associated with a physical pin.
 */
void Display::set_button_tab(uint8_t gpio, touch_tab_state_t state)
{
    LGFX_Sprite tab(this);
    tab.setColorDepth(16);                          // setup for RGB565
    tab.createSprite(TAB_WIDTH, TAB_HEIGHT);        // create sprite
    tab.fillSprite(0x0000);

    if(state == TOUCH_TAB_STATE::ON) tab.pushImage(0, 0, TAB_WIDTH, TAB_HEIGHT, (lgfx::rgb565_t*)active_tab);           // push active background
    else if(state == TOUCH_TAB_STATE::OFF) tab.pushImage(0, 0, TAB_WIDTH, TAB_HEIGHT, (lgfx::rgb565_t*)inactive_tab);   // push active background
    else if(state == TOUCH_TAB_STATE::WAITING) tab.pushImage(0, 0, TAB_WIDTH, TAB_HEIGHT, (lgfx::rgb565_t*)pending_tab);// push pending background
    else 
    {
        Logger.Error_f(F("Unsopported state %d. Only 0(ON), 1(OFF), and 3(PENDING) are supported. Ignoring"), state);
        return;
    }

    for(int i=0; i<sizeof(touch_areas)/sizeof(touch_areas[0]); i++) 
    {
        if(touch_areas[i].gpio == gpio)
        {
            if (xSemaphoreTake(this->_display_mutex, portMAX_DELAY) == pdTRUE)
            {            
                tab.pushSprite(touch_areas[i].tab_x1, touch_areas[i].tab_y1, 0x0000);                                       // push sprite 
                xSemaphoreGive(this->_display_mutex);
            }
        }
    }                           
}

void Display::set_button(uint8_t gpio, touch_tab_state_t state)
{
    LGFX_Sprite button(this);
    button.setColorDepth(16);                                              // setup for RGB565
    button.createSprite(ACTIVE_BUTTON_WIDTH, ACTIVE_BUTTON_HEIGHT);        // create sprite
    button.fillSprite(0x0000);

    if(state == TOUCH_TAB_STATE::ON) button.pushImage(0, 0, ACTIVE_BUTTON_WIDTH, ACTIVE_BUTTON_HEIGHT, (lgfx::rgb565_t*)active_button);           // push active background
    else if(state == TOUCH_TAB_STATE::OFF) button.pushImage(0, 0, ACTIVE_BUTTON_WIDTH, ACTIVE_BUTTON_HEIGHT, (lgfx::rgb565_t*)inactive_button);   // push active background
    else 
    {
        Logger.Error_f(F("Unsopported state %d. Only 0(ON), 1(OFF), and 3(PENDING) are supported. Ignoring"), state);
        return;
    }

    for(int i=0; i<sizeof(touch_areas)/sizeof(touch_areas[0]); i++) 
    {
        if(touch_areas[i].gpio == gpio)
        {
            if (xSemaphoreTake(this->_display_mutex, portMAX_DELAY) == pdTRUE)
            {  
                if(touch_areas[i].icon != NULL)
                {  
                    button.pushImage(0, 0, ACTIVE_BUTTON_WIDTH, ACTIVE_BUTTON_HEIGHT, (lgfx::rgb565_t*)touch_areas[i].icon, 0x0000);// push icon overlay   
                }
                button.pushSprite(touch_areas[i].icon_x1, touch_areas[i].icon_y1, 0x0000);                                          // push sprite
                xSemaphoreGive(this->_display_mutex);
            } 
        }
    }                           
}

/**
 * @brief Manages the EMS overlay.
 * 
 * @param active - true to activate the EMS overlay, false to deactivate it. 
 */
void Display::ems_overlay(bool active)
{
    if(active)
    {
        if (xSemaphoreTake(this->_display_mutex, portMAX_DELAY) == pdTRUE)
        {         
            this->pushImage(0, 0, 480, 320, (lgfx::rgb565_t*)ems);
            xSemaphoreGive(this->_display_mutex);
        } 
    }
    else
    {
        this->draw_canvas();
    }
}

/**
 * @brief Process the touch interrupt and call the callback if set
 * 
 * @remark This method is really to determine whether the touch is relevant and 
 * what downstream action needs to be invoked. For this to work, each screen must
 * register certain areas on the screen and the associated command that should be invoked. 
 * The _callback function will be called with the relevant command to invoke if it is set. 
 * 
 */
void IRAM_ATTR Display::processTouchInterrupt()
{ 
    BaseType_t xHigherPriorityTaskToken = pdFALSE;
    if(this->_touchRunner != NULL)
    {
        gpio_intr_disable((gpio_num_t)TOUCH_IRQ_PIN);
        vTaskNotifyGiveFromISR(this->_touchRunner, &xHigherPriorityTaskToken); 
        portYIELD_FROM_ISR(xHigherPriorityTaskToken);
    }
}

/**
 * @brief Task function managing the display
 * @param args - pointer to task arguments
 */
void Display::touch_runner(void* args)
{
    uint16_t x = UINT16_MAX;
    uint16_t y = UINT16_MAX;
    uint8_t sprite_index = UINT8_MAX;
    bool shouldProcess = true;
    auto& inputs = Inputs::get_inputs();
    Display *_this = reinterpret_cast<Display *>(args);
    Logger.Info(F("...   Touch monitoring task has started."));
    for(;;)
    {
        if(shouldProcess)
        {
            // Wait for the notification to come from the event handler
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            if (_this->_paused) continue;
            if (_this->getTouch(&x, &y)) 
            {
                for(int i=0; i<sizeof(touch_areas)/sizeof(touch_areas[0]); i++) 
                {
                    if(x >= touch_areas[i].icon_x1 && x <= touch_areas[i].icon_x2 && y >= touch_areas[i].icon_y1 && y <= touch_areas[i].icon_y2) 
                    {
                        if(touch_areas[i].enable == false) break;
                        uint8_t gpio = touch_areas[i].gpio;
                        if(gpio != UINT8_MAX) _this->set_button(gpio, TOUCH_TAB_STATE::ON);
                        if(gpio != UINT8_MAX && inputs[gpio].entry != nullptr) inputs[gpio].entry(gpio, inputs[gpio].command.c_str());
                        sprite_index = i;
                        shouldProcess = false;
                        break;
                    }
                }
            }
            if(shouldProcess)
            {   
                gpio_intr_enable((gpio_num_t)TOUCH_IRQ_PIN);
                sprite_index = UINT8_MAX;
            }
        }
        else
        {
            if(digitalRead(TOUCH_IRQ_PIN) == HIGH)
            {
                shouldProcess = true;
                if(sprite_index != UINT8_MAX)
                {
                    uint8_t gpio = touch_areas[sprite_index].gpio;
                    if(gpio != UINT8_MAX && inputs[gpio].exit != nullptr) inputs[gpio].exit(gpio, inputs[gpio].command.c_str());
                    if(gpio != UINT8_MAX) _this->set_button(gpio, TOUCH_TAB_STATE::OFF);
                    sprite_index = UINT8_MAX;
                }
                gpio_intr_enable((gpio_num_t)TOUCH_IRQ_PIN);
            }
        }
        vTaskDelay(10);
    }
    if(_this->_touchRunner != NULL) { vTaskDelete(_this->_touchRunner); _this->_touchRunner = NULL; }
}

/**
 * @brief Task function runnign the homing animation
 * @param args - pointer to task arguments
 */
void Display::homeing_animation_runner(void* args)
{
    Display *_this = reinterpret_cast<Display *>(args);
    int i=0;
    Logger.Info(F("... Homing animation started."));
    for(;;)
    {
        for(uint8_t frame = 0; frame < 60; frame++)
        {
            if (xSemaphoreTake(_this->_display_mutex, portMAX_DELAY) == pdTRUE)
            { 
                _this->draw_homing_frame(frame);
                xSemaphoreGive(_this->_display_mutex);
            }
            vTaskDelay(pdMS_TO_TICKS(50));
            if(_this->_paused || *(_this->_homing_animation_break)) break;
        }
        if(_this->_paused || *(_this->_homing_animation_break)) break;
        i++;
        if(i>5)break;
    }

    Logger.Info(F("... Homing animation complete."));
    _this->_homing_animation = NULL;
    vTaskDelete(NULL);
}


/**
 * @brief Starts the homing animation. Once started, the animation will run until terminated when the value 
 * of the terminate reference goes true
 * 
 * @param terminate - reference to a variable for flow control. Going true will terminate the animation. 
 */
void Display::start_homing_animation(bool& terminate)
{
    if(this->_homing_animation != NULL) return;           // animation taskis already running.
    this->_homing_animation_break = &terminate;
    xTaskCreatePinnedToCore(homeing_animation_runner, "homingAnimationRunner", 2048, this, 1, &_homing_animation, 0);
}


#pragma region Homing animation methods
/**
 * @brief Draws a frame for the homing animation displayed during the homing cycle
 * 
 * @param frame - the frame index to draw.
 */
void Display::draw_homing_frame(uint8_t frame)
{
    frame %= 60;

    if(this->_homingSprite == nullptr)
    {
        this->_homingSprite = new LGFX_Sprite(this);
        this->_homingSprite->createSprite(HOMING_W, HOMING_H);
        this->_homingSprite->setColorDepth(16);
    }
    this->_homingSprite->fillSprite(LCARS_GRAY);

    draw_homing_background();
    draw_homing_scanner(frame);
    draw_homing_carriage(frame);
    draw_homing_reticle(frame);
    draw_homing_status(frame);

    this->_homingSprite->pushSprite(88, 95);
}

/**
 * @brief Draws the background of a homing frame
 * 
 */
void Display::draw_homing_background()
{
    //
    // Grid
    //
    for(int x = 0; x < HOMING_W; x += 17) _homingSprite->drawFastVLine(x, 0, HOMING_H, LCARS_GRID);
    for(int y = 0; y < HOMING_H; y += 17) _homingSprite->drawFastHLine(0, y, HOMING_W, LCARS_GRID);

    //
    // Feed axis rail
    //
    _homingSprite->drawFastHLine(20, HOMING_H/2, HOMING_W - 40, LCARS_BLUE);

    //
    // Home indicator
    //
    _homingSprite->fillTriangle(5, HOMING_H/2, 15, HOMING_H/2 - 10, 15, HOMING_H/2 + 10, LCARS_ORANGE);

    //
    // Pulsing beacon ring
    //
    _homingSprite->drawCircle(18, HOMING_H/2, 6, LCARS_ORANGE);
    _homingSprite->setTextColor(LCARS_ORANGE, LCARS_GRAY);
    _homingSprite->drawString("REFERENCE ACQUISITION", 10, 5);
}

/**
 * @brief Draws the scanning line of the homing animation
 * 
 * @param frame - the frame index of the animation sequence
 */
void Display::draw_homing_scanner(uint8_t frame)
{
    uint8_t sweepFrame = frame % 20;
    int x = (sweepFrame * (HOMING_W - 4)) / 19;

    _homingSprite->drawFastVLine(x - 2, 0, HOMING_H, 0x2C7F);
    _homingSprite->drawFastVLine(x - 1, 0, HOMING_H, 0x43FF);
    _homingSprite->drawFastVLine(x, 0, HOMING_H, LCARS_CYAN);
}

/*
void Display::draw_homing_scanner(uint8_t frame)
{
    constexpr uint16_t beam[] =
    {
        0x0841, 0x1062, 0x1082, 0x18A3, 0x18C3, 0x2104, 0x2945, 0x3186, 0x39C7, 0x4208, 0x52AA, 0x63AE, 0x7513, 0x86B9, 0xA7DF,
        0xFFFF, 0xA7DF, 0x86B9, 0x7513, 0x63AE, 0x52AA, 0x4208, 0x39C7, 0x3186, 0x2945, 0x2104, 0x18C3, 0x18A3, 0x1082, 0x1062, 0x0841
    };

    uint8_t sweepFrame = frame % 20;
    int x = (sweepFrame * (HOMING_W - 4))  / 19;

    constexpr int halfWidth = 15;
    for(int i = -halfWidth; i <= halfWidth; i++)
    {
        int drawX = x + i;
        if(drawX < 0 || drawX >= HOMING_W) continue;
        _homingSprite->drawFastVLine(drawX, 0, HOMING_H, beam[i + halfWidth]);
    }

    //
    // Hot center line
    //
    _homingSprite->drawFastVLine(x, 0, HOMING_H, TFT_WHITE);
}
*/

/*
void Display::draw_homing_scanner(uint8_t frame)
{
    uint8_t sweepFrame = frame % 20;
    int centerX = (sweepFrame * (HOMING_W - 4)) / 19;

    // Symmetric horizontal intensity profile
    constexpr uint16_t beam[] =
    {
        0x1082, 0x18C3, 0x2104, 0x2945, 0x3186, 0x39C7, 0x4208, 0x52AA, 0x63AE, 0x7513, 0x86B9, 0xA7DF, 0xD7FF, 0xEFFF, 0xFFFF, 0xEFFF, 0xD7FF,
        0xA7DF, 0x86B9, 0x7513, 0x63AE, 0x52AA, 0x4208, 0x39C7, 0x3186, 0x2945, 0x2104, 0x18C3, 0x1082
    };

    constexpr int halfWidth = sizeof(beam) / sizeof(beam[0]) / 2;
    for(int i=-halfWidth; i<=halfWidth; i++)
    {
        int x = centerX + i;
        if(x < 0 || x >= HOMING_W) continue;
        _homingSprite->drawFastVLine(x, 0, HOMING_H, beam[i + halfWidth]);
    }
}
*/

/**
 * @brief Draws the moving carriage that is being homed.
 * 
 * @param frame - the frame index of the animation sequence
 */
void Display::draw_homing_carriage(uint8_t frame)
{
    int x = 225 - ((220 * frame) / 59);
    _homingSprite->fillRoundRect(x, HOMING_H/2 - 14, 24, 28, 3, LCARS_CYAN);
    _homingSprite->fillCircle(x + 26, HOMING_H/2 - 5, 2, LCARS_ORANGE);
    _homingSprite->fillCircle(x + 26, HOMING_H/2 + 5, 2, LCARS_ORANGE);
}

/**
 * @brief Draws the homing reticle
 * 
 * @param frame - the frame index of the animation sequence
 */
void Display::draw_homing_reticle(uint8_t frame)
{
    if(frame < 20) return;

    int radius = 8 + ((frame - 20) / 2);
    if(radius > 24) radius = 24;

    constexpr int cx = 18; //152;
    constexpr int cy = HOMING_H / 2; //86;

    _homingSprite->drawCircle(cx, cy, radius, LCARS_ORANGE);
    _homingSprite->drawFastHLine(cx - radius - 6, cy, 5, LCARS_ORANGE);
    _homingSprite->drawFastHLine(cx + radius + 1, cy, 5, LCARS_ORANGE);
    _homingSprite->drawFastVLine(cx, cy - radius - 6, 5, LCARS_ORANGE);
    _homingSprite->drawFastVLine(cx, cy + radius + 1, 5, LCARS_ORANGE);
}

/**
 * @brief Draws the homing status message into the frame
 * 
 * @param frame - the frame index of the animation sequence
 */
void Display::draw_homing_status(uint8_t frame)
{
    if(frame < 20 || (frame > 30 && frame < 40) || (frame > 50 && frame < 60))
    {
        _homingSprite->setTextColor(LCARS_CYAN, LCARS_GRAY);
        _homingSprite->drawString("SEEKING HOME", 5, 135);
    }
}
#pragma endregion

