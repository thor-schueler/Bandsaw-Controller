// Copyright (c) theRealThor. All rights reserved.
// SPDX-License-Identifier: MIT

/*
 * This is the firmware for a handwheel compatible with Candle 2.0. To use this, you need to run Candle 2.0 on 
 * your desktop, connect the ESP32 to your dektop via USB and configure Candle 2.0 to use the appropriate 
 * handwheel port to communicate with the ESP. 
 *   
 */

#define HIGH_WATER_MARK_LOOP_SKIP 120

#include "Arduino.h"
#include "ESP.h"
#include <SPI.h>
#include <esp_heap_caps.h>

// C99 libraries
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <new>

// local libraries
#include "version.h"
//#include "src/config/config_page.h"
#include "src/logging/SerialLogger.h"
#include "src/controller/controller.h"

#include <TMCStepper.h>
#include "driver/ledc.h"

//#define TELEMETRY_FREQUENCY_MILLISECS 120000
//#define AP_ENABLE_PIN 5

//static Config config;
//Wheel *wheel = NULL;
//TaskHandle_t wifi_task = NULL;
//BluetoothSerial bt;
//static bool has_wifi = true;
//static bool config_mode = false;

#ifdef SET_LOOP_TASK_STACK_SIZE
SET_LOOP_TASK_STACK_SIZE(3052);
  //
  // This will only work with ESP-Arduino 2.0.7 or higher. 
  //
#endif

Controller* controller = NULL;


/**
 * @brief Performs system setup activities, including connecting to WIFI, setting time, obtaining the IoTHub info 
 * from DPS if necessary and connecting the IoTHub. Use this method to also register various delegates and command
 * handlers.
 * 
 */
void setup()
{
  delay(2000);
  // Enable PSRAM if available. This will allow us to use the PSRAM for dynamic memory allocation, 
  // which is crucial for the display and other memory intensive operations.
  // esp_log_level_set("*", ESP_LOG_NONE);
  if (psramFound()) {
    heap_caps_malloc_extmem_enable(256);   // allow malloc/new to use PSRAM, threshold in bytes
  }

  //
  // Initialize configuration data from EEPROM
  //
  //config.Initialize(false);
  //Command_t::merge(&config.Commands, &Wheel::Commands);
  //Logger.SetSpeed(config.baud_rate);

  //
  // Startup
  //
  Logger.Info_f(F("Copyright 2026, Thor Schueler, Firmware Version: %d.%d.%d"), FW_VERSION_MAJOR, FW_VERSION_MINOR, FW_BUILD_NUMBER);
  Logger.Info_f(F("Loop task stack size: %i"), getArduinoLoopTaskStackSize());
  Logger.Info_f(F("Loop task stack high water mark: %i"), uxTaskGetStackHighWaterMark(NULL));
  Logger.Info_f(F("Total heap: %d"), ESP.getHeapSize()); 
  Logger.Info_f(F("Free heap: %d"), ESP.getFreeHeap()); 
  Logger.Info_f(F("Total PSRAM: %d"), ESP.getPsramSize()); 
  Logger.Info_f(F("Free PSRAM: %d"), ESP.getFreePsram());
  Logger.Info(F("... Startup"));
  //config.Print();
  
  controller = new Controller();
  controller->begin();

  Logger.Info(F("... Init done"));
  Logger.Info_f(F("Free heap: %d"), ESP.getFreeHeap()); 
  Logger.Info_f(F("Free PSRAM: %d"), ESP.getFreePsram());
}

/**
 * @brief Main loop. Use this loop to execute recurring tasks. In this sample, we will periodically send telemetry
 * and query and update the device twin.  
 * 
 */
void loop()
{
  vTaskDelay(100);
}
