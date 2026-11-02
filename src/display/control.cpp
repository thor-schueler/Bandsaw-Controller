// Copyright (c) Thor Schueler. All rights reserved.
// SPDX-License-Identifier: MIT

#include "control.h"
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
    int lineHeight = sprite.fontHeight();
    int currentY = y;
    int start = 0;

    sprite.setTextColor(BORG_GREEN);
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

