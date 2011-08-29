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

#include "draw.h"

void
Style::drawCheckMark(const QStyleOption *option, QPainter *painter, Check::Type type) const
{
    // the checkmark (using brush)
    painter->setPen(Qt::NoPen);
    painter->setRenderHint(QPainter::Antialiasing);
    bool isOn = option->state & QStyle::State_On;
    switch (type)
    {
    case Check::X:
    {
        const int   d = RECT.height()/8,
                    c = RECT.height()/2,
                    s = RECT.width(),
                    x = RECT.x(), y = RECT.y();
        if (isOn)
        {
            const QPoint points[8] =
            {
                QPoint(x+c,y+c-d), QPoint(x,y),
                QPoint(x+c-d,y+c), QPoint(x,y+s),
                QPoint(x+c,y+c+d), QPoint(x+s,y+s),
                QPoint(x+c+d,y+c), QPoint(x+s,y)
            };
            painter->drawPolygon(points, 8);
        }
        else
        {   // tristate
            const QPoint points[5] =
            {
                QPoint(x+c,y+c-d), QPoint(x,y), QPoint(x+c-d,y+c),
                QPoint(x+s,y+s), QPoint(x+c+d,y+c),
            };
            painter->drawPolygon(points, 5);
        }
        break;
    }
    default:
    case Check::V:
    {
        if (isOn)
        {
            const QPoint points[4] =
            {
                QPoint(RECT.right(), RECT.top()),
                QPoint(RECT.x()+RECT.width()/4, RECT.bottom()),
                QPoint(RECT.x(), RECT.bottom()-RECT.height()/2),
                QPoint(RECT.x()+RECT.width()/4, RECT.bottom()-RECT.height()/4)
            };
            painter->drawPolygon(points, 4);
        }
        else
        {   // tristate
            const int d = 2*RECT.height()/5;
            QRect r = RECT.adjusted(F(2),d,-F(2),-d);
            painter->drawRect(r);
        }
        break;
    }
    case Check::O:
    {
        const int d = RECT.height()/8;
        QRect r = RECT.adjusted(d,d,-d,-d);
        if (!isOn)
            r.adjust(0,r.height()/4,0,-r.height()/4);
        painter->drawRoundRect(r,70,70);
    }
    }
}

void
Style::drawCheck(const QStyleOption *option, QPainter *painter, const QWidget*, bool itemview) const
{
    if (const QStyleOptionViewItemV2 *item = qstyleoption_cast<const QStyleOptionViewItemV2 *>(option))
    if (!(item->features & QStyleOptionViewItemV2::HasCheckIndicator))
        return;

//       if (option->state & State_NoChange)
//          break;
    QStyleOption copy = *option;

    const int f2 = F(2);

    // storage
    painter->save();
    QBrush oldBrush = painter->brush();
    painter->setRenderHint(QPainter::Antialiasing);

    // rect -> square
    QRect r = RECT;
    if (r.width() > r.height())
        r.setWidth(r.height());
    else
        r.setHeight(r.width());

    // box (requires set pen for PE_IndicatorMenuCheckMark)
    painter->setBrush(Qt::NoBrush);
    QPalette::ColorRole fg = QPalette::Text, bg = QPalette::Base;

    if (itemview)
    {   // itemViewCheck
        r.adjust(f2, f2, -f2, -f2);
        if (!(option->state & State_Off))
            copy.state |= State_On;
        if (option->state & State_Selected)
            { fg = QPalette::HighlightedText; bg = QPalette::Highlight; }
        painter->setPen(Colors::mid(COLOR(bg), COLOR(fg)));
    }

    if (appType != GTK)
    {
        if (painter->pen() != Qt::NoPen)
            { r.adjust(f2, f2, -f2, -f2); painter->drawRoundRect(r); }

        if (option->state & State_Off) // not checked, get out
            { painter->restore(); return; }
    }
    else
    {
        copy.rect.adjust(F(1), F(5), -F(6), -F(2));
        oldBrush = painter->pen().brush();
        copy.state |= State_On;
    }

    // checkmark
    if (itemview)
        painter->setBrush(COLOR(fg));
    else
    {
        painter->setBrush(oldBrush);
        painter->setBrushOrigin(r.topLeft());
    }
    copy.rect.adjust(F(3),0,0,-F(3));
    drawCheckMark(&copy, painter, Check::V);
    painter->restore();
}

/**static!*/ void
Style::drawExclusiveCheck(const QStyleOption * option, QPainter * painter, const QWidget *)
{
    painter->save();
    painter->setBrush(Qt::NoBrush);
    painter->setRenderHint ( QPainter::Antialiasing );
    painter->drawEllipse ( RECT );
    if (option->state & State_On)
    {
        painter->setBrush ( painter->pen().color() );
        const int dx = 3*RECT.width()/8, dy = 3*RECT.height()/8;
        painter->drawEllipse ( RECT.adjusted(dx, dy, -dx, -dy) );
    }
    painter->restore();
}

#define SET_POINTS(_P1_, _P2_, _P3_)\
points[0] = _P1_; points[1] = _P2_; points[2] = _P3_;

/**static!*/ void
Style::drawArrow(Navi::Direction dir, const QRect &rect, QPainter *painter, const QWidget*)
{
    // create an appropriate rect and move it to center of desired rect
    int s = qMin(rect.width(), rect.height());
    QRect r;
    if (!(s%2)) s -= 1; // shall be odd!
    if (dir > Navi::E)
    {   // diagonal
        s = int(s/1.4142135623); // sqrt(2)...
        r.setRect ( 0, 0, s, s );
    }
    else if (dir < Navi::W) // North/South
        r.setRect ( 0, 0, s, s/2+1 );
    else // East/West
        r.setRect ( 0, 0, s/2+1, s );
    r.moveCenter(rect.center());

    QPoint points[3];
    switch (dir)
    {
    case Navi::N:
    //       r.adjust(0,0,1,1);
        SET_POINTS(QPoint(r.center().x(), r.top()), r.bottomRight(), r.bottomLeft());
        break;
    case Navi::E:
        SET_POINTS(r.topLeft(), QPoint(r.right(), r.center().y()), r.bottomLeft());
        break;
    case Navi::S:
    //       r.adjust(0,0,1,1);
        SET_POINTS(r.topRight(), r.topLeft(), QPoint(r.center().x(), r.bottom()));
        break;
    case Navi::W:
        SET_POINTS(QPoint(r.left(), r.center().y()), r.topRight(), r.bottomRight());
        break;
    case Navi::NW:
        SET_POINTS(r.topLeft(), r.topRight(), r.bottomLeft());
        break;
    case Navi::NE:
        SET_POINTS(r.topLeft(), r.topRight(), r.bottomRight());
        break;
    case Navi::SE:
        SET_POINTS(r.topRight(), r.bottomRight(), r.bottomLeft());
        break;
    case Navi::SW:
        SET_POINTS(r.topLeft(), r.bottomRight(), r.bottomLeft());
        break;
    }
    // don't need antialiasing, our triangles are PERFECT ;)
    SAVE_ANTIALIAS
    painter->setRenderHint(QPainter::Antialiasing, false);
    bool reset_pen = (painter->pen() == Qt::NoPen);
    if (reset_pen)
        painter->setPen(QPen(painter->brush(), 1));
    painter->drawPolygon(points, 3);
    if (reset_pen)
        painter->setPen(Qt::NoPen);
    RESTORE_ANTIALIAS
}

extern bool isUrlNaviButtonArrow;
/**static!*/ void
Style::drawSolidArrow(Navi::Direction dir, const QRect &rect, QPainter *painter, const QWidget *w)
{
    if (isUrlNaviButtonArrow)
    {
        if ( painter->brush() != Qt::NoBrush && 
             (!w || painter->brush().color().rgb() == w->palette().color(QPalette::HighlightedText).rgb()) && 
             painter->brush().color().alpha() < 255 )
            dir = (dir == Navi::W) ? Navi::SW : Navi::SE;
        if (w)
        {
            painter->setBrush(w->palette().color(w->foregroundRole()));
            painter->setPen(w->palette().color(w->foregroundRole()));
        }
    }
    bool hadNoBrush = painter->brush() == Qt::NoBrush;
    if (hadNoBrush)
        painter->setBrush(painter->pen().brush());
    drawArrow(dir, rect, painter, w);
    if (hadNoBrush)
        painter->setBrush(Qt::NoBrush);
}
