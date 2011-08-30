/* Bespin widget style for Qt4
   Copyright (C) 2007 Thomas Luebking <thomas.luebking@web.de>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
 */

#ifndef BESPIN_CONFIG
#define BESPIN_CONFIG

#include "types.h"
#include "blib/gradients.h"

namespace Bespin
{

typedef struct Config
{
    QString appDataPath;

    struct bg
    {
        BGMode mode;
        bool glassy, ringOverlay;
        int structure, intensity, minValue, opacity, blur;
        struct
        {
            bool invert, shape;
        } docks;
        struct
        {
            bool glassy, invert;
            int opacity;
        } modal;
        QPalette::ColorRole tooltip_role[2];
    } bg;

    struct btn
    {
        int layer, minHeight;
        Check::Type checkType;
        bool cushion, fullHover, backLightHover, ambientLight, bevelEnds, round;
        Gradients::Type gradient;
        QPalette::ColorRole std_role[2], active_role[2];
        struct tool {
            int disabledStyle, frame;
            bool connected, separator;
            Gradients::Type gradient;
            QPalette::ColorRole std_role[2], active_role[2];
        } tool;
    } btn;

    struct chooser
    {
        Gradients::Type gradient;
    } chooser;

    int  dialogBtnLayout;
    bool drawSplitters;
    bool fadeInactive;
    int fontExtent;
    int fontOffset[2];
    int groupBoxMode;

    struct input
    {
        ushort pwEchoChar;
    } input;

    struct kwin
    {
        Gradients::Type gradient[2];
        QPalette::ColorRole inactive_role[2], active_role[2], text_role[2];
    } kwin;
    Qt::LayoutDirection leftHanded;

    bool macStyle;

    struct menu
    {
        QPalette::ColorRole std_role[2], active_role[2];
        Gradients::Type itemGradient;
        bool showIcons, shadow, barSunken, boldText, itemSunken, activeItemSunken, glassy, round, roundSelect;
        int opacity, delay;
    } menu;

    int mnemonic;

    struct progress
    {
        Gradients::Type gradient;
        QPalette::ColorRole __role[2];
    } progress;

    int winBtnStyle;

    float scale;

    struct scroll
    {
        Gradients::Type gradient;
        Groove::Mode groove;
        bool showButtons, invertBg, fullHover;
        char sliderWidth;
        QPalette::ColorRole __role[2];
    } scroll;

    float shadowIntensity;
    bool  shadowTitlebar;
    bool  showOff;

    struct tab
    {
        QPalette::ColorRole std_role[2], active_role[2];
        Gradients::Type gradient;
        bool activeTabSunken;
    } tab;

    struct toolbox
    {
        QPalette::ColorRole active_role[2];
        Gradients::Type gradient;
    } toolbox;

    struct UNO
    {
        bool used, sunken, title, toolbar;
        QPalette::ColorRole __role[2];
        Gradients::Type gradient;
    } UNO;

    struct view
    {
        QPalette::ColorRole header_role[2], sortingHeader_role[2];
        Gradients::Type headerGradient, sortingHeaderGradient;
        QPalette::ColorRole shadeRole;
        int shadeLevel;
    } view;

} Config;

} // namespace

#endif // BESPIN_CONFIG
