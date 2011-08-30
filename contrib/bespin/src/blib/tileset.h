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

#ifndef TILESET_H
#define TILESET_H

#include <QBitmap>
#include <QRect>
#include <QHash>

#ifdef Q_WS_X11
#include <QX11Info>
#include <X11/Xlib.h>

#ifndef QT_NO_XRENDER
#include <X11/extensions/Xrender.h>
#endif

#include "fixx11h.h"
#else

#define QT_NO_XRENDER #

#endif

#include "gradients.h"

namespace Tile
{

   enum Section
   { // DON'T CHANGE THE ORDER FOR NO REASON, i misuse them on the masks...
      TopLeft = 0, TopRight, BtmLeft, BtmRight,
      TopMid, BtmMid, MidLeft, MidMid, MidRight
   };
   enum Position
   {
      Top = 0x1, Left=0x2, Bottom=0x4, Right=0x8,
      Ring=0xf, Center=0x10, Full=0x1f
   };

   typedef uint PosFlags;

inline static bool matches(PosFlags This, PosFlags That){return (This & That) == This;}

class BLIB_EXPORT Set
{
public:
    Set(const QImage &img, int xOff, int yOff, int width, int height, int round = 99);
    Set()
    {
        setDefaultShape(Ring);
        _hasCorners = false;
    }

    const QPixmap &corner(PosFlags pf) const;

    inline bool
    hasCorners() const {return _hasCorners;}

    inline int
    height(Section sect) const {return pixmap[sect].height();}

    inline bool
    isQBitmap() const {return _isBitmap;}

    void outline(const QRect &rect, QPainter *p, QColor c, int size = 1) const;

    QRect rect(const QRect &rect, PosFlags pf) const;

    void render(const QRect &rect, QPainter *p) const;

    void render(const QRect &rect, QPainter *p, const QColor &c) const;

    void render(const QRect &rect, QPainter *p,
                const QPixmap &pix, const QPoint &offset = QPoint()) const;

    void sharpenEdges();
   
    inline void
    render( const QRect &rect, QPainter *p, Bespin::Gradients::Type type, Qt::Orientation o,
            const QColor &c, int size = -1, const QPoint &offset = QPoint()) const
    {
        if (type == Bespin::Gradients::None)
            render(rect, p, c);
        else
        {
            const int s = (size > 0) ? size : (o == Qt::Vertical) ? rect.height() : rect.width();
            render(rect, p, Bespin::Gradients::pix(c, s, o, type), offset);
        }
    }
   
    inline void
    render(const QRect &rect, QPainter *p, const QBrush &brush, const QPoint &offset = QPoint()) const
    {
        if (brush.style() == Qt::TexturePattern)
            render(rect, p, brush.texture(), offset);
        else
            render(rect, p, brush.color());
    }

    inline int
    width(Section sect) const { return pixmap[sect].width(); }

    inline void
    setDefaultShape(PosFlags pf) { _defShape = pf; }


    inline const QPixmap &
    tile(Section s) const { return pixmap[s]; }

protected:
   QPixmap pixmap[9];
   PosFlags _defShape;
private:
   bool _isBitmap, _hasCorners;
   QRect rndRect;
};

BLIB_EXPORT PosFlags shape();
BLIB_EXPORT void setShape(PosFlags pf);
BLIB_EXPORT void reset();

class BLIB_EXPORT Line
{
public:
   Line(const QPixmap &pix, Qt::Orientation o, int d1, int d2);
   Line(){}
   void render(const QRect &rect, QPainter *p, PosFlags pf = Full, bool btmRight = false) const;
   inline int thickness() const { return _thickness; }
private:
   inline int width(int i) const {return pixmap[i].width();}
   inline int height(int i) const {return pixmap[i].height();}
   Qt::Orientation _o;
   QPixmap pixmap[3];
   int _thickness;
};

} // namespace Tile

#endif //TILESET_H
