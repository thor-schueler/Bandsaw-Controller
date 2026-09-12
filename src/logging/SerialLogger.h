// Copyright (c) Microsoft Corporation. All rights reserved.
// Copyright (c) Thor Schueler. All rights reserved.
// SPDX-License-Identifier: MIT

#ifndef SERIALLOGGER_H
#define SERIALLOGGER_H

#include <Arduino.h>
#include <HardwareSerial.h>

#ifndef SERIAL_LOGGER_BAUD_RATE
#define SERIAL_LOGGER_BAUD_RATE 115200
#endif

/**
 * @brief Allows logging of messages and errors to the serial console.
 * 
 */
class SerialLogger
{
public:
  /**
   * @brief Construct a new Serial Logger:: Serial Logger object
   * 
   */
  SerialLogger();
  
  /**
   * @brief Logs an information message to the serial console. 
   * 
   * @param message The message to log
   */
  void Info(String message);

  /**
   * @brief Logs a formatted message to the serial console. Follows print_f conventions.
   * 
   * @param format The format string
   * @param ... Argument list for the token replacement in the format string.
   * @return size_t The lenght of the actual string logged.
   */ 
  size_t Info_f(String format, ...);

  /**
   * @brief Logs an error message to the serial console. 
   * 
   * @param message The message to log
   */  
  void Error(String message);

  /**
   * @brief Logs a formatted error to the serial console. Follows print_f conventions.
   * 
   * @param format The format string
   * @param ... Argument list for the token replacement in the format string.
   * @return size_t The lenght of the actual string logged.
   */ 
  size_t Error_f(String format, ...);

  /**
   * @brief Sets the transmission speed
   * @param speed - the transmission speed. 
   */
  void SetSpeed(uint32_t speed);

private:
  /**
   * @brief Writes the current time inline to the console. 
   * 
   */
  void writeTime();
};

/**
 * @brief Global instance to be used for logging
 * 
 */
extern SerialLogger Logger;

#endif // SERIALLOGGER_H
