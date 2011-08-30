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

#include <QPainterPath>
#include <QRectF>
#include "shapes.h"

using namespace Bespin;

#define _S(_I_) const float s##_I_ = bound.height() / (float)_I_;

QPainterPath
Shapes::close(const QRectF &bound, Style style)
{
    _S(3) _S(4) _S(8)
    QPainterPath path;
    switch (style)
    {
    case Square:
        path.addRect(bound);
        path.addRect(bound.adjusted(s4,s4,-s4,-s4));
        path.addRect(bound.adjusted(s3,s3,-s3,-s3));
        break;
    case LasseKongo:
        path.addRect(bound);
        path.addRect(bound.adjusted(s8,s8,-s8,-s8));
        path.addRect(bound.adjusted(s3,s3,-s3,-s3));
        break;
    default:
    case Round:
        path.addEllipse(bound);
        path.addEllipse(bound.adjusted(s3, s3, -s3, -s3));
        break;
    case TheRob:
        path.addEllipse(bound);
        path.addEllipse(bound.adjusted(s8,s8,-s8,-s8));
        path.addEllipse(bound.adjusted(s4,s4,-s4,-s4));
    }
    return path;
}

QPainterPath
Shapes::min(const QRectF &bound, Style style)
{
    _S(3) _S(4) _S(8)
    QPainterPath path;
    switch (style)
    {
    case Square:
    {
        path.addRect(bound);
        path.addRect(bound.adjusted(s4, 0, 0, -s4));
        path.addRect(bound.adjusted(2*s3, 0, 0, -2*s3));
        break;
    }
    case LasseKongo:
        path.addRect(bound.adjusted(0, 2*s3, 0, 0));
        path.addRect(bound.adjusted(s8, 2*s3, -s8, -s8));
        break;
    default:
    case Round:
        path.moveTo(bound.center());
        path.arcTo(bound, 180, 180);
        path.closeSubpath();
        break;
    case TheRob:
        path.moveTo(bound.center());
        path.arcTo(bound, 180, 180);
        path.closeSubpath();
        path.moveTo(bound.center());
        path.arcTo(bound.adjusted(s8,s8,-s8,-s8), 180, 180);
        path.closeSubpath();
        path.addEllipse(bound.adjusted(s4,s4,-s4,-s4));
        break;
    }
    return path;
}

QPainterPath
Shapes::max(const QRectF &bound, Style style)
{
    _S(3) _S(4) _S(8)
    QPainterPath path;
    switch (style)
    {
    case Square:
        path.addRect(bound);
        path.addRect(bound.adjusted(0, s4, -s4, 0));
        path.addRect(bound.adjusted(0, 2*s3, -2*s3, 0));
        break;
    case LasseKongo:
    {
        _S(5);
        const float d = 3*s5;
        QRectF rect = bound.adjusted(0,0,-d,-d);
        QRectF rect2(0,0,d,d);
        
        path.addRect(rect);
        rect2.moveCenter(rect.bottomRight());
        path.moveTo(rect2.center()); path.arcTo(rect2, 90, 90); path.closeSubpath();

        rect.translate(d,0);
        path.addRect(rect);
        rect2.moveCenter(rect.bottomLeft());
        path.moveTo(rect2.center()); path.arcTo(rect2, 0, 90); path.closeSubpath();
        
        rect.translate(0,d);
        path.addRect(rect);
        rect2.moveCenter(rect.topLeft());
        path.moveTo(rect2.center()); path.arcTo(rect2, -90, 90); path.closeSubpath();
        
        rect.translate(-d,0);
        path.addRect(rect);
        rect2.moveCenter(rect.topRight());
        path.moveTo(rect2.center()); path.arcTo(rect2, -180, 90); path.closeSubpath();
        break;
    }
    default:
    case Round:
        path.moveTo(bound.center());
        path.arcTo(bound, 0, 180);
        path.closeSubpath();
        break;
    case TheRob:
        path.moveTo(bound.center());
        path.arcTo(bound, 0, 180);
        path.closeSubpath();
        path.moveTo(bound.center());
        path.arcTo(bound.adjusted(s8,s8,-s8,-s8), 0, 180);
        path.closeSubpath();
        path.addEllipse(bound.adjusted(s4,s4,-s4,-s4));
        break;
    }
    return path;
}

QPainterPath
Shapes::dockControl(const QRectF &bound, bool floating, Style style)
{
    _S(4)
    QPainterPath path;
    switch (style)
    {
    case Square:
    case LasseKongo:
        if (floating)
            path.addRect(bound.adjusted(0, s4, 0, -s4));
        else
        {
            _S(2) _S(3)
            path.addRect(bound.adjusted(0, 0, -3*s4, -s3));
            path.addRect(bound.adjusted(s3, s2, -s3, 0));
        }
        break;
    default:
    case Round:
    case TheRob:
        if (floating)
        {
            _S(6)
            path.moveTo(bound.center());
            path.arcTo(bound, 180, 270);
            QRectF rect = bound.adjusted(0,0,-s6,-s6);
            path.moveTo(rect.center());
            path.arcTo(rect, 90, 90);
            path.closeSubpath();
        }
        else
            path = Shapes::unAboveBelow(bound, style);
        break;
    }
    return path;
}

QPainterPath
Shapes::restore(const QRectF &bound, Style style)
{
    _S(3) _S(4) _S(8)
    QPainterPath path;
    switch (style)
    {
    case Square:
        path.addRect(bound);
        path.addRect(bound.adjusted(0, 0, -s4, -s4));
        path.addRect(bound.adjusted(0, 0, -2*s3, -2*s3));
        break;
    case LasseKongo:
        path.addEllipse(bound);
        path.addRect(bound.adjusted(s3,0,-s3,0));
        path.addRect(bound.adjusted(0,s3,-2*s3,-s3));
        path.addRect(bound.adjusted(2*s3,s3,0,-s3));
        break;
    default:
    case Round:
        path.moveTo(bound.center());
        path.arcTo(bound, 225, 180);
        path.closeSubpath();
        break;
    case TheRob:
        path.moveTo(bound.center());
        path.arcTo(bound, 225, 180);
        path.closeSubpath();
        path.moveTo(bound.center());
        path.arcTo(bound.adjusted(s8,s8,-s8,-s8), 225, 180);
        path.closeSubpath();
        path.addEllipse(bound.adjusted(s4,s4,-s4,-s4));
        break;
    }
    return path;
}

QPainterPath
Shapes::stick(const QRectF &bound, Style style)
{
    _S(6)
    QPainterPath path;
    switch (style)
    {
    case Square:
        path.addRect(bound.adjusted(s6, s6, -s6, -s6));
        break;
    case LasseKongo:
    {
        _S(4);
        const float d = 3*s4;
        QRectF rect = bound.adjusted(0,0,-d,-d);
        path.addRect(rect);
        path.addRect(rect.translated(d,0));
        path.addRect(rect.translated(0,d));
        path.addRect(rect.translated(d,d));
        break;
    }
    default:
    case Round:
    case TheRob:
        path.addEllipse(bound.adjusted(s6, s6, -s6, -s6));
        break;
    }
    return path;
}

QPainterPath
Shapes::unstick(const QRectF &bound, Style style)
{
    _S(3)
    QPainterPath path;
    switch (style)
    {
    case Square:
    case LasseKongo:
        path.addRect(bound.adjusted(s3, s3, -s3, -s3));
        break;
    default:
    case Round:
    case TheRob:
        path.addEllipse(bound.adjusted(s3, s3, -s3, -s3));
        break;
    }
    return path;
}

QPainterPath
Shapes::keepAbove(const QRectF &bound, Style style)
{
    _S(3) _S(4) _S(2) _S(6)
    QPainterPath path;
    switch (style)
    {
    case Square:
    case LasseKongo:
        path.addRect(bound.adjusted(s4, 0, -s4, -2*s3));
        path.addRect(bound.adjusted(0, 2*s3, -2*s3, 0));
        path.addRect(bound.adjusted(2*s3, 2*s3, 0, 0));
        break;
    default:
    case Round:
    case TheRob:
    {
        QRectF rect = bound.adjusted(0, s2+s6, -s2, s6);
        path.moveTo(bound.center());
        path.arcTo(bound, 0, 180);
        path.closeSubpath();
        path.moveTo(rect.center());
        path.arcTo(rect, 0, 180);
        path.closeSubpath();
        rect.translate(s2, 0);
        path.moveTo(rect.center());
        path.arcTo(rect, 0, 180);
        path.closeSubpath();
        break;
    }
    }
    return path;
}

QPainterPath
Shapes::keepBelow(const QRectF &bound, Style style)
{
    _S(2) _S(3) _S(4)
    QPainterPath path;
    switch (style)
    {
    case Square:
    case LasseKongo:
        path.addRect(bound.adjusted(s4, 2*s3, -s4, 0));
        path.addRect(bound.adjusted(0, 0, -2*s3, -2*s3));
        path.addRect(bound.adjusted(2*s3, 0, 0, -2*s3));
        break;
    default:
    case Round:
    case TheRob:
        QRectF rect = bound.adjusted(0, 0, -s2, -s2);
        path.moveTo(bound.center() + QPointF(0, s2));
        path.arcTo(bound.translated(0, s2), 0, 180);
        path.closeSubpath();
        path.moveTo(rect.center());
        path.arcTo(rect, 0, 180);
        path.closeSubpath();
        rect.translate(s2, 0);
        path.moveTo(rect.center());
        path.arcTo(rect, 0, 180);
        path.closeSubpath();
        break;
    }
    return path;
}

QPainterPath
Shapes::unAboveBelow(const QRectF &bound, Style style)
{
    _S(6) _S(3) _S(4)
    QPainterPath path;
    switch (style)
    {
        case Square:
        case LasseKongo:
            path.addRect(bound.adjusted(0, s4, -2*s3, -s4));
            path.addRect(bound.adjusted(2*s3, s4, 0, -s4));
            break;
        default:
        case Round:
        case TheRob:
            QRectF rect = bound.adjusted(0,0,-s6, 0);
            path.moveTo(rect.center());
            path.arcTo(rect, 90, 180);
            path.closeSubpath();
            rect.translate(s6,0);
            path.moveTo(rect.center());
            path.arcTo(rect, -90, 180);
            path.closeSubpath();
            break;
    }
    return path;
}

QPainterPath
Shapes::menu(const QRectF &bound, bool leftSide, Style style)
{
    _S(2)
    QPainterPath path;
    switch (style)
    {
    case Square:
    {
        _S(4)
        path.addRect(bound);
        path.addRect(bound.adjusted(leftSide ? s2 : 0, s4, leftSide ? 0 : -s2, 0));
        break;
    }
    case LasseKongo:
        path.moveTo(bound.topLeft() + QPointF(0, s2));
        path.lineTo(bound.topRight() + QPointF(0, s2));
        path.lineTo(bound.bottomLeft() + QPointF(s2, 0));
        path.closeSubpath();
        break;
    default:
    case Round:
    case TheRob:
    {
        _S(9)
        path.moveTo(bound.center());
        path.arcTo(bound, leftSide ? -90 : 0, 270);
        path.closeSubpath();
        path.addRect(bound.adjusted(leftSide ? 0 : 5*s9, 5*s9, leftSide ? -5*s9 : 0, 0));
        break;
    }
    }
    return path;
}

QPainterPath
Shapes::help(const QRectF &bound, Style style)
{
    _S(2) _S(3) _S(4) _S(6)
    QPainterPath path;
    switch (style)
    {
    case Square:
    case LasseKongo:
        path.addRect(bound.adjusted(s2-s3, 0, -s4 , -s3));
        path.addRect(bound.adjusted(s2-s3, s4, -s2 , -s3));
        path.addRect(bound.adjusted(s2, 5*s6, -s4 , 0));
        break;
    default:
    case Round:
    case TheRob:
        path.moveTo(bound.center());
        path.arcTo(bound, -30, 180);
        path.addEllipse(bound.adjusted(s2, s2+s6, -s6, 0));
        break;
    }
    return path;
}

QPainterPath
Shapes::shade(const QRectF &bound, Style style)
{
    _S(3)
    QPainterPath path;
    switch (style)
    {
    case Square:
    case LasseKongo:
        path.addRect(bound.adjusted(0, s3, 0, -s3));
        break;
    default:
    case Round:
    case TheRob:
        path.addEllipse(bound.adjusted(0, s3, 0, -s3));
        break;
    }
    return path;
}

QPainterPath
Shapes::unshade(const QRectF &bound, Style style)
{
    _S(3)
    QPainterPath path;
    switch (style)
    {
    case Square:
    case LasseKongo:
        path.addRect(bound.adjusted(0, s3, 0, -s3));
        break;
    default:
    case Round:
    case TheRob:
        path.addEllipse(bound.adjusted(0, s3, 0, -s3));
        break;
    }
    return path;
}

QPainterPath
Shapes::exposee(const QRectF &bound, Style style)
{
    _S(3)
    const float d = 2*s3;
    QRectF rect = bound.adjusted(0,0,-d,-d);
    QPainterPath path;
    switch (style)
    {
    case Square:
    case LasseKongo:
        rect = bound.adjusted(0,0,-d,-d);
        path.addRect(rect);
        path.addRect(rect.translated(d,0));
        path.addRect(rect.translated(0,d));
        path.addRect(rect.translated(d,d));
        break;
    default:
    case Round:
    case TheRob:
        path.addEllipse(rect);
        path.addEllipse(rect.translated(d,0));
        path.addEllipse(rect.translated(0,d));
        path.addEllipse(rect.translated(d,d));
        break;
    }
    return path;
}

QPainterPath
Shapes::info(const QRectF &bound, Style style)
{
    _S(3) _S(4)
    QPainterPath path;
    switch (style)
    {
    case Square:
    case LasseKongo:
        path.addRect(bound.adjusted(s3, 0, -s3, -3*s4));
        path.addRect(bound.adjusted(s3, s3, -s3, 0));
        break;
    default:
    case Round:
    case TheRob:
        path.addEllipse(bound.adjusted(s3, 0, -s3, -2*s3));
        path.addEllipse(bound.adjusted(s3, s3, -s3, 0));
        break;
    }
    return path;
}


QPainterPath
Shapes::logo(const QRectF &bound)
{
    _S(4) _S(8) _S(12)
    QPainterPath path;
    path.moveTo(bound.center());
    path.arcTo(bound, 90, 270);
    path.lineTo(bound.right(), bound.y() + 4*s12);
    path.lineTo(bound.right() - s4, bound.y() + 4*s12);
    path.lineTo(bound.center().x() + s8, bound.center().y());
    path.lineTo(bound.center());
    path.closeSubpath();
    path.addEllipse(bound.right() - 3*s8, bound.y(), s4, s4);
    return path;
}
