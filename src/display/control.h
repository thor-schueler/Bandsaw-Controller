// Copyright (c) Thor Schueler. All rights reserved.
// SPDX-License-Identifier: MIT

#ifndef _CONTROL_H_
#define _CONTROL_H_

#include <Arduino.h>
#include "display.h"

/**
 * @brief defines an interface for a display control class for the bandsaw controller
 */
template<typename T>
class IControl
{
    public:
        virtual ~IControl() = default;

        virtual void load(T value) = 0;
        virtual void show() = 0;
        virtual bool has_changes() = 0;
        virtual uint8_t height() = 0;
        virtual T value() = 0;

        std::function <void(T value)>on_change = nullptr;

    protected: 
        IControl(Display& display) : display_(display) {}
        Display& display_;
};

#endif // _CONTROL_H_
