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
  { 13, 55, 63, 86, start_icon, EXT_GPIO_START_PIN },
  { 13, 89, 63, 120, engage_icon, EXT_GPIO_ENGAGE_PIN },
  { 13, 123, 63, 154, home_icon, EXT_GPIO_HOME_PIN},
  { 13, 259, 63, 290, settings_icon, UINT8_MAX },
  { 359, 143, 413, 193, NULL, UINT8_MAX },
  { 416, 143, 464, 193, NULL, UINT8_MAX },
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
      cfg.dummy_read_pixel = 0;
      cfg.dummy_read_bits  = 0;
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
    Logger.Info(F("... Initializing display..."));
    Logger.Info(F("...   Set touch IRQ input pin"));
    pinMode(TOUCH_IRQ_PIN, INPUT_PULLUP); 
   
    Logger.Info(F("...   Initialize display"));
    this->init();
    this->setRotation(1);           // Landscape
    this->fillScreen(TFT_BLACK);
    this->setTextColor(TFT_WHITE);
    this->setTextSize(1);

    this->pushImage(0, 0, 480, 320, (lgfx::rgb565_t*)background);
    this->setCursor(370, 5);
    this->printf("%d.%d.%d", FW_VERSION_MAJOR, FW_VERSION_MINOR, FW_BUILD_NUMBER);

    Logger.Info(F("...   Setup various tasks"));
    xTaskCreatePinnedToCore(touch_runner, "touchRunner", 2048, this, 1, &_touchRunner, 0);

    Logger.Info(F("...   Regsiter Touch interrupts"));
    uint8_t ctrl = 0b10010000;
    attachInterrupt(digitalPinToInterrupt(TOUCH_IRQ_PIN), std::bind(&Display::processTouchInterrupt, this), FALLING);
    lgfx::spi::beginTransaction(SPI2_HOST, SPI_BUS_TOUCH_FREQUENCY, 0);
    lgfx::gpio_lo(CS_TOUCH_PIN);
    lgfx::spi::writeBytes(SPI2_HOST, &ctrl, 1);   // PD1=0, PD0=0 → IRQ enabled
    lgfx::gpio_hi(CS_TOUCH_PIN);
    lgfx::spi::endTransaction(SPI2_HOST);
    Logger.Info(F("...   Touch Interrupt on XPT2046 activated"));
    Logger.Info(F("...   ST7796S + XPT2046 Initialization complete."));
    Logger.Info(F("...   Done."));
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
    LGFX_Sprite active(_this);
    active.createSprite(ACTIVE_BUTTON_WIDTH, ACTIVE_BUTTON_HEIGHT);         // create sprite
    active.setColorDepth(16);                                               // setup for RGB565
    Logger.Info(F("...   Touch monitoring task has started."));
    for(;;)
    {
        if(shouldProcess)
        {
            // Wait for the notification to come from the event handler
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            if (_this->getTouch(&x, &y)) 
            {
                for(int i=0; i<sizeof(touch_areas)/sizeof(touch_areas[0]); i++) 
                {
                    if(x >= touch_areas[i].x1 && x <= touch_areas[i].x2 && y >= touch_areas[i].y1 && y <= touch_areas[i].y2) 
                    {
                        if(touch_areas[i].icon != NULL)
                        {
                            active.fillSprite(0x0000);
                            active.pushImage(0, 0, ACTIVE_BUTTON_WIDTH, ACTIVE_BUTTON_HEIGHT, (lgfx::rgb565_t*)active_button);                  // push active background
                            active.pushImage(0, 0, ACTIVE_BUTTON_WIDTH, ACTIVE_BUTTON_HEIGHT, (lgfx::rgb565_t*)touch_areas[i].icon, 0x0000);    // push icon overlay
                            active.pushSprite(touch_areas[i].x1, touch_areas[i].y1, 0x0000);                                                    // push sprite
                            sprite_index = i;
                        }
                        uint8_t gpio = touch_areas[i].gpio;
                        if(gpio != UINT8_MAX && inputs[gpio].entry != nullptr) inputs[gpio].entry(gpio, inputs[gpio].command.c_str());
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
                    if(inputs[gpio].exit != nullptr) inputs[gpio].exit(gpio, inputs[gpio].command.c_str());
                    if(touch_areas[sprite_index].icon != NULL)
                    {
                        active.pushImage(0, 0, ACTIVE_BUTTON_WIDTH, ACTIVE_BUTTON_HEIGHT, (lgfx::rgb565_t*)inactive_button);                            // push active background
                        active.pushImage(0, 0, ACTIVE_BUTTON_WIDTH, ACTIVE_BUTTON_HEIGHT, (lgfx::rgb565_t*)touch_areas[sprite_index].icon, 0x0000);     // push icon overlay
                        active.pushSprite(touch_areas[sprite_index].x1, touch_areas[sprite_index].y1, 0x0000);        
                    }
                    sprite_index = UINT8_MAX;
                }
                gpio_intr_enable((gpio_num_t)TOUCH_IRQ_PIN);
            }
        }
        vTaskDelay(10);
    }
    if(_this->_touchRunner != NULL) { vTaskDelete(_this->_touchRunner); _this->_touchRunner = NULL; }
}