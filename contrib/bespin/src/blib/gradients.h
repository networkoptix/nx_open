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

#ifndef GRADIENTS_H
#define GRADIENTS_H

#include <QBrush>
#include <QColor>
#include <QPixmap>

#ifndef Q_WS_X11
#define QT_NO_XRENDER #
#endif

namespace Bespin {

class BLIB_EXPORT  BgSet {
public:
    BgSet()
    {
        clients = 0;
    }
    QPixmap topTile, btmTile;
    QPixmap cornerTile, lCorner, rCorner;
    /** the amount of clients attached to this set - use this if you don't want the auto cache */
    uint clients; // used by the deco
};

namespace Gradients {

enum Type {
   None = 0, Simple, Button, Sunken, Gloss, Glass, Metal, Cloudy, Shiny, //RadialGloss,
   TypeAmount
};

// static const char *string{ "None", "Simple", "Button", "Sunken", "Gloss", "Glass", "Metal", "Cloudy", "Shiny" };

enum BgMode { BevelV = 2, BevelH };

enum Position { Top = 0, Bottom, Left, Right };


/** use only if sure you're not requesting Type::None */
BLIB_EXPORT const QPixmap& pix(const QColor &c, int size, Qt::Orientation o, Type type = Simple);

/** wrapper to support Type::None */
BLIB_EXPORT inline QBrush brush(const QColor &c, int size, Qt::Orientation o, Type type  = Simple)
{
    if (type == None)
        return QBrush(c);
    return QBrush(pix(c, size, o, type));
}

BLIB_EXPORT inline bool isReflective(Type type = Simple)
{
    return type == Button || type == Metal || type == Shiny;
}

BLIB_EXPORT inline bool isTranslucent(Type type = Simple)
{
    return type > Sunken && type != Metal && type != Shiny;
}

BLIB_EXPORT QColor endColor(const QColor &c, Position p, Type type = Simple, bool checkValue = false);

/** a diagonal NW -> SE light */
BLIB_EXPORT const QPixmap &shadow(int height, bool bottom = false);

/** a diagonal 16:9 SE -> NW light */
BLIB_EXPORT const QPixmap &ambient(int height);

/** a horizontal black bevel from low alpha to transparent */
BLIB_EXPORT const QPixmap &bevel(bool ltr = true);

/** a vertical N -> S light */
BLIB_EXPORT const QPixmap &light(int height);

BLIB_EXPORT const QPixmap &structure(const QColor &c, bool light = false);

/** pulls a background pixmap set out of a (limited) cache - creates it if necessary */
BLIB_EXPORT const BgSet &bgSet(const QColor &c);

/** create a bgset on the heap, utilized by the caching variant - The pointer is YOUR response!! */
BLIB_EXPORT BgSet *bgSet(const QColor &c, BgMode mode, int bgBevelIntesity = 110);
// const QPixmap &bgCorner(const QColor &c, bool other = false);

BLIB_EXPORT void init(BgMode mode = BevelV, int structure = 0,
                      int bgBevelIntesity = 110, int btnBevelSize = 16,
                      bool force = false, bool invertedGroups = false);

BLIB_EXPORT const QPixmap &borderline(const QColor &c, Position pos);
BLIB_EXPORT void wipe();

}
}

#endif //GRADIENTS_H
