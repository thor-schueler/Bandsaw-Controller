// Copyright (c) Thor Schueler. All rights reserved.
// SPDX-License-Identifier: MIT

#ifndef _CONFIG_H_
#define _CONFIG_H_


#include "Arduino.h"
#include "src/logging/SerialLogger.h"

/** Settings base structure. You should not use this directly. */
typedef struct SETTINGS
{
    uint32_t magic = 0xCAFEBABE;
    uint8_t version = 1;
    uint32_t crc = 0;
} settings_t;

/**
 * Stores general settings
 */
typedef struct GENERAL_SETTINGS : SETTINGS
{

    uint8_t reserved[64];
        // when adding new settings, remember to lower the dimension of the reserved 
        // array by the number of bytes of the new settings added to keep 
        // the overall size the same

} general_settings_t;

typedef struct TIMEOUT_SETTINGS : SETTINGS
{

    uint8_t reserved[64];
        // when adding new settings, remember to lower the dimension of the reserved 
        // array by the number of bytes of the new settings added to keep 
        // the overall size the same
} timeout_settings_t;

typedef struct JOB_SETTINGS : SETTINGS
{

    uint8_t reserved[64];
        // when adding new settings, remember to lower the dimension of the reserved 
        // array by the number of bytes of the new settings added to keep 
        // the overall size the same
} job_settings_t;


/**
 * @brief Manages the blade movement, feed movement and other ciritcal items for bandsaw.
 * 
 */
class Config
{
    public:

        /**
         * @brief Gets the General settings
         */
        static general_settings_t& General() { if(_instance == nullptr) _instance = new Config(); return _instance->general; }

        /**
         * @brief Gets the Job related settings
         */
        static job_settings_t& Jobs() { if(_instance == nullptr) _instance = new Config(); return _instance->jobs; }

        /**
         * Gets the Timeout settings
         */
        static timeout_settings_t& Timeouts() { if(_instance == nullptr) _instance = new Config(); return _instance->timeouts; }

        /**
         * @brief Persists the settings and configuration to NVS.
         */
        void save();

        /**
         * @brief Prints the settings to the console
         */
        static void print();

    protected:

        /**
         * Creates a new instance of the config class
         */
        Config();
        
        /**
         * @brief Destroy the Config object and clean up associated resources
         * 
         */
        ~Config();

        /**
         * @brief Loads the configuration and settings from NVS 
         */
        void load();

    private:



        inline static Config* _instance = nullptr;

        general_settings_t general;
        job_settings_t jobs;
        timeout_settings_t timeouts;

};


#endif //_CONFIG_H_