// Copyright (c) Thor Schueler. All rights reserved.
// SPDX-License-Identifier: MIT

#ifndef _CONFIG_H_
#define _CONFIG_H_


#include "Arduino.h"
#include "src/logging/SerialLogger.h"

enum class field_type_t
{
    bool_t,
    uint8_t,
    uint16_t,
    uint32_t,
    float_t
};

typedef struct FIELD_INFO
{
    const char* name;
    size_t offset;
    field_type_t type;
    const char* format_string;
    
} field_info_t;
    
#define FIELD_bool  field_type_t::bool_t
#define FIELD_uint8_t  field_type_t::uint8_t
#define FIELD_uint16_t field_type_t::uint16_t
#define FIELD_uint32_t field_type_t::uint32_t
#define FIELD_float field_type_t::float_t


#define FIELD_bool_FORMAT      "   %s: %s"
#define FIELD_uint8_t_FORMAT   "   %s: %u"
#define FIELD_uint16_t_FORMAT  "   %s: %u"
#define FIELD_uint32_t_FORMAT  "   %s: %u"
#define FIELD_float_FORMAT     "   %s: %.3f"


/** Settings base structure. You should not use this directly. */
typedef struct SETTINGS
{
    uint32_t magic = 0xCAFEBABE;
    uint8_t version = 1;
    uint32_t crc = 0;
} settings_t;
struct GENERAL_SETTINGS;
struct TIMEOUT_SETTINGS;
struct JOB_SETTINGS;

#define DECLARE(settings_type, member_type, name, value) member_type name = value;
#define GENERAL_FIELDS(X)  \
    X(GENERAL_SETTINGS, bool, activate_backlight_with_cold_light , true) \ 
    X(GENERAL_SETTINGS, bool, activate_backlight_with_warm_light, false) 

#define TIMEOUT_FIELDS(X) \
    X(TIMEOUT_SETTINGS, uint16_t, task_shutdown_timeout , 2000) 

#define JOBS_FIELDS(X) \
    X(JOB_SETTINGS, bool, use_task_for_manual_feeding, false) 

/**
 * Stores general settings
 */
typedef struct GENERAL_SETTINGS : SETTINGS
{
    GENERAL_FIELDS(DECLARE)

    uint8_t reserved[62];
        // when adding new settings, remember to lower the dimension of the reserved 
        // array by the number of bytes of the new settings added to keep 
        // the overall size the same

} general_settings_t;

typedef struct TIMEOUT_SETTINGS : SETTINGS
{
    TIMEOUT_FIELDS(DECLARE)

    uint8_t reserved[63];
        // when adding new settings, remember to lower the dimension of the reserved 
        // array by the number of bytes of the new settings added to keep 
        // the overall size the same
} timeout_settings_t;

typedef struct JOB_SETTINGS : SETTINGS
{
    JOBS_FIELDS(DECLARE)

    uint8_t reserved[63];
        // when adding new settings, remember to lower the dimension of the reserved 
        // array by the number of bytes of the new settings added to keep 
        // the overall size the same
} job_settings_t;

#undef DECLARE

#define FIELD(settings_type, member_type, name, value) {#name, offsetof(settings_type, name), FIELD_##member_type, FIELD_##member_type##_FORMAT},
inline field_info_t general_fields[] =
{
    GENERAL_FIELDS(FIELD)
};

inline field_info_t timeout_fields[] =
{
    TIMEOUT_FIELDS(FIELD)
};

inline field_info_t jobs_fields[] =
{
    JOBS_FIELDS(FIELD)
};

#undef FIELD

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
        static void save() { if(_instance == nullptr) _instance = new Config(); _instance->store(); } 

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

        /**
         * @brief Persists the settings and configuration to NVS.
         */
        void store();

    private:



        inline static Config* _instance = nullptr;

        general_settings_t general;
        job_settings_t jobs;
        timeout_settings_t timeouts;

};


#endif //_CONFIG_H_