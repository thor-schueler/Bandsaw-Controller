// Copyright (c) Thor Schueler. All rights reserved.
// SPDX-License-Identifier: MIT

#include "version.h"
#include "display.h"
#include "src/inputs/inputs.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <FunctionalInterrupt.h>




touch_area_t main_touch_areas[] = {
  { 0, 55, 63, 86, 64, 55, 73, 86, start_icon, EXT_GPIO_START_PIN, true },
  { 0, 89, 63, 120, 64, 89, 73, 120, engage_icon, EXT_GPIO_ENGAGE_PIN, true },
  { 0, 123, 63, 154, 64, 123, 73, 154, home_icon, EXT_GPIO_HOME_PIN, true },
  { 0, 157, 63, 188, 64, 157, 73, 188, lube_icon, EXT_GPIO_LUBE_ON, false},
  { 0, 157, 63, 188, 64, 157, 73, 188, lube_icon, EXT_GPIO_LUBE_AUTO, false},
  { 0, 191, 63, 222, 64, 191, 73, 222, air_icon, EXT_GPIO_AIR_ON, false},
  { 0, 191, 63, 222, 64, 191, 73, 222, air_icon, EXT_GPIO_AIR_AUTO, false},
  { 0, 225, 63, 256, 64, 225, 73, 256, light_icon, EXT_GPIO_LIGHT_COLD, false},      
  { 0, 225, 63, 256, 64, 225, 73, 256, light_icon, EXT_GPIO_LIGHT_WARM, false},
  { 0, 259, 63, 290, 64, 259, 73, 290, settings_icon, 16, true },
  { 359, 143, 413, 193, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, NULL, UINT8_MAX, true },
  { 416, 143, 464, 193, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, NULL, 17, true },
};

touch_area_t alerts_touch_areas[] = {
  { 359, 91, 464, 125, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, NULL, 0, true },
  { 359, 127, 410, 160, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, NULL, 1, true },
  { 412, 127, 464, 160, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, NULL, 2, true },
  { 359, 161, 464, 193, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, NULL, 3, true }  
};

touch_area_t settings_touch_areas[] = {
  { UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, NULL, UINT8_MAX, false }
};

touch_area_t ems_touch_areas[] = {
  { UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, NULL, UINT8_MAX, false }
};

touch_screen_t touch_areas[] = {
    [SCREENS::MAIN] = {
        main_touch_areas,
        sizeof(main_touch_areas) / sizeof(main_touch_areas[0])
    },
    [SCREENS::ALERTS] = {
        alerts_touch_areas,
        sizeof(alerts_touch_areas) / sizeof(alerts_touch_areas[0])
    },
    [SCREENS::SETTINGS] = {
        settings_touch_areas,
        sizeof(settings_touch_areas) / sizeof(settings_touch_areas[0])
    },
    [SCREENS::EMS] = {
        ems_touch_areas,
        sizeof(ems_touch_areas) / sizeof(ems_touch_areas[0])
    }
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
    if(this->_homing_animation != NULL) { vTaskDelete(this->_homing_animation); this->_homing_animation == NULL; }
    if(this->_fas_runner != NULL) { vTaskDelete(this->_fas_runner); this->_fas_runner == NULL; }
    if(this->_feed_animation != NULL) { vTaskDelete(this->_feed_animation); this->_feed_animation == NULL; }
    if(this->_cutting_chart != NULL) { vTaskDelete(this->_cutting_chart); this->_cutting_chart == NULL; }
    if(this->_toastRunner != NULL) { vTaskDelete(this->_toastRunner); this->_toastRunner == NULL; }
    if(this->_alertBadgeRunner != NULL) { vTaskDelete(this->_alertBadgeRunner); this->_alertBadgeRunner == NULL; }
}

/**
 * @brief Initialize display and touch panel, set background
 * 
 */
void Display::begin() {
    Logger.Info(F("... Initializing display controller..."));
    Logger.Info(F("....Generating Mutexes"));
    _display_mutex = xSemaphoreCreateMutex(); 

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
    xTaskCreatePinnedToCore(touch_runner, "touchRunner", 3072, this, 1, &_touchRunner, 0);
    xTaskCreatePinnedToCore(alerts_badge_runner, "alertBadgeRunner", 2048, this, 1, &_alertBadgeRunner, 1);
    esp_timer_create_args_t args = {
            .callback = [](void* arg)
                {
                    Display* _this = static_cast<Display*>(arg);
                    _this->hide_toast();
                },
                .arg = this,
                .dispatch_method = ESP_TIMER_TASK,
                .name = "toast"
    };
    esp_timer_create(&args, &toast_timer);


    Logger.Info(F("...   Regsiter Touch interrupts"));
    uint8_t ctrl = 0b10010000;
    attachInterrupt(digitalPinToInterrupt(TOUCH_IRQ_PIN), std::bind(&Display::processTouchInterrupt, this), FALLING);
    if (xSemaphoreTake(this->_display_mutex, portMAX_DELAY) == pdTRUE)
    {
        // this is enabling touch interrupts on the XPT2046 chip.
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
        this->pushImage(STATUS_X, STATUS_Y, status_width, status_height, (lgfx::rgb565_t*)status);
        this->setCursor(370, 5);
        this->printf("%d.%d.%d", FW_VERSION_MAJOR, FW_VERSION_MINOR, FW_BUILD_NUMBER);
        xSemaphoreGive(this->_display_mutex);
    }    
}


/**
 * @brief Draws the taskbar.
 * 
 */
void Display::draw_status_bar()
{
    LGFX_Sprite taskbar(this);
    taskbar.setColorDepth(16);
    taskbar.createSprite(status_width, status_height);
    taskbar.pushImage(0, 0, status_width, status_height, (lgfx::rgb565_t*)status);
    if (xSemaphoreTake(this->_display_mutex, portMAX_DELAY) == pdTRUE)
    {  
        taskbar.pushSprite(STATUS_X, STATUS_Y);
        xSemaphoreGive(this->_display_mutex);
    }
    taskbar.deleteSprite();
}

/**
 * @brief Set the button tab on a particular button
 * 
 * @param gpio - the gpio number associated with the button, there is no action for GPIOs > 64
 * @param on - the desired state of the tab (on, off or pending)
 * 
 * @remarks - the value of gpio reflects an actual GPIO if less than 64. Above 64 the value is asusmed to be logically 
 * only and not associated with a physical pin.
 */
void Display::set_button_tab(uint8_t gpio, touch_tab_state_t state)
{
    if(this->_screen == SCREENS::ALERTS || this->_screen == SCREENS::EMS) return; 
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

    for(int i=0; i< touch_areas[_screen].count; i++) 
    {
        if(touch_areas[_screen].areas[i].gpio == gpio && touch_areas[_screen].areas[i].tab_x1 != UINT16_MAX && touch_areas[_screen].areas[i].tab_y1 != UINT16_MAX && touch_areas[_screen].areas[i].tab_x2 != UINT16_MAX && touch_areas[_screen].areas[i].tab_y2 != UINT16_MAX)
        {
            if (xSemaphoreTake(this->_display_mutex, portMAX_DELAY) == pdTRUE)
            {            
                tab.pushSprite(touch_areas[_screen].areas[i].tab_x1, touch_areas[_screen].areas[i].tab_y1, 0x0000);                                       // push sprite 
                xSemaphoreGive(this->_display_mutex);
            }
        }
    }  
    tab.deleteSprite();                         
}

/**
 * @brief Set the icon and background for a button
 * 
 * @param gpio - the gpio number associated with the button, there is no action for GPIOs > 64
 * @param on - the desired state of the tab (on, off or pending)
 * 
 * @remarks - the value of gpio reflects an actual GPIO if less than 64. Above 64 the value is asusmed to be logically 
 * only and not associated with a physical pin.
 */
void Display::set_button(uint8_t gpio, touch_tab_state_t state)
{
    if(this->_screen == SCREENS::ALERTS || this->_screen == SCREENS::EMS) return; 
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

    for(int i=0; i<touch_areas[_screen].count; i++) 
    {
        if(touch_areas[_screen].areas[i].gpio == gpio)
        {
            if (xSemaphoreTake(this->_display_mutex, portMAX_DELAY) == pdTRUE)
            {  
                if(touch_areas[_screen].areas[i].icon != NULL)
                {  
                    button.pushImage(13, 0, ACTIVE_BUTTON_WIDTH-13, ACTIVE_BUTTON_HEIGHT, (lgfx::rgb565_t*)touch_areas[_screen].areas[i].icon, 0x0000); // push icon overlay   
                }
                button.pushSprite(touch_areas[_screen].areas[i].icon_x1, touch_areas[_screen].areas[i].icon_y1, 0x0000);                                // push sprite
                xSemaphoreGive(this->_display_mutex);
            } 
        }
    }
    button.deleteSprite();                             
}

/**
 * @brief Set the workarea title 
 * 
 * @param title_image - A pointer to an image for the title. Could be an icon or a font bitmap
 * @param title_image_size - The number of elements in the image.
 * @param title - Title string to use
 * @remark The title (if present) is written after the image (if present)
 */
void Display::set_workarea_title(const uint16_t* title_image, size_t title_image_size, String title)
{
    LGFX_Sprite title_sprite(this);
    title_sprite.setColorDepth(16);
    title_sprite.createSprite(260, 20);
    title_sprite.fillSprite(TFT_BLACK);

    if(title_image != nullptr) title_sprite.pushImage(0, 0, title_image_size/20, 20, (lgfx::rgb565_t*)title_image);
    if(!title.isEmpty())
    {
        title_sprite.setTextColor(LCARS_CYAN, TFT_BLACK);
        title_sprite.setFont(&fonts::Font2);
        title_sprite.drawString(title.c_str(), 0, 0);
    }
    if (xSemaphoreTake(this->_display_mutex, portMAX_DELAY) == pdTRUE)
    {     
        title_sprite.pushSprite(87, 59);
        xSemaphoreGive(this->_display_mutex);
    }
    title_sprite.deleteSprite();  
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
 * @brief Task function monitoring the speed and updating the feeds and speeds panel
 * @param args - pointer to task arguments
 */
void Display::fas_runner(void* args) 
{
    FAS_TaskArgs* _args = static_cast<FAS_TaskArgs*>(args);
    Display* _this = _args->self;
    float speed = -1;
    Logger.Info(F("... Feeds and Speeds Monitoring started."));
    for(;;)
    {
        vTaskDelay(pdMS_TO_TICKS(50));
        if(_this->_paused) { speed = -1; continue; }
        if(_this->_fas_break)
        {
            Logger.Info(F("... Feeds and Speeds Monitoring task received termination request"));
            break;
        }
        float s = _args->speed_function();
        if(s!=speed)
        {
            speed = s;
            _this->fas_overlay(true, speed);
        }

    }
    _this->fas_overlay(false, 0);
    
    Logger.Info(F("... Feeds and Speeds Monitoring complete."));
    _this->_fas_runner = NULL;
    _this->_fas_break = false;
    delete _args;
    vTaskDelete(NULL);
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
    Display *_this = reinterpret_cast<Display *>(args);
    screens_t old_screen = SCREENS::MAIN;
    bool shouldProcess = true;
    
    Logger.Info(F("...   Touch monitoring task has started."));
    for(;;)
    {
        vTaskDelay(10);
        if(shouldProcess)
        {
            // Wait for the notification to come from the event handler
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            auto& in = Inputs::get_inputs(_this->_screen);         
            if (_this->getTouch(&x, &y)) 
            {
                for(int i=0; i<touch_areas[_this->_screen].count; i++) 
                {
                    if(x >= touch_areas[_this->_screen].areas[i].icon_x1 && x <= touch_areas[_this->_screen].areas[i].icon_x2 && y >= touch_areas[_this->_screen].areas[i].icon_y1 && y <= touch_areas[_this->_screen].areas[i].icon_y2) 
                    {
                        if(touch_areas[_this->_screen].areas[i].enable == false) break;
                        uint8_t gpio = touch_areas[_this->_screen].areas[i].gpio;
                        if(gpio < in.max_phys_inputs && gpio != UINT8_MAX) _this->set_button(gpio, TOUCH_TAB_STATE::ON);
                        if(gpio < in.max_inputs && gpio != UINT8_MAX && in.inputs[gpio].entry != nullptr) in.inputs[gpio].entry(gpio, in.inputs[gpio].command.c_str());
                        if(_this->_screen != old_screen) 
                        { 
                            old_screen = _this->_screen;
                            sprite_index = UINT8_MAX; 
                                    // prevent the exit command from firing as that would be for the wrong screen....
                                    // alternatively, we could save the screen for the exit, but generally, that will not be necessary
                        }
                        else sprite_index = i;
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
                auto& in = Inputs::get_inputs(_this->_screen);
                shouldProcess = true;
                if(sprite_index != UINT8_MAX)
                {
                    uint8_t gpio = touch_areas[_this->_screen].areas[sprite_index].gpio;
                    if(gpio < in.max_inputs && gpio != UINT8_MAX && in.inputs[gpio].exit != nullptr) in.inputs[gpio].exit(gpio, in.inputs[gpio].command.c_str());
                    if(gpio < in.max_phys_inputs && gpio != UINT8_MAX) _this->set_button(gpio, TOUCH_TAB_STATE::OFF);
                    sprite_index = UINT8_MAX;
                }
                gpio_intr_enable((gpio_num_t)TOUCH_IRQ_PIN);
            }
        }
    }
    Logger.Info(F("...   Touch monitoring task complete."));
    _this->_touchRunner = NULL;
    vTaskDelete(NULL);
}

/**
 * @brief Task function runnign the homing animation
 * @param args - pointer to task arguments
 */
void Display::homeing_animation_runner(void* args)
{
    LGFX_Sprite *_sprite = nullptr;
    Display *_this = reinterpret_cast<Display *>(args);
    Logger.Info(F("... Homing animation started."));
    _this->_homing_animation_break = false;
    _sprite = new LGFX_Sprite(_this);
    _sprite->createSprite(HOMING_W, HOMING_H);
    _sprite->setColorDepth(16);
    _this->actions_overlay(true, homing, homing_size);
    for(;;)
    {
        for(uint8_t frame = 0; frame < 60; frame++)
        { 
            _this->draw_homing_frame(_sprite, frame);
            vTaskDelay(pdMS_TO_TICKS(50));
            if(_this->_paused || _this->_homing_animation_break) break;
        }
        if(_this->_paused || _this->_homing_animation_break) break;
    }
    if(!_this->_paused)
    {
        _this->draw_homing_frame(_sprite, 255);
        _this->actions_overlay(false);
        _this->set_workarea_title(nullptr, 0, "");
        _this->set_button_tab(EXT_GPIO_HOME_PIN, TOUCH_TAB_STATE::OFF);
    }

    Logger.Info(F("... Homing animation complete."));
    if(_sprite != nullptr) { _sprite->deleteSprite(); delete _sprite; }
    _this->_homing_animation_break = false;
    _this->_homing_animation = NULL;
    vTaskDelete(NULL);
}

/**
 * @brief Task function runnign the feed animation
 * @param args - pointer to task arguments
 */ 
void Display::feed_animation_runner(void* args)
{
    LGFX_Sprite *_sprite = nullptr;
    Feed_TaskArgs *_args = reinterpret_cast<Feed_TaskArgs *>(args);
    Display *_this = _args->self;
    Logger.Info(F("... Feed animation started."));
    _sprite = new LGFX_Sprite(_this);
    _sprite->createSprite(HOMING_W, HOMING_H);
    _sprite->setColorDepth(16);
    _this->set_workarea_title(manual_feeding_title, manual_feeding_title_size, "");
    _this->_feed_animation_break = false;
    for(;;)
    {
        if(_this->_paused || _this->_feed_animation_break) 
        {
            if(!_this->_paused)
            {
                _this->draw_homing_frame(_sprite, 255);
                _this->set_workarea_title(nullptr, 0, "");
            }
            break;
        }

        _this->update_manual_feed_data(_args->speed_function);
        _this->draw_manual_feed_plot(_sprite);
        if (xSemaphoreTake(_this->_display_mutex, portMAX_DELAY) == pdTRUE)
        {     
            _sprite->pushSprite(88, 95);
            xSemaphoreGive(_this->_display_mutex);
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    Logger.Info(F("... Feed animation complete."));
    if(_sprite != nullptr) { _sprite->deleteSprite(); delete _sprite; }
    _this->reset_feed_data(); 
    _this->_feed_animation_break = false;
    _this->_feed_animation = NULL;
    vTaskDelete(NULL);
}

/**
 * @brief Task function runnign the cutting chart during the cutting operation
 * @param args - pointer to task arguments
 */    
void Display::cutting_chart_runner(void* args)
{
    //uint8_t i=20;
    LGFX_Sprite *_sprite = nullptr;
    Cut_TaskArgs *_args = reinterpret_cast<Cut_TaskArgs *>(args);
    Display *_this = _args->self;

    Logger.Info(F("... Cutting chart task started."));
    _sprite = new LGFX_Sprite(_this);
    _sprite->createSprite(HOMING_W, HOMING_H);
    _sprite->setColorDepth(16);
    _this->set_workarea_title(cutting_title, cutting_title_size, "");
    _this->actions_overlay(true, cutting, cutting_size, "", action_blue);
    _this->_cutting_chart_break = false;
    _this->_suspend_cutting_chart = false;
    _this->reset_cutting_data();
    for(uint16_t counter=0;;counter++)
    {
        if(_this->_paused || _this->_cutting_chart_break) 
        {
            if(!_this->_paused)
            {
                _this->draw_homing_frame(_sprite, 255);
                _this->set_workarea_title(nullptr, 0, "");
            }
            break;
        }

        if(_this->_suspend_cutting_chart)
        {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
                // cutting chart is suspended but should remain on the screen....
        }
        else
        {
            uint32_t d = _args->metrics_function();

            if(counter % 20 == 0) _this->calculate_data_max();
            _this->add_data_point(d);
            _this->draw_cutting_chart_area(_sprite);
            _this->draw_cutting_chart(_sprite);
            if (xSemaphoreTake(_this->_display_mutex, portMAX_DELAY) == pdTRUE)
            {     
                _sprite->pushSprite(88, 95);
                xSemaphoreGive(_this->_display_mutex);
            }
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
    _this->actions_overlay(false);
    Logger.Info(F("... Cutting chart task complete."));
    if(_sprite != nullptr) { _sprite->deleteSprite(); delete _sprite; }
    _this->_cutting_chart_break = false;
    _this->_cutting_chart = NULL;
    heap_caps_check_integrity_all(true);
    vTaskDelete(NULL);
}