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

 /**
 erhem... just noticed and in case you should wonder:
 this is NOT derived from the Oxygen style, but rather kinda vice versa.
 so i'm not forgetting credits. period.
 */

#include <QPainter>
#include <qmath.h>
#include "FX.h"
#include "tileset.h"

#include <QtDebug>

using namespace Tile;

// some static elements (benders)
static QPixmap nullPix;
static PosFlags _shape = 0;
static const QPixmap *_texPix = 0;
static const QColor *_texColor = 0;
static const QPoint *_offset = 0;

// static functions
PosFlags Tile::shape() { return _shape; }
void Tile::setShape(PosFlags pf) { _shape = pf; }
void Tile::reset()
{
    _shape = 0;
}

static QPixmap simple(QImage &img)
{
    if (!img.hasAlphaChannel())
        return QPixmap::fromImage(img);

    bool translucent = false, content = false;
    uint *data = ( uint * ) img.bits();
    int total = img.width() * img.height();
    int alpha;
    for (int current = 0 ; current < total ; ++current)
    {
        alpha = qAlpha(data[ current ]);
        if (alpha)
        {
            content = true;
            if (alpha < 255)
            {
                translucent = true;
                break;
            }
        }
    }

    if (!content)
        return QPixmap();

    if (!translucent)
    {
        QPixmap pix(img.size());
        QPainter p(&pix);
        p.drawImage(0,0, img);
        p.end();
        return pix;
    }

    return QPixmap::fromImage(img);
}
#if 0
static QPixmap invertAlpha(const QPixmap & pix)
{
   if (pix.isNull()) return pix;
   QImage img =  pix.toImage();
   QImage *dst = new QImage(img);
   uint *data = ( uint * ) img.bits();
   uint *ddata = ( uint * ) dst->bits();
   int total = img.width() * img.height();
   for ( int c = 0 ; c < total ; ++c )
      ddata[c] = qRgba( qRed(data[c]), qGreen(data[c]), qBlue(data[c]), 255-qAlpha(data[c]) );
   QPixmap ret = QPixmap::fromImage(*dst, 0);
   delete dst;
   return ret;
}
#endif

Set::Set(const QImage &img, int xOff, int yOff, int width, int height, int round)
{
    if (img.isNull())
        { _isBitmap = false; return; }

    _isBitmap = img.depth() == 1;
    int w = qMax(1, width), h = qMax(1, height);

    int i = xOff*2*round/100;
    rndRect = QRect(i, i, i, i);

    int rOff = img.width() - xOff - w;
    int bOff = img.height() - yOff - h;
    int tileWidth = qMax(32, width);
    int tileHeight = qMax(32, height);

    QPainter p;
    QImage hook;
    QPixmap dump;
    QPixmap transSrc(qMax(32, img.width()), qMax(32, img.height()));
    transSrc.fill(Qt::transparent);

#define DUMP(_SECTION_, _WIDTH_, _HEIGHT_, _X_, _Y_, _W_, _H_)\
dump = simple(hook = img.copy(_X_, _Y_, _W_, _H_));\
if (!dump.isNull())\
{\
    if (dump.hasAlphaChannel())\
        pixmap[_SECTION_] = transSrc.copy(0,0,_WIDTH_, _HEIGHT_);\
    else\
        pixmap[_SECTION_] = QPixmap(_WIDTH_, _HEIGHT_);\
    p.begin(&pixmap[_SECTION_]);\
    p.drawTiledPixmap(pixmap[_SECTION_].rect(), dump);\
    p.end();\
} //

    pixmap[TopLeft] = simple(hook = img.copy(0, 0, xOff, yOff));

    DUMP(TopMid,   tileWidth, yOff,   xOff, 0, w, yOff);

    pixmap[TopRight] = simple(hook = img.copy(xOff+w, 0, rOff, yOff));

    //----------------------------------
    DUMP(MidLeft,   xOff, tileHeight,   0, yOff, xOff, h);
    DUMP(MidMid,   tileWidth, tileHeight,   xOff, yOff, w, h);
    DUMP(MidRight,   rOff, tileHeight,   xOff+w, yOff, rOff, h);
    //----------------------------------

    pixmap[BtmLeft] = simple(hook = img.copy(0, yOff+h, xOff, bOff));

    DUMP(BtmMid,   tileWidth, bOff,   xOff, yOff+h, w, bOff);

    pixmap[BtmRight] = simple(hook = img.copy(xOff+w, yOff+h, rOff, bOff));

    _hasCorners = !img.isNull();
    _defShape = Full;
#undef initPixmap
#undef finishPixmap
}

void Set::sharpenEdges()
{
    pixmap[TopMid] = QPixmap(pixmap[TopMid].size()); pixmap[TopMid].fill(Qt::black);
    pixmap[BtmMid] = QPixmap(pixmap[BtmMid].size()); pixmap[BtmMid].fill(Qt::black);
    pixmap[MidLeft] = QPixmap(pixmap[MidLeft].size()); pixmap[MidLeft].fill(Qt::black);
    pixmap[MidRight] = QPixmap(pixmap[MidRight].size()); pixmap[MidRight].fill(Qt::black);
}

QRect
Set::rect(const QRect &rect, PosFlags pf) const
{
    QRect ret = rect;
    switch (pf)
    {
    case Center:
        ret.adjust(width(MidLeft),height(TopMid),-width(TopMid),-height(BtmMid)); break;
    case Left:
        ret.setRight(ret.left()+width(MidLeft)); break;
    case Top:
        ret.setBottom(ret.top()+height(TopMid)); break;
    case Right:
        ret.setLeft(ret.right()-width(MidRight)); break;
    case Bottom:
        ret.setTop(ret.bottom()-height(BtmMid)); break;
    default: break;
    }
    return ret;
}

void
Set::render(const QRect &r, QPainter *p) const
{

//     filledPix.fill(Qt::transparent);

#if 0
#define MAKE_FILL(_OFF_)\
if (!tile->isNull())\
{\
    if ((_texPix || _texColor))\
    {\
        if (filledPix.size() != tile->size())\
            filledPix = QPixmap(tile->size());\
        if (_texPix)\
        {\
            pixPainter.begin(&filledPix);\
            pixPainter.drawTiledPixmap(filledPix.rect(), *_texPix, _OFF_-off);\
            pixPainter.end();\
            filledPix = FX::applyAlpha(filledPix, *tile);\
        }\
        else\
            filledPix = FX::tint(*tile, *_texColor);\
        tile = &filledPix;\
    }\
} // skip semicolon
#endif

#define MAKE_FILL(_OFF_)\
if (!tile->isNull())\
{\
    if ((_texPix || _texColor))\
    {\
        if (filledPix.size() != tile->size())\
            filledPix = tile->copy();\
        filledPix.fill(Qt::transparent);\
        if (_texPix)\
        {\
            pixPainter.begin(&filledPix);\
            pixPainter.drawTiledPixmap(filledPix.rect(), *_texPix, _OFF_-off);\
            pixPainter.end();\
            filledPix = FX::applyAlpha(filledPix, *tile);\
        }\
        else\
            filledPix = FX::tint(*tile, *_texColor);\
        tile = &filledPix;\
    }\
} // skip semicolon

    PosFlags pf = _shape ? _shape : _defShape;

    QPixmap filledPix; QPainter pixPainter;

    QPoint off = r.topLeft();
    if (_offset)
        off -= *_offset;
    int rOff = 0, xOff, yOff, w, h;

    r.getRect(&xOff, &yOff, &w, &h);
    int tlh = height(TopLeft), blh = height(BtmLeft),
        trh = height(TopRight), brh = height(BtmLeft),
        tlw = width(TopLeft), blw = width(BtmLeft),
        trw = width(TopRight), brw = width(BtmRight);

    // vertical overlap geometry adjustment (horizontal is handled during painting)
    if (pf & Left)
    {
        w -= width(TopLeft);
        xOff += width(TopLeft);
        if (pf & (Top | Bottom) && tlh + blh > r.height())
        {   // vertical edge overlap
            tlh = (tlh*r.height())/(tlh+blh);
            blh = r.height() - tlh;
        }
    }

    if (pf & Right)
    {
        w -= width(TopRight);
        if (pf & (Top | Bottom) && trh + brh > r.height())
        {   // vertical edge overlap
            trh = (trh*r.height())/(trh+brh);
            brh = r.height() - trh;
        }
    }

    if (pf & (Top | Bottom))
    {
        if (w < 0 && matches(Left | Right, pf))
        {   // horizontal edge overlap
            blw = /*atm*/ tlw = tlw*r.width()/(tlw+trw);
            brw = /*atm*/ trw = r.width() - tlw;

//             blw = (blw*r.width())/(blw+brw); // see above
//             brw = r.width() - blw; // see above
        }
    }

    rOff = r.right()-trw+1;

   // painting
    const QPixmap *tile;
    QRect checkRect;
    const bool unclipped = !p->hasClipping() || p->clipRegion().isEmpty();
#define UNCLIPPED (unclipped || p->clipRegion().intersects(checkRect))

#define NEED_RECT_FILL(_TILE_) \
_texPix && (!pixmap[_TILE_].hasAlphaChannel() ||\
(_texPix->width() > pixmap[_TILE_].width() && checkRect.width() > pixmap[_TILE_].width()) ||\
(_texPix->height() > pixmap[_TILE_].height() && checkRect.height() > pixmap[_TILE_].height()))

    if (pf & Top)
    {
        yOff += tlh;
        h -= tlh;

        checkRect.setRect(xOff, r.y(), w, tlh);
        if (w > 0 && !pixmap[TopMid].isNull() && UNCLIPPED)
        {   // upper line
            if (NEED_RECT_FILL(TopMid))
                p->drawTiledPixmap(checkRect, *_texPix, QPoint(xOff, r.y()) - off);
            else
            {
                tile = &pixmap[TopMid];
                MAKE_FILL(QPoint(xOff, r.y()));
                p->drawTiledPixmap(checkRect, *tile);
            }
        }

        checkRect.setRect(r.x(),r.y(), tlw, tlh);
        if ((pf & Left) && UNCLIPPED)
        {
            tile = &pixmap[TopLeft];
            MAKE_FILL(r.topLeft());
            p->drawPixmap(r.x(),r.y(), *tile, 0, 0, tlw, tlh);
        }

        checkRect.setRect(rOff, r.y(), trw, trh);
        if ((pf & Right) && UNCLIPPED)
        {
            tile = &pixmap[TopRight];
            MAKE_FILL(r.topRight()-tile->rect().topRight());
            p->drawPixmap(rOff, r.y(), *tile, width(TopRight)-trw, 0, trw, trh);
        }
    }

    if (pf & Bottom)
    {
        int bOff = r.bottom()-blh+1;
        h -= blh;

        checkRect.setRect(r.x(), bOff, blw, blh);
        if ((pf & Left) && UNCLIPPED)
        {
            tile = &pixmap[BtmLeft];
            MAKE_FILL(r.bottomLeft()-tile->rect().bottomLeft());
            p->drawPixmap(r.x(), bOff, *tile, 0, height(BtmLeft)-blh, blw, blh);
        }

        checkRect.setRect(rOff, bOff, brw, brh);
        if ((pf & Right) && UNCLIPPED)
        {
            tile = &pixmap[BtmRight];
            MAKE_FILL(r.bottomRight()-tile->rect().bottomRight());
            p->drawPixmap(rOff, bOff, *tile, width(BtmRight)-brw, height(BtmRight)-brh, brw, brh);
        }

        checkRect.setRect(xOff, bOff, w, blh);
        if (w > 0 && !pixmap[BtmMid].isNull() && UNCLIPPED)
        {   // lower line
            if (NEED_RECT_FILL(TopMid))
                p->drawTiledPixmap(checkRect, *_texPix, QPoint(xOff, bOff) - off);
            else
            {
                tile = &pixmap[BtmMid];
                MAKE_FILL(QPoint(xOff, bOff));
                p->drawTiledPixmap(checkRect, *tile);
            }
        }
    }

    if (h > 0)
    {
        checkRect.setRect(xOff, yOff, w, h);
        if ((pf & Center) && (w > 0) && !pixmap[MidMid].isNull() && UNCLIPPED)
        {   // center part
            if (NEED_RECT_FILL(MidMid))
                p->drawTiledPixmap(checkRect, *_texPix, QPoint(xOff, yOff) - off);
            else
            {
                tile = &pixmap[MidMid];
                MAKE_FILL(QPoint(xOff, yOff));
                p->drawTiledPixmap(checkRect, *tile);
            }
        }

        checkRect.setRect(r.x(), yOff, tlw, h);
        if ((pf & Left) && !pixmap[MidLeft].isNull() && UNCLIPPED)
        {
            if (NEED_RECT_FILL(MidLeft))
                p->drawTiledPixmap(checkRect, *_texPix, QPoint(r.x(), yOff) - off);
            else
            {
                tile = &pixmap[MidLeft];
                MAKE_FILL(QPoint(r.x(), yOff));
                p->drawTiledPixmap(checkRect, *tile);
            }
        }

        checkRect.setRect(rOff, yOff, trw, h);
        if ((pf & Right) && !pixmap[MidRight].isNull() && UNCLIPPED)
        {
            if (NEED_RECT_FILL(MidRight))
                p->drawTiledPixmap(checkRect, *_texPix, QPoint(rOff, yOff) - off);
            else
            {
                tile = &pixmap[MidRight];
                rOff = r.right()-width(MidRight)+1;
                MAKE_FILL(QPoint(rOff, yOff));
                p->drawTiledPixmap(checkRect, *tile);
            }
        }
    }

#undef MAKE_FILL
}

void
Set::outline(const QRect &r, QPainter *p, QColor c, int size) const
{
    PosFlags pf = _shape ? _shape : _defShape;
    const int d = (size+1)/2-1;
//    const int o = size%2;
    QRect rect = r.adjusted(d,d,-d,-d);
    if (rect.isNull())
        return;

    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
//    p->setClipRect(r);
    QPen pen = p->pen();
    pen.setColor(c); pen.setWidth(size);
    p->setPen(pen); p->setBrush(Qt::NoBrush);

    QList<QPainterPath> paths;
    paths << QPainterPath();
    QPoint end = rect.topLeft();
    Set *that = const_cast<Set*>(this);

    if (pf & Top)
    {
        if (pf & Right)
        {
            that->rndRect.moveTopRight(rect.topRight());
            paths.last().arcMoveTo(rndRect, 0);
            paths.last().arcTo(rndRect, 0, 90);
        }
        else
            paths.last().moveTo(rect.topRight());
        if (pf & Left)
        {
            that->rndRect.moveTopLeft(rect.topLeft());
            paths.last().arcTo(rndRect, 90, 90);
        }
        else
            paths.last().lineTo(rect.topLeft());
    }
    else
        paths.last().moveTo(rect.topLeft());

    if (pf & Left)
    {
        if (pf & Bottom)
        {
            that->rndRect.moveBottomLeft(rect.bottomLeft());
            paths.last().arcTo(rndRect, 180, 90);
        }
        else
            paths.last().lineTo(rect.bottomLeft());
    }
    else
    {
        if (!paths.last().isEmpty())
            paths << QPainterPath();
        paths.last().moveTo(rect.bottomLeft());
    }

    if (pf & Bottom)
    {
        if (pf & Right)
        {
            that->rndRect.moveBottomRight(rect.bottomRight());
            paths.last().arcTo(rndRect, 270, 90);
        }
        else
            paths.last().lineTo(rect.bottomRight());
    }
    else
    {
        if (!paths.last().isEmpty())
            paths << QPainterPath();
        paths.last().moveTo(rect.bottomRight());
    }

    if (pf & Right)
    {
        if (pf & Top)
            paths.last().connectPath(paths.first());
        else
            paths.last().lineTo(rect.topRight());
    }

    for (int i = 0; i < paths.count(); ++i)
        p->drawPath(paths.at(i));
    p->restore();
}

const QPixmap &Set::corner(PosFlags pf) const
{
   if (pf == (Top | Left))
      return pixmap[TopLeft];
   if (pf == (Top | Right))
      return pixmap[TopRight];
   if (pf == (Bottom | Right))
      return pixmap[BtmRight];
   if (pf == (Bottom | Left))
      return pixmap[BtmLeft];

   qWarning("requested impossible corner %d",pf);
   return nullPix;
}

void
Set::render(const QRect &rect, QPainter *p, const QColor &c) const
{
    _texColor = &c; render(rect, p); _texColor = 0L;
}

void
Set::render(const QRect &rect, QPainter *p, const QPixmap &pix, const QPoint &offset) const
{
    _texPix = &pix; _offset = &offset;
    render(rect, p);
    _texPix = 0L; _offset = 0L;
}

Line::Line(const QPixmap &pix, Qt::Orientation o, int d1, int d2)
{
    _o = o;
    QPainter p;
    if (o == Qt::Horizontal)
    {
        _thickness = pix.height();
        pixmap[0] = pix.copy(0, 0, d1, pix.height());

        int d = qMax(1, pix.width()-d1+d2);
        QPixmap dump = pix.copy(d1, 0, d, pix.height());
        pixmap[1] = QPixmap(qMax(32 , d), pix.height());
        pixmap[1].fill(Qt::transparent);
        p.begin(&pixmap[1]);
        p.drawTiledPixmap(pixmap[1].rect(), dump);
        p.end();

        pixmap[2] = pix.copy(pix.width()+d2, 0, -d2, pix.height());
    }
    else
    {
        _thickness = pix.width();
        pixmap[0] = pix.copy(0, 0, pix.width(), d1);

        int d = qMax(1, pix.height()-d1+d2);
        QPixmap dump = pix.copy(0, d1, pix.width(), d);
        pixmap[1] = QPixmap(pix.width(), qMax(32, d));
        pixmap[1].fill(Qt::transparent);
        p.begin(&pixmap[1]);
        p.drawTiledPixmap(pixmap[1].rect(), dump);
        p.end();

        pixmap[2] = pix.copy(0, pix.height()+d2, pix.width(), -d2);
    }
}

void
Line::render(const QRect &rect, QPainter *p, PosFlags pf, bool btmRight) const
{
    int d0,d2;
    if (_o == Qt::Horizontal)
    {
        const int y = btmRight ? (rect.bottom() + 1 - _thickness) : rect.y();
        d0 = (pf & Left) ? width(0) : 0;
        d2 = (pf & Right) ? width(2) : 0;
        if ((pf & Center) && rect.width() >= d0+d2)
            p->drawTiledPixmap(rect.x() + d0, y, rect.width() - (d0 + d2), height(1), pixmap[1]);
        else if (d0 || d2)
        {
            d0 = qMin(d0, d0*rect.width()/(d0+d2));
            d2 = qMin(d2, rect.width() - d0);
        }
        if (pf & Left)
            p->drawPixmap(rect.x(), y, pixmap[0], 0, 0, d0, height(0));
        if (pf & Right)
            p->drawPixmap(rect.right() + 1 - d2, y, pixmap[2], width(2) - d2, 0, d2, height(2));
    }
    else
    {
        const int x = btmRight ? (rect.right() + 1 - _thickness) : rect.x();
        d0 = (pf & Top) ? height(0) : 0;
        d2 = (pf & Bottom) ? height(2) : 0;
        if ((pf & Center) && rect.height() >= d0+d2)
            p->drawTiledPixmap(x, rect.y() + d0, width(1), rect.height() - (d0 + d2), pixmap[1]);
        else if (d0 || d2)
        {
            d0 = qMin(d0, d0*rect.height()/(d0 + d2));
            d2 = qMin(d2, rect.height() - d0);
        }
        if (pf & Top)
            p->drawPixmap(x, rect.y(), pixmap[0], 0, 0, width(0), d0);
        if (pf & Bottom)
            p->drawPixmap(x, rect.bottom() + 1 - d2, pixmap[2], 0, height(2) - d2, width(2), d2);
    }
}
