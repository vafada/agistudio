/*
 *  QT AGI Studio :: Copyright (C) 2000 Helen Zommer
 *
 *  A big part of this code is adapted from the original (MSDOS)
 *  Picedit developed by Lance Ewing.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */


#include <algorithm>
#include <fstream>

#include <QColor>
#include <QFileInfo>
#include <QImage>
#include <QStringList>

#include "picture.h"
#include "menu.h"
#include "game.h"
#include "wutil.h"


static bool picDrawEnabled0, priDrawEnabled0;
static int picColour0, priColour0;

//*********************************************************
Picture::Picture() :
    brushSize(1), brushTexture(), brushShape(), picPos(picCodes.begin()),
    bg_on(false), add_pic(), add_pri(), bgpix(), buf(), clickX(), clickY(),
    code_pic(), code_pri(), curcol(), curp(), dX(), dY(), drawing_mode(),
    firstClick(), newp(), numClicks(), patCode(), patNum(), picColour(),
    picDrawEnabled(), picture(), pptr(), priColour(), priDrawEnabled(),
    priority(), refill_pic(), refill_pri(), refill_x(), refill_y(),
    rpos(), spos(), stepClicks(), tool()
{ }

//*********************************************************
const QString Picture::showPos(byte *code, byte *val) const
//show current picture buffer position
{
    QString codestring;
    int count;
    actionListIter tempIter;

    if (picPos != picCodes.end()) {
        *code = *picPos;
        if (std::next(picPos) != picCodes.end())
            *val = *std::next(picPos);
    }
    for (tempIter = picPos, count = 0; ((count < 6) && (tempIter != picCodes.end())); count++, tempIter++)
        codestring += QString("%1").arg(QString::number(*tempIter, 16), 2, QChar('0'));
    return codestring;
}

const uint32_t Picture::getPos() const
{
    return std::distance(picCodes.begin(), (actionList::const_iterator)picPos);
}

//*********************************************************
int Picture::setBufPos(int inputValue)
//set current picture buffer position to inputValue
{
    if ((inputValue < 0) || (inputValue > picCodes.size()))
        return 1;

    if (inputValue == picCodes.size())
        picPos = picCodes.end();
    else {
        /* Advance to requested location */
        picPos = picCodes.begin();
        std::advance(picPos, inputValue);

        /* Walk backward to the beginning of the current action position */
        while (*picPos < 0xF0)
            picPos--;
    }
    draw();
    init_tool();
    return 0;
}

//*********************************************************
void Picture::qstore(byte q)
{
    if (spos + 1 == rpos || (spos + 1 == QUMAX && !rpos))
        return;
    buf[spos] = q;
    spos++;
    if (spos == QUMAX)
        spos = 0;  /* loop back */
}

//*********************************************************
byte Picture::qretrieve()
{
    if (rpos == QUMAX)
        rpos = 0; /* loop back */
    if (rpos == spos)
        return EMPTY;
    rpos++;
    return buf[rpos - 1];
}

/**************************************************************************
** getCode
**
** Gets the next picture code from the linked list.
**************************************************************************/
byte Picture::getCode(actionListIter *pos) const
{
    byte retVal;

    if (*pos == picCodes.end())
        return DrawEnd;

    retVal = **pos;
    (*pos)++;

    return retVal;
}

byte Picture::testCode(actionListIter *pos) const
{
    if (*pos == picCodes.end())
        return 0xFF;

    return **pos;
}


/**************************************************************************
** picPSet
**
** Draws a pixel in the picture screen.
**************************************************************************/
void Picture::picPSet(word x, word y)
{
    x <<= 1;
    if (x >= MAX_W)
        return;
    if (y >= MAX_H)
        return;

    picture[y * MAX_W + x] = picColour;
    picture[y * MAX_W + x + 1] = picColour;
}

/**************************************************************************
** priPSet
**
** Draws a pixel in the priority screen.
**************************************************************************/
void Picture::priPSet(word x, word y)
{
    x <<= 1;
    if (x >= MAX_W)
        return;
    if (y >= MAX_H)
        return;
    priority[y * MAX_W + x] = priColour;
    priority[y * MAX_W + x + 1] = priColour;
}

/**************************************************************************
** pset
**
** Draws a pixel in each screen depending on whether drawing in that
** screen is enabled or not.
**************************************************************************/
void Picture::pset(word x, word y)
{
    if (picDrawEnabled)
        picPSet(x, y);
    if (priDrawEnabled)
        priPSet(x, y);
}

/**************************************************************************
** picGetPixel
**
** Get colour at x,y on the picture page.
**************************************************************************/
byte Picture::picGetPixel(word x, word y) const
{
    x <<= 1;
    if (x >= MAX_W)
        return 4;
    if (y >= MAX_H)
        return 4;

    return (picture[y * MAX_W + x]);
}

/**************************************************************************
** priGetPixel
**
** Get colour at x,y on the priority page.
**************************************************************************/
byte Picture::priGetPixel(word x, word y) const
{
    x <<= 1;
    if (x >= MAX_W)
        return 4;
    if (y >= MAX_H)
        return 4;

    return (priority[y * MAX_W + x]);
}

/**************************************************************************
** round
**
** Rounds a float to the closest int. Takes into actions which direction
** the current line is being drawn when it has a 50:50 decision about
** where to put a pixel.
**************************************************************************/
int Picture::round(float aNumber, float dirn)
{
    if (dirn < 0)
        return ((aNumber - floor(aNumber) <= 0.501) ? (int)floor(aNumber) : (int)ceil(aNumber));
    return ((aNumber - floor(aNumber) < 0.499) ? (int)floor(aNumber) : (int)ceil(aNumber));
}

/**************************************************************************
** drawline
**
** Draws an AGI line.
**************************************************************************/
void Picture::drawline(word x1, word y1, word x2, word y2)
{
    int height, width;
    float x, y, addX, addY;

    height = (y2 - y1);
    width = (x2 - x1);
    addX = (height == 0 ? height : (float)width / abs(height));
    addY = (width == 0 ? width : (float)height / abs(width));

    if (abs(width) > abs(height)) {
        y = y1;
        addX = (width == 0 ? 0 : (width / abs(width)));
        for (x = x1; x != x2; x += addX) {
            pset(round(x, addX), round(y, addY));
            y += addY;
        }
        pset(x2, y2);
    } else {
        x = x1;
        addY = (height == 0 ? 0 : (height / abs(height)));
        for (y = y1; y != y2; y += addY) {
            pset(round(x, addX), round(y, addY));
            x += addX;
        }
        pset(x2, y2);
    }
}

/**************************************************************************
** okToFill
**************************************************************************/
bool Picture::okToFill(byte x, byte y)
{
    if (!picDrawEnabled && !priDrawEnabled)
        return false;
    if (picColour == 15)
        return false;
    if (!priDrawEnabled)
        return (picGetPixel(x, y) == 15);
    if (priDrawEnabled && !picDrawEnabled)
        return (priGetPixel(x, y) == 4);
    return (picGetPixel(x, y) == 15);
}

/**************************************************************************
** agiFill
**************************************************************************/
void Picture::agiFill(word x, word y)
{
    byte x1, y1;
    rpos = spos = 0;

    qstore(x);
    qstore(y);

    for (;;) {

        x1 = qretrieve();
        y1 = qretrieve();

        if ((x1 == EMPTY) || (y1 == EMPTY))
            break;
        else {

            if (okToFill(x1, y1)) {

                pset(x1, y1);

                if (okToFill(x1, y1 - 1) && (y1 != 0)) {
                    qstore(x1);
                    qstore(y1 - 1);
                }
                if (okToFill(x1 - 1, y1) && (x1 != 0)) {
                    qstore(x1 - 1);
                    qstore(y1);
                }
                if (okToFill(x1 + 1, y1) && (x1 != 159)) {
                    qstore(x1 + 1);
                    qstore(y1);
                }
                if (okToFill(x1, y1 + 1) && (y1 != 167)) {
                    qstore(x1);
                    qstore(y1 + 1);
                }
            }
        }
    }
}

/**************************************************************************
** xCorner
**
** Draws an xCorner  (drawing action 0xF5)
**************************************************************************/
void Picture::xCorner(actionListIter *pos)
{
    byte x1, x2, y1, y2;

    x1 = getCode(pos);
    y1 = getCode(pos);

    pset(x1, y1);

    for (;;) {
        x2 = getCode(pos);
        if (x2 >= action_codes_start)
            break;
        drawline(x1, y1, x2, y1);
        x1 = x2;
        y2 = getCode(pos);
        if (y2 >= action_codes_start)
            break;
        drawline(x1, y1, x1, y2);
        y1 = y2;
    }

    if (*pos != picCodes.begin())
        (*pos)--;
}

/**************************************************************************
** yCorner
**
** Draws an yCorner  (drawing action 0xF4)
**************************************************************************/
void Picture::yCorner(actionListIter *pos)
{
    byte x1, x2, y1, y2;

    x1 = getCode(pos);
    y1 = getCode(pos);

    pset(x1, y1);

    for (;;) {
        y2 = getCode(pos);
        if (y2 >= action_codes_start)
            break;
        drawline(x1, y1, x1, y2);
        y1 = y2;
        x2 = getCode(pos);
        if (x2 >= action_codes_start)
            break;
        drawline(x1, y1, x2, y1);
        x1 = x2;
    }

    if (*pos != picCodes.begin())
        (*pos)--;
}

/**************************************************************************
** relativeDraw
**
** Draws short lines relative to last position.  (drawing action 0xF7)
**************************************************************************/
void Picture::relativeDraw(actionListIter *pos)
{
    byte x1, y1, disp;
    char dx, dy;

    x1 = getCode(pos);
    y1 = getCode(pos);

    pset(x1, y1);

    for (;;) {
        disp = getCode(pos);
        if (disp >= action_codes_start)
            break;
        dx = ((disp & 0xF0) >> 4) & 0x0F;
        dy = (disp & 0x0F);
        if (dx & 0x08)
            dx = (-1) * (dx & 0x07);
        if (dy & 0x08)
            dy = (-1) * (dy & 0x07);
        drawline(x1, y1, x1 + dx, y1 + dy);
        x1 += dx;
        y1 += dy;
    }

    if (*pos != picCodes.begin())
        (*pos)--;
}

/**************************************************************************
** fill
**
** AGI flood fill.  (drawing action 0xF8)
**************************************************************************/
void Picture::fill(actionListIter *pos)
{
    byte x1, y1;

    for (;;) {
        if ((x1 = getCode(pos)) >= action_codes_start)
            break;
        if ((y1 = getCode(pos)) >= action_codes_start)
            break;
        agiFill(x1, y1);
    }

    if (*pos != picCodes.end())
        (*pos)--;
}

/**************************************************************************
** absoluteLine
**
** Draws long lines to actual locations (cf. relative) (drawing action 0xF6)
**************************************************************************/
void Picture::absoluteLine(actionListIter *pos)
{
    byte x1, y1, x2, y2;

    x1 = getCode(pos);
    y1 = getCode(pos);

    pset(x1, y1);

    for (;;) {
        if ((x2 = getCode(pos)) >= action_codes_start)
            break;
        if ((y2 = getCode(pos)) >= action_codes_start)
            break;
        drawline(x1, y1, x2, y2);
        x1 = x2;
        y1 = y2;
    }

    if (*pos != picCodes.begin())
        (*pos)--;
}


#define plotPatternPoint() \
   if (patCode & 0x20) { \
      if ((splatterMap[bitPos>>3] >> (7-(bitPos&7))) & 1) pset(x1, y1); \
      bitPos++; \
      if (bitPos == 0xff) bitPos=0; \
   } else pset(x1, y1)

/**************************************************************************
** plotPattern
**
** Draws pixels, circles, squares, or splatter brush patterns depending
** on the pattern code.
**************************************************************************/
void Picture::plotPattern(byte x, byte y)
{
    static byte circles[][15] = { /* agi circle bitmaps */
        {0x80},
        {0xfc},
        {0x5f, 0xf4},
        {0x66, 0xff, 0xf6, 0x60},
        {0x23, 0xbf, 0xff, 0xff, 0xee, 0x20},
        {0x31, 0xe7, 0x9e, 0xff, 0xff, 0xde, 0x79, 0xe3, 0x00},
        {0x38, 0xf9, 0xf3, 0xef, 0xff, 0xff, 0xff, 0xfe, 0xf9, 0xf3, 0xe3, 0x80},
        {0x18, 0x3c, 0x7e, 0x7e, 0x7e, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7e, 0x7e, 0x7e, 0x3c, 0x18}
    };

    static byte splatterMap[32] = { /* splatter brush bitmaps */
        0x20, 0x94, 0x02, 0x24, 0x90, 0x82, 0xa4, 0xa2,
        0x82, 0x09, 0x0a, 0x22, 0x12, 0x10, 0x42, 0x14,
        0x91, 0x4a, 0x91, 0x11, 0x08, 0x12, 0x25, 0x10,
        0x22, 0xa8, 0x14, 0x24, 0x00, 0x50, 0x24, 0x04
    };

    static byte splatterStart[128] = { /* starting bit position */
        0x00, 0x18, 0x30, 0xc4, 0xdc, 0x65, 0xeb, 0x48,
        0x60, 0xbd, 0x89, 0x05, 0x0a, 0xf4, 0x7d, 0x7d,
        0x85, 0xb0, 0x8e, 0x95, 0x1f, 0x22, 0x0d, 0xdf,
        0x2a, 0x78, 0xd5, 0x73, 0x1c, 0xb4, 0x40, 0xa1,
        0xb9, 0x3c, 0xca, 0x58, 0x92, 0x34, 0xcc, 0xce,
        0xd7, 0x42, 0x90, 0x0f, 0x8b, 0x7f, 0x32, 0xed,
        0x5c, 0x9d, 0xc8, 0x99, 0xad, 0x4e, 0x56, 0xa6,
        0xf7, 0x68, 0xb7, 0x25, 0x82, 0x37, 0x3a, 0x51,
        0x69, 0x26, 0x38, 0x52, 0x9e, 0x9a, 0x4f, 0xa7,
        0x43, 0x10, 0x80, 0xee, 0x3d, 0x59, 0x35, 0xcf,
        0x79, 0x74, 0xb5, 0xa2, 0xb1, 0x96, 0x23, 0xe0,
        0xbe, 0x05, 0xf5, 0x6e, 0x19, 0xc5, 0x66, 0x49,
        0xf0, 0xd1, 0x54, 0xa9, 0x70, 0x4b, 0xa4, 0xe2,
        0xe6, 0xe5, 0xab, 0xe4, 0xd2, 0xaa, 0x4c, 0xe3,
        0x06, 0x6f, 0xc6, 0x4a, 0xa4, 0x75, 0x97, 0xe1
    };

    int circlePos = 0;
    byte x1, y1, penSize, bitPos = splatterStart[patNum];

    penSize = (patCode & 7);

    if (x < ((penSize / 2) + 1))
        x = ((penSize / 2) + 1);
    else if (x > 160 - ((penSize / 2) + 1))
        x = 160 - ((penSize / 2) + 1);
    if (y < penSize)
        y = penSize;
    else if (y >= 168 - penSize)
        y = 167 - penSize;

    for (y1 = y - penSize; y1 <= y + penSize; y1++) {
        for (x1 = x - ((int)ceil((float)penSize / 2)); x1 <= x + ((int)floor((float)penSize / 2)); x1++) {
            if (patCode & 0x10)   /* Square */
                plotPatternPoint();
            else { /* Circle */
                if ((circles[patCode & 7][circlePos >> 3] >> (7 - (circlePos & 7))) & 1) {
                    plotPatternPoint();
                }
                circlePos++;
            }
        }
    }

}


/**************************************************************************
** plotBrush
**
** Plots points and various brush patterns.
**************************************************************************/
void Picture::plotBrush(actionListIter *pos)
{
    byte x1, y1;

    for (;;) {
        if (patCode & 0x20) {
            if ((patNum = getCode(pos)) >= action_codes_start)
                break;
            patNum = (patNum >> 1 & 0x7f);
        }
        if ((x1 = getCode(pos)) >= action_codes_start)
            break;
        if ((y1 = getCode(pos)) >= action_codes_start)
            break;
        plotPattern(x1, y1);
    }

    if (*pos != picCodes.begin())
        (*pos)--;
}

//********************************************************************
void Picture::load(byte *picdata, int picsize)
{
    byte nodeData;

    picCodes.clear();

    do {
        nodeData = *picdata++;
        picsize--;
        if (nodeData != DrawEnd)
            picCodes.emplace_back(nodeData);
        else
            break;

    } while (picsize);
}

//*************************************************
int Picture::open(const std::string &filename)
{
    std::ifstream pic_stream(filename, std::ios::binary);

    if (!pic_stream.is_open()) {
        menu->errmes("Can't open file '%s'!", filename.c_str());
        return 1;
    }

    ResourceData.Size = std::filesystem::file_size(filename);
    pic_stream.read(reinterpret_cast<char *>(ResourceData.Data), ResourceData.Size);
    pic_stream.close();

    load(ResourceData.Data, ResourceData.Size);
    refill_pic = refill_pri = false;
    draw();
    init();
    return 0;
}

//*************************************************
int Picture::open(int ResNum)
{
    int err = game->ReadResource(PICTURE, ResNum);
    if (!err) {
        load(ResourceData.Data, ResourceData.Size);
        refill_pic = refill_pri = false;
        draw();
        init();
    }
    return err;
}

//*************************************************
int Picture::save(int ResNum)
{
    save();
    return (game->AddResource(PICTURE, ResNum));
}

//*************************************************
int Picture::save(const std::string &filename)
{
    std::ofstream pic_stream(filename, std::ios::binary | std::ios::trunc);

    if (!pic_stream.is_open()) {
        menu->errmes("Can't open file '%s'!", filename.c_str());
        return 1;
    }
    save();
    pic_stream.write(reinterpret_cast<char *>(ResourceData.Data), ResourceData.Size);
    pic_stream.close();
    return 0;
}

//*************************************************
void Picture::save()
{
    byte *ptr = ResourceData.Data;

    if (picCodes.empty()) {   /* Black picture */
        *ptr = action_codes_end; /* End of picture marker */
        ResourceData.Size = 1;
        return;
    }

    for (const auto &picCode : picCodes)
        *ptr++ = picCode;

    *ptr++ = action_codes_end; /* End of picture marker */
    ResourceData.Size = (int)(ptr - ResourceData.Data);
}

//*************************************************
void Picture::refill(actionListIter pos_fill_start, actionListIter pos_fill_end, int refmode)
{
    actionListIter pos, picPos0, temp_pic, temp_pri;
    int col_pic_orig, col_pri_orig, col_pic_new, col_pri_new;
    bool picDrawEnabled_orig, priDrawEnabled_orig;
    bool draw_pic_orig, draw_pri_orig, draw_pic_new, draw_pri_new;

    picDrawEnabled_orig = priDrawEnabled_orig = false;
    col_pic_orig = col_pri_orig = -1;
    pos = pos_fill_start;
    draw_pic_orig = draw_pri_orig = false;

    do {
        if (pos == picCodes.begin())
            break;
        pos--;
        switch (*pos) {
            case SetPicColor:
                if (col_pic_orig == -1) {
                    col_pic_orig = *std::next(pos);
                    picDrawEnabled_orig = true;
                    temp_pic = pos;
                }
                break;
            case EnablePriority:
                if (col_pic_orig == -1) {
                    col_pic_orig = -2;
                    temp_pic = pos;
                }
                break;
            case SetPriColor:
                if (col_pri_orig == -1) {
                    col_pri_orig = *std::next(pos);
                    priDrawEnabled_orig = true;
                    temp_pri = pos;
                }
                break;
            case EnableVisual:
                if (col_pri_orig == -1) {
                    col_pri_orig = -2;
                    temp_pri = pos;
                }
                break;
            default:
                if (*pos >= YCorner && *pos < DrawEnd) {
                    if (col_pic_orig == -1)
                        draw_pic_orig = true;
                    if (col_pri_orig == -1)
                        draw_pri_orig = true;
                }
                break;
        }
    } while (col_pic_orig == -1 || col_pri_orig == -1); //find the fill original color

    picPos0 = picPos;
    picPos = pos_fill_start;

    if ((refmode | 1) && picDrawEnabled0) {
        if (picDrawEnabled_orig) {
            if (col_pic != col_pic_orig) {
                if (col_pic == 15) {
                    if (draw_pic_orig)
                        addCode(EnablePriority);
                    else {
                        *temp_pic = EnablePriority;
                        picPos = std::next(temp_pic);
                        dldelete();
                    }
                } else { //col_pic != 15
                    if (draw_pic_orig) {
                        addCode(SetPicColor);
                        addCode(col_pic);
                    } else
                        *std::next(temp_pic) = col_pic;
                }
            }
        } else { //!picDrawEnabled_orig
            if (col_pic != 15) {
                addCode(SetPicColor);
                addCode(col_pic);
            }
        }
    }

    if ((refmode | 2) && priDrawEnabled0) {
        if (priDrawEnabled_orig) {
            if (col_pri != col_pri_orig) {
                if (col_pri == 4) {
                    if (draw_pri_orig)
                        addCode(EnableVisual);
                    else {
                        *temp_pri = EnableVisual;
                        picPos = std::next(temp_pri);
                        dldelete();
                    }
                } else { //col_pri != 4
                    if (draw_pri_orig) {
                        addCode(SetPriColor);
                        addCode(col_pri);
                    } else
                        *std::next(temp_pri) = col_pri;
                }
            }
        } else { //!priDrawEnabled_orig
            if (col_pri != 4) {
                addCode(SetPriColor);
                addCode(col_pri);
            }
        }
    }

    pos = pos_fill_end;
    col_pic_new = col_pri_new = -1;
    draw_pic_new = draw_pri_new = false;
    if (pos != picCodes.end()) {
        do {
            switch (*pos) {
                case SetPicColor:
                    col_pic_new = *std::next(pos);
                    break;
                case EnablePriority:
                    col_pic_new = -2;
                    break;
                case SetPriColor:
                    col_pri_new = *std::next(pos);
                    break;
                case EnableVisual:
                    col_pri_new = -2;
                    break;
                default:
                    if (*pos >= YCorner && *pos < DrawEnd) {
                        if (col_pic_new == -1)
                            draw_pic_new = true;
                        if (col_pri_new == -1)
                            draw_pri_new = true;
                    }
                    break;
            }
            pos++;
        } while ((pos != picCodes.end()) && (col_pic_orig == -1 || col_pri_orig == -1));


        picPos = pos_fill_end;
        if ((refmode | 1) && picDrawEnabled0) {
            if (col_pic != col_pic_orig) {
                if (draw_pic_new) {
                    if (picDrawEnabled_orig) {
                        addCode(SetPicColor);
                        addCode(col_pic_orig);
                    } else {
                        if (col_pic != 0x15)
                            addCode(EnablePriority);
                    }
                }
            }
        }

        if ((refmode | 2) && priDrawEnabled0) {
            if (col_pri != col_pri_orig) {
                if (draw_pri_new) {
                    if (priDrawEnabled_orig) {
                        addCode(SetPriColor);
                        addCode(col_pri_orig);
                    } else {
                        if (col_pri != 0x4)
                            addCode(EnableVisual);
                    }
                }
            }
        }

    }
    picPos = picPos0;
    draw();
}

//*************************************************
void Picture::draw()
{
    byte action;
    actionListIter pos, pos_fill_start, pos_fill_end;
    int refmode;
    bool finishedPic = false;
    int pC, pN;

    memset(picture, 15, MAX_W * MAX_H); /* Visual screen default, white */
    memset(priority, 4, MAX_W * MAX_H); /* Priority screen default, red */
    rpos = QUMAX;
    spos = 0;
    picDrawEnabled = false;
    priDrawEnabled = false;
    picColour = priColour = 0;
    tool = -1;

    if (!picCodes.empty()) {

        pC = patCode;
        pN = patNum;
        patCode = patNum = 0;

        pos = picCodes.begin();

        if (picPos != picCodes.begin())
            do {
                action = getCode(&pos);

                switch (action) {

                    case SetPicColor:
                        picColour = getCode(&pos);
                        picDrawEnabled = true;
                        break;
                    case EnablePriority:
                        picDrawEnabled = false;
                        break;
                    case SetPriColor:
                        priColour = getCode(&pos);
                        priDrawEnabled = true;
                        break;
                    case EnableVisual:
                        priDrawEnabled = false;
                        break;
                    case YCorner:
                        tool = T_STEP;
                        yCorner(&pos);
                        break;
                    case XCorner:
                        tool = T_STEP;
                        xCorner(&pos);
                        break;
                    case AbsoluteLine:
                        tool = T_LINE;
                        absoluteLine(&pos);
                        break;
                    case RelativeLine:
                        tool = T_PEN;
                        relativeDraw(&pos);
                        break;
                    case Fill:
                        tool = T_FILL;
                        pos_fill_start = std::prev(pos);
                        fill(&pos);
                        pos_fill_end = pos;
                        if (refill_pic || refill_pri) {
                            //find which FILL filled the selected area
                            refmode = 0;
                            if (refill_pic && picGetPixel(refill_x, refill_y) != 15) {
                                refmode |= 1;
                                refill_pic = false;
                            }
                            if (refill_pri && priGetPixel(refill_x, refill_y) != 4) {
                                refill_pri = false;
                                refmode |= 2;
                            }
                            if (refmode)
                                refill(pos_fill_start, pos_fill_end, refmode);
                            if (!refill_pic && !refill_pri)
                                return;
                        }
                        break;
                    case SetPattern:
                        patCode = getCode(&pos);
                        break;
                    case Brush:
                        tool = T_BRUSH;
                        plotBrush(&pos);
                        break;
                    case DrawEnd:
                        finishedPic = true;
                        break;
                    default:
                        printf("Unknown picture code : %X", action);
                        break;
                }

            } while ((pos != picPos) && !finishedPic);

        patCode = pC;
        patNum = pN;
    }
}

//**************************************************

void Picture::dldelete()
{
    // Remove the node currently pointed to by picPos.

    if (picPos == picCodes.end())
        return;
    picPos = picCodes.erase(picPos);
}

void Picture::removeAction()
{
    // Remove all nodes up to, but not including, the next action.

    if (picPos != picCodes.end()) {
        dldelete();
        while ((picPos != picCodes.end()) && (*picPos < action_codes_start))
            dldelete();
    }
}

void Picture::wipeAction()
{
    // Remove all nodes up to the end of the list.

    if (picPos != picCodes.end()) {
        dldelete();
        while (picPos != picCodes.end())
            dldelete();
    }
}

void Picture::moveBackAction()
{
    if (picCodes.empty() || (picPos == picCodes.begin()))
        return;

    // Back up until we find the previous Action code.
    do {
        if (picPos != picCodes.begin())
            picPos--;
        else
            break;
    } while (*picPos < action_codes_start);
}

void Picture::moveForwardAction()
{
    if (picCodes.empty() || (picPos == picCodes.end()))
        return;

    // Look ahead until we find the next Action code.
    auto searchPos = std::next(picPos);
    while (searchPos != picCodes.end()) {
        if (*searchPos >= action_codes_start)
            break;
        searchPos++;
    }
    picPos = searchPos;
}

//**************************************************
void Picture::init()
{
    clear_tools();

    priDrawEnabled = picDrawEnabled = false;
    curp = &points0;
    points0.n = points1.n = 0;
    newp = &points;
    add_pic = add_pri = false;
}

//**************************************************
void Picture::newpic()
{
    picCodes.clear();
    picPos = picCodes.begin();
    draw();
    init();
}

//**************************************************
void Picture::addCode(byte code)
{
    picCodes.insert(picPos, code);
}

//**************************************************
void Picture::replaceCode(byte code)
{
    picCodes.insert(picPos, code);
    picPos = picCodes.erase(picPos);
}

//**************************************************
void Picture::addPatCode()
{
    int pat = 0;

    pat = (brushSize & 0x07);
    if (brushShape == SQUARE)
        pat |= 0x10;
    if (brushTexture == SPRAY)
        pat |= 0x20;

    addCode(pat);
}

//**************************************************
void Picture::adjustDisp(int *dX, int *dY)
{
    // Clamp values for pen displacement
    //
    // Due to the encoding format, dX cannot be <=
    // -7 as it would result in a byte >= 0xF0,
    // which signals a new action code.
    *dX = std::clamp(*dX, -6, 7);
    *dY = std::clamp(*dY, -7, 7);
}

//**************************************************
void Picture::clear_tools()
{
    numClicks = 0;
    stepClicks = 0;
    firstClick = true;
    tool = -1;
}

//**************************************************
void Picture::init_tool()
{
    numClicks = 0;
    stepClicks = 0;
    firstClick = true;
}

//**************************************************
void Picture::home_proc()
{
    picPos = picCodes.begin();
    draw();
    init_tool();
}

//**************************************************
void Picture::left_proc()
{
    moveBackAction();
    draw();
    init_tool();
}

//**************************************************
void Picture::right_proc()
{
    moveForwardAction();
    draw();
    init_tool();
}

//**************************************************
void Picture::end_proc()
{
    picPos = picCodes.end();
    draw();
    init_tool();
}

//**************************************************
void Picture::del_proc()
{
    removeAction();
    draw();
    init_tool();
}

//**************************************************
void Picture::wipe_proc()
{
    wipeAction();
    draw();
    init_tool();
}

//**************************************************
void Picture::tool_proc(int k)
{
    tool = k;
    numClicks = 0;
    stepClicks = 0;
    firstClick = true;
}

//**************************************************
void Picture::set_brush(int mode, int val)
{
    switch (mode) {
        case 0:
            brushSize = val;
            break;
        case 1:
            brushShape = val;
            break;
        case 2:
            brushTexture = val;
            break;
    }
    numClicks = 0;
}

//**************************************************
void Picture::choose_color(int button, int color)
{
    if (button == M_LEFT) {
        add_pic = true;
        if (color == -1) { //off
            code_pic = EnablePriority;
            picDrawEnabled = false;
        } else {
            picColour = color;
            code_pic = SetPicColor;
            col_pic = picColour;
            picDrawEnabled = true;
        }
    } else {
        add_pri = true;
        if (color == -1) { //off
            code_pri = EnableVisual;
            priDrawEnabled = false;
        } else {
            priColour = color;
            code_pri = SetPriColor;
            col_pri = priColour;
            priDrawEnabled = true;
        }
    }
    init_tool();
    curcol = (drawing_mode) ? priColour : picColour;
}

//**************************************************
int Picture::move_action(int newX, int newY)
{
    int ret = 0; //1 - draw "temporary" line on screen

    switch (tool) {
        case T_LINE:
            if (numClicks == 0)
                break;
            normline2(clickX >> 1, clickY, newX >> 1, newY);
            ret = 1;
            break;
        case T_PEN:
            if (numClicks == 0)
                break;
            dX = ((newX >> 1) - (clickX >> 1));
            dY = ((newY) - (clickY));
            adjustDisp(&dX, &dY);
            normline2(clickX >> 1, clickY, (clickX >> 1) + dX, clickY + dY);
            ret = 1;
            break;
        case T_STEP:
            if (stepClicks == 0)
                break;
            switch (stepClicks) {
                case 1:
                    dX = ((newX >> 1) - (clickX >> 1));
                    dY = ((newY) - (clickY));
                    if (abs(dX) > abs(dY))
                        dY = 0;
                    else
                        dX = 0;
                    normline2(clickX >> 1, clickY, (clickX >> 1) + dX, clickY + dY);
                    ret = 1;
                    break;

                default:
                    dX = ((newX >> 1) - (clickX >> 1));
                    dY = ((newY) - (clickY));
                    if (stepClicks % 2)
                        dX = 0;
                    else
                        dY = 0;
                    normline2(clickX >> 1, clickY, (clickX >> 1) + dX, clickY + dY);
                    ret = 1;
                    break;
            }
            break;
    }

    return ret;
}

//**************************************************
int Picture::button_action(int newX, int newY)
{
    int disp;
    int ret = 0;

    if (!(tool == T_FILL && !okToFill(newX >> 1, newY))) {
        if (add_pic) {
            addCode(code_pic);
            if (code_pic == SetPicColor)
                addCode(col_pic);
            add_pic = false;
        }
        if (add_pri) {
            addCode(code_pri);
            if (code_pri == SetPriColor)
                addCode(col_pri);
            add_pri = false;
        }
    }

    switch (tool) {
        case T_LINE:
            switch (numClicks++) {
                case 0:
                    break;
                case 1:
                    addCode(AbsoluteLine);
                    addCode(clickX >> 1);
                    addCode(clickY);
                    ret = 1;
                    [[fallthrough]];
                default:
                    addCode(newX >> 1);
                    addCode(newY);
                    ret = 1;
                    drawline(clickX >> 1, clickY, newX >> 1, newY);
                    break;
            }
            clickX = newX;
            clickY = newY;
            curp = &points0;
            points0.n = points1.n = 0;
            break;
        case T_PEN:
            dX = ((newX >> 1) - (clickX >> 1));
            dY = ((newY) - (clickY));
            adjustDisp(&dX, &dY);
            switch (numClicks++) {
                case 0:
                    clickX = newX;
                    clickY = newY;
                    break;
                case 1:
                    addCode(RelativeLine);
                    addCode(clickX >> 1);
                    addCode(clickY);
                    [[fallthrough]];
                default:
                    if (dX < 0)
                        disp = (0x80 | ((((-1) * dX) - 0) << 4));
                    else
                        disp = (dX << 4);
                    if (dY < 0)
                        disp |= (0x08 | (((-1) * dY) - 0));
                    else
                        disp |= dY;
                    addCode(disp);
                    ret = 1;
                    drawline(clickX >> 1, clickY,
                             (clickX >> 1) + dX, (clickY) + dY);
                    clickX = clickX + (dX << 1);
                    clickY = clickY + dY;
                    break;
            }
            curp = &points0;
            points0.n = points1.n = 0;
            break;
        case T_STEP:
            switch (stepClicks) {
                case 0:
                    clickX = newX;
                    clickY = newY;
                    break;

                case 1:
                    dX = ((newX >> 1) - (clickX >> 1));
                    dY = ((newY) - (clickY));
                    if (abs(dX) > abs(dY)) {   /* X or Y corner */
                        dY = 0;
                        stepClicks++;
                        addCode(XCorner);
                        addCode(clickX >> 1);
                        addCode(clickY);
                        addCode((clickX >> 1) + dX);
                    } else {
                        dX = 0;
                        addCode(YCorner);
                        addCode(clickX >> 1);
                        addCode(clickY);
                        addCode((clickY) + dY);
                    }
                    ret = 1;
                    drawline(clickX >> 1, clickY,
                             (clickX >> 1) + dX, (clickY) + dY);
                    clickX = clickX + (dX << 1);
                    clickY = clickY + dY;
                    break;

                default:
                    dX = ((newX >> 1) - (clickX >> 1));
                    dY = ((newY) - (clickY));
                    if (stepClicks % 2) {
                        dX = 0;
                        addCode((clickY) + dY);
                    } else {
                        dY = 0;
                        addCode((clickX >> 1) + dX);
                    }
                    ret = 1;
                    drawline(clickX >> 1, clickY,
                             (clickX >> 1) + dX, (clickY) + dY);
                    clickX = clickX + (dX << 1);
                    clickY = clickY + dY;
                    break;
            }
            stepClicks++;
            curp = &points0;
            points0.n = points1.n = 0;
            break;
        case T_FILL:
            if (!(okToFill(newX >> 1, newY))) {
                status(0);
                refill_x = newX >> 1;
                refill_y = newY;
                refill_pic = refill_pri = true;
                draw();
                ret = 1;
                init();
                status(1);
                tool = T_FILL;
                init_tool();
                break;
            }

            agiFill(newX >> 1, newY);
            if (firstClick) {
                addCode(Fill);
                firstClick = false;
            }
            addCode(newX >> 1);
            addCode(newY);
            ret = 1;
            break;
        case T_BRUSH:
            if (numClicks == 0) {
                addCode(SetPattern);
                addPatCode();
                addCode(Brush);
            }
            numClicks++;

            patCode = brushSize;
            if (brushShape == SQUARE)
                patCode |= 0x10;
            if (brushTexture == SPRAY)
                patCode |= 0x20;
            patNum = ((rand() % 0xEE) >> 1) & 0x7F;
            plotPattern(newX >> 1, newY);
            if (brushTexture == SPRAY)
                addCode(patNum << 1);
            addCode(newX >> 1);
            addCode(newY);
            ret = 1;
    }

    return ret;
}

//***************************************************
void Picture::set_mode(int mode)
{
    drawing_mode = mode;
    if (mode == 0) {
        pptr = picture;
        curcol = picColour;
    } else {
        pptr = priority;
        curcol = priColour;
    }
}

//***************************************************
void Picture::putpix2(int x, int y)
{
    byte c;
    QColor cc;

    if (x < 0 || y < 0 || x >= MAX_W || y >= MAX_HH)
        return;

    //save the pixels under the line to be drawn
    //so it can be restored when the line is moved
    if (bg_on) { //if background is on - must save the contents of the background image
        x <<= 1;
        curp->p[curp->n].x = x;
        curp->p[curp->n].y = y;
        c = pptr[y * MAX_W + x];
        if ((c == 15 && pptr == picture) || (c == 4 && pptr == priority)) {
            curp->p[curp->n].cc = QColor(bgpix->pixel(x, y));
            curp->n++;
            curp->p[curp->n].x = x + 1;
            curp->p[curp->n].y = y;
            curp->p[curp->n].cc = QColor(bgpix->pixel(x + 1, y));
            curp->n++;
        } else {
            cc = egacolor[c];
            curp->p[curp->n].cc = cc;
            curp->n++;
            curp->p[curp->n].x = x + 1;
            curp->p[curp->n].y = y;
            curp->p[curp->n].cc = cc;
            curp->n++;
        }
        x >>= 1;
    } else { //save the pixels
        curp->p[curp->n].x = x;
        curp->p[curp->n].y = y;
        curp->p[curp->n].c = pptr[y * MAX_W + (x << 1)];
        curp->n++;
    }
    newp->p[newp->n].x = x;
    newp->p[newp->n].y = y;
    newp->p[newp->n].c = curcol;
    newp->n++;
}

//***************************************************
void Picture::normline2(int x1, int y1, int x2, int y2)
{
    int height, width;
    float x, y, addX, addY;

    height = (y2 - y1);
    width = (x2 - x1);
    addX = (height == 0 ? height : (float)width / abs(height));
    addY = (width == 0 ? width : (float)height / abs(width));

    points.n = 0;
    curp->n = 0;

    if (abs(width) > abs(height)) {
        y = y1;
        addX = (width == 0 ? 0 : (width / abs(width)));
        for (x = x1; x != x2; x += addX) {
            putpix2(round(x, addX), round(y, addY));
            y += addY;
        }
        putpix2(x2, y2);
    } else {
        x = x1;
        addY = (height == 0 ? 0 : (height / abs(height)));
        for (y = y1; y != y2; y += addY) {
            putpix2(round(x, addX), round(y, addY));
            x += addX;
        }
        putpix2(x2, y2);
    }

    if (curp == &points0)
        curp = (&points1);
    else
        curp = (&points0);
}

//***************************************************
void Picture::viewData(QStringList *data)
{
    QString line, temp;

    data->clear();

    if (picCodes.empty()) {   /* Black picture */
        data->append("ff");
        return;
    }

    for (auto const &node : picCodes) {
        if (node >= action_codes_start && !line.isEmpty()) {
            data->append(line);
            line.clear();
            line = QString("%1 ").arg(QString::number(node, 16), 2, QChar('0'));
        } else {
            temp = QString("%1 ").arg(QString::number(node, 16), 2, QChar('0'));
            line += temp;
        }
    }

    if (!temp.isEmpty()) {
        line += temp;
        data->append(line);
    }
    data->append("ff");
}

//***************************************************
void Picture::status(int mode)
{
    if (mode == 0) {
        picDrawEnabled0 = picDrawEnabled;
        priDrawEnabled0 = priDrawEnabled;
        picColour0 = picColour;
        priColour0 = priColour;
    } else {
        picDrawEnabled = picDrawEnabled0;
        priDrawEnabled = priDrawEnabled0;
        picColour = picColour0;
        priColour = priColour0;
    }
}
