// Copyright (c) Thor Schueler. All rights reserved.
// SPDX-License-Identifier: MIT
// IMPORTANT: LIBRARY MUST BE SPECIFICALLY CONFIGURED FOR EITHER TFT SHIELD
// OR BREAKOUT BOARD USAGE.

#include "display.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <FunctionalInterrupt.h>


touch_area_t touch_areas[] = {
  { 13, 55, 63, 86, "start" },
  { 13, 88, 63, 119, "engage" },
  { 13, 121, 63, 153, "home" },
  { 13, 255, 63, 286, "settings" },
  { 359, 143, 413, 193, "stock" },
  { 416, 143, 464, 193, "alerts" },
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
    this->print("0.00.00");

    Logger.Info(F("...   Setup various tasks"));
    xTaskCreatePinnedToCore(touch_runner, "touchRunner", 1560, this, 1, &_touchRunner, 0);

    Logger.Info(F("...   ST7796S + XPT2046 Initialization complete."));
    Logger.Info(F("...   Done."));
}

/**
 * @brief Set the Touch Callback method to call when a relevant touch event occurs
 * 
 * @param callback - The callback function to call when a touch event occurs
 */  
void Display::setTouchCallback(std::function<void(const char*)> callback) {
    this->_callback = callback;

    /// only register an interrupt if we actually have a callback funtion to 
    /// minimize unncessary logic execution
    if(this->_callback != NULL) 
    {
      uint8_t ctrl = 0b10010000;
      attachInterrupt(digitalPinToInterrupt(TOUCH_IRQ_PIN), std::bind(&Display::processTouchInterrupt, this), FALLING);
      Logger.Info(F("... Touch callback registered."));

      lgfx::spi::beginTransaction(SPI2_HOST, SPI_BUS_TOUCH_FREQUENCY, 0);
      lgfx::gpio_lo(CS_TOUCH_PIN);
      lgfx::spi::writeBytes(SPI2_HOST, &ctrl, 1);   // PD1=0, PD0=0 → IRQ enabled
      lgfx::gpio_hi(CS_TOUCH_PIN);
      lgfx::spi::endTransaction(SPI2_HOST);
      Logger.Info(F("... Touch Interrupt on XPT2046 activated"));
    } 
    else 
    {
      detachInterrupt(digitalPinToInterrupt(TOUCH_IRQ_PIN));
      Logger.Info(F("... Touch callback unregistered."));
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
    if(this->_callback != NULL && this->_touchRunner != NULL)
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
    bool shouldProcess = true;
    Display *_this = reinterpret_cast<Display *>(args);
    for(;;)
    {
        uint16_t x, y;

        if(shouldProcess)
        {
            // Wait for the notification to come from the event handler
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            if (_this->getTouch(&x, &y) && _this->_callback != NULL) 
            {
                for(int i=0; i<sizeof(touch_areas)/sizeof(touch_areas[0]); i++) 
                {
                    if(x >= touch_areas[i].x1 && x <= touch_areas[i].x2 && y >= touch_areas[i].y1 && y <= touch_areas[i].y2) 
                    {
                        _this->_callback(touch_areas[i].command.c_str());
                        shouldProcess = false;
                        break;
                    }
                }
            }
            if(shouldProcess) gpio_intr_enable((gpio_num_t)TOUCH_IRQ_PIN);
        }
        else
        {
            if(digitalRead(TOUCH_IRQ_PIN) == HIGH)
            {
                shouldProcess = true;
                gpio_intr_enable((gpio_num_t)TOUCH_IRQ_PIN);
            }
        }
        vTaskDelay(10);
    }
    if(_this->_touchRunner != NULL) { vTaskDelete(_this->_touchRunner); _this->_touchRunner = NULL; }
}