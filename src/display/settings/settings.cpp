// Copyright (c) Thor Schueler. All rights reserved.
// SPDX-License-Identifier: MIT

#include "version.h"
#include "src/display/display.h"
#include "src/display/control/control.h"
#include "src/display/checkbox/checkbox.h"
#include "src/inputs/inputs.h"
#include "src/config/config.h"
#include "src/display/dialog/dialog.h"
#include <stdio.h>
#include <stdlib.h>
#include <FunctionalInterrupt.h>
#include <variant>

const static uint8_t SETTINGS_X = 88;
const static uint8_t SETTINGS_Y = 95;
const static uint8_t SETTINGS_ICON_OFFSET_X = 8;
const static uint8_t SETTINGS_ICON_OFFSET_Y = 5;
const static uint8_t SETTINGS_ICON_W = 87;
const static uint8_t SETTINGS_ICON_H = 75;


settings_t dummy;
std::array<IBase*, 10> __controls {};
std::array<setting_screen_t, 6> __screens = {{    
    { 3, SETTING_SCREENS::GENERAL, "General", settings_general, 0, 0, Config::General(), settings_variant_t(Config::General()), general_fields, std::size(general_fields), nullptr, nullptr},
    { 4, SETTING_SCREENS::JOBS, "Jobs", settings_jobs, 0, 1, Config::Jobs(), settings_variant_t(Config::Jobs()), jobs_fields, std::size(jobs_fields), nullptr, nullptr},
    { 5, SETTING_SCREENS::TIMEOUTS, "Timeouts", settings_timeouts, 1, 2, Config::Timeouts(), settings_variant_t(Config::Timeouts()), timeout_fields, std::size(timeout_fields), nullptr, nullptr},
    { 6, SETTING_SCREENS::INVALID, "", nullptr, 0, 0, dummy, dummy, nullptr, 0, nullptr, nullptr},
    { 7, SETTING_SCREENS::INVALID, "", nullptr, 0, 0, dummy, dummy, nullptr, 0, nullptr, nullptr},
    { 8, SETTING_SCREENS::INVALID, "", nullptr, 0, 0, dummy, dummy, nullptr, 0, nullptr, nullptr}
}};
setting_screens_t __current_screen = SETTING_SCREENS::INVALID;
bool __needs_saving = false;
LGFX_Sprite *__s = nullptr;

/**
 * @brief Local helper to replace the underscores in a string with spaces and to capitalize each word
 * 
 * @param s - String to process
 * @return String cotaining the result.
 */
String prettify(String s)
{
    bool capitalize = true;
    for (char& c : s)
    {
        if (c == '_') { c = ' '; capitalize = true; }
        else if (capitalize)
        {
            c = std::toupper(static_cast<unsigned char>(c));
            capitalize = false;
        }
    }
    return s;
}

/**
 * @brief Toggles the settings screen on an off.
 * @param force_on - true to force the settings display on. Defaults to false. 
 */
void Display::toggle_settings(bool force_on)
{
    
    if(this->_screen != SCREENS::SETTINGS || (this->_screen == SCREENS::SETTINGS && __current_screen != SETTING_SCREENS::INVALID) || force_on)
    {
        LGFX_Sprite controls(this);
        _screen = SCREENS::SETTINGS;
        this->pause_tasks();
        vTaskDelay(pdMS_TO_TICKS(100));
        this->draw_canvas();
        this->set_workarea_title(settings_title, settings_title_size, "");
        this->draw_settings_canvas();
        controls.setColorDepth(16);
        controls.createSprite(127, 143);
        controls.fillSprite(TFT_BLACK);
        controls.pushImage(0, 0, 127, 143, (lgfx::rgb565_t*)settings_controls);
        if (xSemaphoreTake(this->_display_mutex, portMAX_DELAY) == pdTRUE)
        { 
            controls.pushSprite(353, 58);
            xSemaphoreGive(this->_display_mutex);
        }
        controls.deleteSprite();

        if(__current_screen == SETTING_SCREENS::INVALID && !__needs_saving)
        {
            //Config::print();
            for(setting_screen_t& _s: __screens)
            {
                switch(_s.section)
                {
                    case SETTING_SCREENS::GENERAL: 
                        _s.new_settings = settings_variant_t(Config::General()); 
                        //Logger.Info("General Copy"); 
                        //for (size_t i=0; i < _s.fields_size; ++i)
                        //{
                        //    const field_info_t field = _s.fields[i];
                        //    const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&_s.new_settings) + field.offset;
                        //    switch(field.type)
                        //    {
                        //        case field_type_t::bool_t:
                        //            Logger.Info_f(field.format_string, field.name, *reinterpret_cast<const bool*>(ptr) ? "true" : "false");
                        //            break;
                        //        case field_type_t::uint8_t:
                        //            Logger.Info_f(field.format_string, field.name, *reinterpret_cast<const uint8_t*>(ptr));
                        //            break; 
                        //        case field_type_t::uint16_t:
                        //            Logger.Info_f(field.format_string, field.name, *reinterpret_cast<const uint16_t*>(ptr));
                        //            break;     
                        //        case field_type_t::uint32_t:
                        //            Logger.Info_f(field.format_string, field.name, *reinterpret_cast<const uint32_t*>(ptr));
                        //            break;            
                        //        case field_type_t::float_t:
                        //            Logger.Info_f(field.format_string, field.name, *reinterpret_cast<const float*>(ptr));
                        //            break;
                        //        default:
                        //            Logger.Info_f(F("   %s: %u (unknown type, assuming one byte value)"), field.name, *ptr);
                        //    }
                        //}
                        //Logger.Info("General Done"); 
                        break;
                    case SETTING_SCREENS::JOBS: _s.new_settings = settings_variant_t(Config::Jobs()); break;
                    case SETTING_SCREENS::TIMEOUTS: _s.new_settings = settings_variant_t(Config::Timeouts()); break;
                    default:
                        _s.new_settings = _s.settings;
                }
            }
        }
    }
    else
    {   if(__needs_saving)
        {
            Dialog d(this);
            Logger.Info("Entering Dialog");
            if(d.show("Discard changes and exit?"))
            {
                Logger.Info("Dialog done - proceed");
                _screen = SCREENS::MAIN;
                this->draw_canvas();
                this->resume_tasks();
                    // we are on the main settings screen. 
                    // return to the main screen
            }
            else
            {
                Logger.Info("Dialog done - cancel");
                this->toggle_settings(true);
            }
        }
        else
        {
            if(__s != nullptr)
            { 
                if(__s->getBuffer() != nullptr) __s->deleteSprite();
                delete __s;
                __s = nullptr;
            }
            _screen = SCREENS::MAIN;
            this->draw_canvas();
            this->resume_tasks();
                // we are on the main settings screen. 
                // return to the main screen
        }
    }
    __current_screen = SETTING_SCREENS::INVALID;
}

/** 
 * @brief Draws the main canvas of the settings screen
 */
void Display::draw_settings_canvas()
{
    LGFX_Sprite section(this);
    section.setColorDepth(16);
    section.createSprite(59,59);

    // ensure previous controls are deleted if necessary
    for(uint8_t i=0; i<__controls.size(); i++) if(__controls[i] != nullptr) { delete __controls[i]; __controls[i] = nullptr; } 

    for (const auto& s : __screens) 
    {
        if(s.section == SETTING_SCREENS::INVALID) continue;
        if(s.icon == nullptr) continue;  

        section.pushImage(0, 0, 59, 59, (lgfx::rgb565_t *)s.icon);
        if (xSemaphoreTake(this->_display_mutex, portMAX_DELAY) == pdTRUE)
        { 
            section.pushSprite(
                    SETTINGS_X + SETTINGS_ICON_OFFSET_X + s.icon_column * SETTINGS_ICON_W, 
                    SETTINGS_Y + SETTINGS_ICON_OFFSET_X + s.icon_row * SETTINGS_ICON_H);
            xSemaphoreGive(this->_display_mutex);
        }
    }
    section.deleteSprite();
}

/**
 * @brief Draws teh detail screen for a particular setting section
 * @param id - ID of the settings screen
 * @param command - Command name passed in from the input watcher
 * 
 * @remarks - the function signature is a delegate for the input watcher
 */
void Display::draw_settings_detail(uint8_t id, const char*)
{
    setting_screen_t *ps = nullptr;
    for(auto &s : __screens) if(s.id == id) { ps = &s; break; }
    __current_screen = ps->section;

    // draw the controls for the various sections here...
    const settings_t& settings = ps->settings;
    const field_info_t* fields = ps->fields;
    const size_t size = ps->fields_size; 

    //Logger.Info("before");
    //for (size_t i=0; i < size; ++i)
    //{
    //    const field_info_t field = fields[i];
    //    const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&(ps->new_settings)) + field.offset;
    //    switch(field.type)
    //    {
    //        case field_type_t::bool_t:
    //            Logger.Info_f(field.format_string, field.name, *reinterpret_cast<const bool*>(ptr) ? "true" : "false");
    //            break;
    //        case field_type_t::uint8_t:
    //            Logger.Info_f(field.format_string, field.name, *reinterpret_cast<const uint8_t*>(ptr));
    //            break; 
    //        case field_type_t::uint16_t:
    //            Logger.Info_f(field.format_string, field.name, *reinterpret_cast<const uint16_t*>(ptr));
    //            break;     
    //        case field_type_t::uint32_t:
    //            Logger.Info_f(field.format_string, field.name, *reinterpret_cast<const uint32_t*>(ptr));
    //            break;            
    //        case field_type_t::float_t:
    //            Logger.Info_f(field.format_string, field.name, *reinterpret_cast<const float*>(ptr));
    //            break;
    //        default:
    //            Logger.Info_f(F("   %s: %u (unknown type, assuming one byte value)"), field.name, *ptr);
    //    }
    //}
    //Logger.Info("before done");

    std::visit([&](auto& s)
    {
        //Logger.Info("inside");
        //for (size_t i=0; i < size; ++i)
        //{
        //    const field_info_t field = fields[i];
        //    const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&s) + field.offset;
        //    switch(field.type)
        //    {
        //        case field_type_t::bool_t:
        //            Logger.Info_f(field.format_string, field.name, *reinterpret_cast<const bool*>(ptr) ? "true" : "false");
        //            break;
        //        case field_type_t::uint8_t:
        //            Logger.Info_f(field.format_string, field.name, *reinterpret_cast<const uint8_t*>(ptr));
        //            break; 
        //        case field_type_t::uint16_t:
        //            Logger.Info_f(field.format_string, field.name, *reinterpret_cast<const uint16_t*>(ptr));
        //            break;     
        //        case field_type_t::uint32_t:
        //            Logger.Info_f(field.format_string, field.name, *reinterpret_cast<const uint32_t*>(ptr));
        //            break;            
        //        case field_type_t::float_t:
        //            Logger.Info_f(field.format_string, field.name, *reinterpret_cast<const float*>(ptr));
        //            break;
        //        default:
        //            Logger.Info_f(F("   %s: %u (unknown type, assuming one byte value)"), field.name, *ptr);
        //    }
        //}
        //Logger.Info("inside done");
        this->draw_settings(s, fields, size);
    }, ps->new_settings);
    this->set_workarea_title(settings_title, settings_title_size, " - " + String(ps->name));
}

/**
 * @brief Draws the settings controls for a certain settings section
 * @param value - settings enumeration. Must derive from settings_t
 * @param fields - the fields enumeration for the settings enumeration
 * @param fields_size - the number of fields in fields
 */
void Display::draw_settings(const settings_t& value, const field_info_t* fields, size_t fields_size)
{
    if(__s == nullptr)
    {
        __s = new LGFX_Sprite(this);
        __s->setColorDepth(16);
    }
    if(__s->getBuffer() != nullptr) __s->deleteSprite();
    __s->createSprite(HOMING_W, HOMING_H);
    __s->fillSprite(TFT_BLACK);
    uint16_t y = 0;

    for (size_t i=0; i < fields_size; ++i)
    {
        const field_info_t field = fields[i];
        const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&value) + field.offset;
        switch(field.type)
        {
            case field_type_t::bool_t:
                if(__controls[i] == nullptr)
                { 
                    __controls[i] = new Checkbox(field.name, prettify(field.name));
                    __controls[i]->copy_from(ptr);
                    __controls[i]->on_change = std::bind(&Display::on_control_change, this, std::placeholders::_1);
                }
                y += __controls[i]->show(*__s, 0, y, HOMING_W);
                //Logger.Info_f(field.format_string, field.name, *reinterpret_cast<const bool*>(ptr) ? "true" : "false");
                break;
            case field_type_t::uint8_t:
                //Logger.Info_f(field.format_string, field.name, *reinterpret_cast<const uint8_t*>(ptr));
                break; 
            case field_type_t::uint16_t:
                //Logger.Info_f(field.format_string, field.name, *reinterpret_cast<const uint16_t*>(ptr));
                break;     
            case field_type_t::uint32_t:
                //Logger.Info_f(field.format_string, field.name, *reinterpret_cast<const uint32_t*>(ptr));
                break;            
            case field_type_t::float_t:
                //Logger.Info_f(field.format_string, field.name, *reinterpret_cast<const float*>(ptr));
                break;
            default:
                Logger.Info_f(F("   %s: %u (unknown type, assuming one byte value)"), field.name, *ptr);
        }
    }

    if(xSemaphoreTake(this->_display_mutex, portMAX_DELAY) == pdTRUE)
    { 
        __s->pushSprite(SETTINGS_X, SETTINGS_Y);
        xSemaphoreGive(this->_display_mutex);
    }    
}

/**
 * @brief Get the settings screens object containing information about the settings screens
 * 
 * @return std::array<settings_screen_t, 6> 
 */
std::array<setting_screen_t, 6>& Display::get_settings_screens() { return __screens; }

/**
 * @brief Gets the current settings screens object containing information about the settings screen
 * 
 * @return settings_screen_t& 
 */
const setting_screens_t& Display::get_current_settings_screen() { return __current_screen; }


/**
 * @brief Save the settings to NVMS
 * @param id - Comamnd id
 * @param command - Command name passed in from the input watcher
 * 
 * @remarks - the function signature is a delegate for the input watcher
 */
void Display::save_settings(uint8_t gpio, const char*)
{
    if(!__needs_saving) return;

    Logger.Info("Save settings");
    for(auto &s : __screens) 
    {
        std::visit([&](auto& ns)
        {
            for (size_t i=0; i < s.fields_size; ++i)
            {
                const field_info_t field = s.fields[i];
                uint8_t* ptr_dst = reinterpret_cast<uint8_t*>(&(s.settings)) + field.offset;
                uint8_t* ptr_src = reinterpret_cast<uint8_t*>(&ns) + field.offset;
                memcpy(ptr_dst, ptr_src, field.size);
            }
        }, s.new_settings);
    }
    Config::save();
    __needs_saving = false;
    this->_settings_status |= 0x01;
    esp_timer_start_once(this->status_timer, STATUSTIMEOUT);
}

/**
 * @brief Apply the defaults to the settings
 * @param id - Command ID.
 * @param command - Command name passed in from the input watcher
 * 
 * @remarks - the function signature is a delegate for the input watcher
 */
void Display::default_settings(uint8_t gpio, const char*)
{
    Logger.Info("Restore Defaults");
    ESP.restart();
}

/**
 * @brief Process touch input for settings detail screens
 * @param x - touch x coordinate
 * @param y - touch y coordinate
 */
void Display::process_settings_screen_touch(uint16_t x, uint16_t y)
{
    // here we need to iterate though all of the created controls and figure out 
    // which control to dispatch the touch to. We need to adjust the passed through
    // coordinates by SETTINGS_X and SETTINGS_Y since the controls operate relative to that
    // coordinate. 
    for(auto c : __controls) if(c != nullptr) c->process_touch(x-SETTINGS_X, y-SETTINGS_Y);  
}

/**
 * @brief Callback called when a control changes its value. Initiates saving to settings
 * and redrawing the control
 * 
 * @param src - The control raising the event.
 */
void Display::on_control_change(IBase* src)
{
    if(__current_screen == SETTING_SCREENS::INVALID) return;

    setting_screen_t *ps = nullptr;
    for(auto &s : __screens) if(s.section == __current_screen) { ps = &s; break; }
    const field_info_t* fields = ps->fields;
    const size_t size = ps->fields_size; 

    std::visit([&](auto& settings)
    {
        for (size_t i=0; i < size; ++i)
        {
            const field_info_t field = fields[i];
            if(src->name() != field.name) continue;

            uint8_t* ptr = reinterpret_cast<uint8_t*>(&settings) + field.offset;
            switch(field.type)
            {
                case field_type_t::bool_t:
                    Checkbox* c = static_cast<Checkbox*>(src);
                    if(c->has_changes())
                    {
                        *reinterpret_cast<bool*>(ptr) = c->value();
                        c->redraw_volatiles(*__s); 
                        __needs_saving = true; 
                    }
                    break;
            }
        }
    },
    ps->new_settings);
    
    if(xSemaphoreTake(this->_display_mutex, portMAX_DELAY) == pdTRUE)
    { 
        __s->pushSprite(SETTINGS_X, SETTINGS_Y);
        xSemaphoreGive(this->_display_mutex);
    }    
}