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

#ifndef BESPIN_DRAW_H
#define BESPIN_DRAW_H

#include <qmath.h>
#include <QPainter>

#include "blib/colors.h"
#include "bespin.h"
#include "makros.h"

using namespace Bespin;

#if 0
Style::Masks Style::masks;
Style::Shadows Style::shadows;
Style::Lights Style::lights;
Config Style::config;
Dpi Style::dpi;
#endif

#define OPT_SUNKEN bool sunken = option->state & State_Sunken;
#define OPT_ENABLED bool isEnabled = option->state & State_Enabled;
#define OPT_HOVER bool hover = (option->state & State_Enabled) && (option->state & State_MouseOver);
#define OPT_FOCUS bool hasFocus = option->state & State_HasFocus;

#define B_STATES OPT_SUNKEN OPT_ENABLED OPT_HOVER OPT_FOCUS

#define BESPIN_MNEMONIC config.mnemonic

static inline void
setBold(QPainter *p, const QString &text = QString(), int w = -1)
{
    if (p->font().pointSize() < 1)  // ignore pixelsize fonts for reports of bad visual quality (#2787384)
        return;

    QFont fnt = p->font();
    fnt.setBold(true);
    if (!text.isEmpty())
    {
        int wb = QFontMetrics(fnt).size(Qt::TextShowMnemonic, text).width();
        if (wb > w)
        {
            int wd = qMax(w-F(4), QFontMetrics(p->font()).size(Qt::TextShowMnemonic, text).width());
            fnt.setStretch(lround(wd*100.0/wb));
        }
    }
    p->setFont(fnt);
}

static inline void
setTitleFont(QPainter *p, const QString &text = QString(), int w = -1 )
{
    if (p->font().pointSize() < 1) // ignore pixelsize fonts for reports of bad visual quality (#2787384)
        return;

    QFont fnt = p->font();
    fnt.setWeight(QFont::Black);
//         fnt.setUnderline(true);
//         fnt.setStretch(QFont::SemiExpanded);
#if QT_VERSION >= 0x040400
    fnt.setCapitalization(QFont::AllUppercase/*SmallCaps*/);
#else
    title = title.toUpper();
#endif
    if (text.isEmpty())
        fnt.setPointSize(9*fnt.pointSize()/10);
    else
    {
        int wb = QFontMetrics(fnt).size(Qt::TextShowMnemonic, text).width();
        if (wb > w)
        {
            int wd = qMax(w-F(4), QFontMetrics(p->font()).size(Qt::TextShowMnemonic, text).width());
            fnt.setStretch(lround(wd*100.0/wb));
        }
    }

    p->setFont(fnt);
}

#endif // BESPIN_DRAW_H
