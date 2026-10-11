// Copyright (c) Thor Schueler. All rights reserved.
// SPDX-License-Identifier: MIT

#include "config.h"
#include "src/logging/SerialLogger.h"
#include <Preferences.h>
#include <esp_crc.h>

/**
 * @brief Calculates the CRC of a settings derived object. 
 */
template<typename T>
uint32_t calculate_crc(const T& s)
{
    static_assert(std::is_base_of_v<settings_t, T>, "T must derive from settings_t");

    const uint8_t* start = reinterpret_cast<const uint8_t*>(&s.crc) + sizeof(s.crc);
    size_t length = sizeof(T)
            - (reinterpret_cast<const uint8_t*>(&s.crc) + sizeof(s.crc)
            - reinterpret_cast<const uint8_t*>(&s));
    return esp_crc32_le(0, start, length);
}

/** 
 * @brief Reads a settings section and does some basic validation.
 */
template<typename T>
bool read_config_section(const char* name, T& s)
{
    static_assert(std::is_base_of_v<settings_t, T>, "T must derive from settings_t");

    T temp; 
    Preferences prefs;
    prefs.begin("config", true);
    size_t len = prefs.getBytesLength(name); 
    if (len == sizeof(T)) Logger.Error_f(F("Length of settings section %s is incorrect. Expecting %u, got %u."), name, sizeof(T), len);
    else
    {
        prefs.getBytes(name, &temp, sizeof(T));
        if(s.version != temp.version) Logger.Error_f(F("Version mismatch for settings %s. Expected version %u, actual version %u."), name, s.version, temp.version);
        else if (s.magic != temp.magic) Logger.Error_f(F("Magic cookie mismatch for settings %s. Expected 0x%04X, actual 0x%04X."), name, s.magic, temp.magic);
        else if(temp.crc != calculate_crc(temp)) Logger.Error_f(F("CRC of stored %s does not match indicating settings corruption"), name);
        else
        {
            s = temp;
            prefs.end();
            return true;
        }
    }
    return false;
}

template<typename T>
void print_section(const char* name, const T& s)
{
    static_assert(std::is_base_of_v<settings_t, T>, "T must derive from settings_t");
    Logger.Info_f(F("Setting for section %s"), name);
    Logger.Info_f(F("   Settings Version: %u"), s.version);
    Logger.Info_f(F("   Magic cookie: 0x%04X"), s.magic);
    Logger.Info(F(""));
}

/**
 * Creates a new instance of the config class
 */
Config::Config()
{
    this->load();
}

/**
 * @brief Destroy the Config object and clean up associated resources
 * 
 */
Config::~Config()
{
    this->save();
}

/**
 * @brief Loads the configuration and settings from NVS 
 */
void Config::load()
{
    read_config_section("general", general);
    read_config_section("jobs", jobs);
    read_config_section("timeouts", timeouts);
}

/**
 * @brief Persists the settings and configuration to NVS.
 */
void Config::save()
{
    this->general.crc = calculate_crc(this->general);
    this->jobs.crc = calculate_crc(this->jobs);
    this->timeouts.crc = calculate_crc(this->timeouts);

    Preferences prefs;
    prefs.begin("config", false);
    prefs.putBytes("general", &general, sizeof(general));
    prefs.putBytes("jobs", &jobs, sizeof(jobs));
    prefs.putBytes("timeouts", &timeouts, sizeof(timeouts));
    prefs.end();
}

/**
 * @brief Prints the settings to the console
 */
void Config::print()
{
    if(Config::_instance == nullptr) Config::_instance = new Config();
    print_section("General", Config::General());
    print_section("Jobs", Config::Jobs());
    print_section("Timeouts", Config::Timeouts());
}
