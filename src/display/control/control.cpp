// Copyright (c) Thor Schueler. All rights reserved.
// SPDX-License-Identifier: MIT

#include "src/display/display.h"
#include "src/display/control/control.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

/**
 * @brief Draws word wrapped text into the sprite
 * 
 * @param sprite - The sprite hosting the canvas to draw into
 * @param text - The text to draw
 * @param x - The x coordinate for the start
 * @param y - The y coordinate for the end
 * @param maxWidth - The width to fit the text into 
 * 
 * @return uint8_t height of the text block that has been rendered.
 * @remarks The method will not check for vertical overrun. Vertical overrung will simply be truncated.
 */
uint8_t IBase::draw_wrapped_text(LGFX_Sprite& sprite, const char* text, int x, int y, int maxWidth)
{
    String t(text);
    String line;

    sprite.setTextColor(BORG_GREEN);
    sprite.setFont(&fonts::FreeSans9pt7b);
    sprite.setTextSize(1);

    int lineHeight = sprite.fontHeight();
    int currentY = y;
    int start = 0;

    while (start < t.length())
    {
        int end = t.indexOf(' ', start);
        if (end == -1) end = t.length();

        String word = t.substring(start, end);
        String candidate = line.length() ? line + " " + word : word;

        if (sprite.textWidth(candidate) <= maxWidth) line = candidate;
        else
        {
            sprite.drawString(line, x, currentY);
            currentY += lineHeight + 2;
            line = word;
        }
        start = end + 1;
    }
    if (line.length()) sprite.drawString(line, x, currentY);
    return currentY - y + lineHeight + 2;
}

