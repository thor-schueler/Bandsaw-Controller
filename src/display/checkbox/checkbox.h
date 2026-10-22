// Copyright (c) Thor Schueler. All rights reserved.
// SPDX-License-Identifier: MIT

#ifndef _CHECKBOX_H_
#define _CHECKBOX_H_

#include <Arduino.h>
#include "src/display/display.h"
#include "src/display/control.h"

#define CHKBOX_HEIGHT 10

class Checkbox : IControl<bool>
{
    public:
       Checkbox(Display &display) : IControl(display) {}
       
       void load(bool value) override { _value = value; }
       void show() override;
       bool has_changes() override { return _has_changes; }
       bool value() override { return _value; }
       uint8_t height() override { return CHKBOX_HEIGHT; }

    private:
        bool _value;
        bool _has_changes = false;

};

#endif // _CHECKBOX_H_