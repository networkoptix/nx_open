/* Bespin widget style for Qt4
Copyright (C); 2007 Thomas Luebking <thomas.luebking@web.de>

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

#ifndef PATHS_H
#define PATHS_H

class QPainterPath;
class QRectF;

namespace Bespin
{
namespace Shapes
{
    enum Style { Square, Round, TheRob, LasseKongo };
    BLIB_EXPORT QPainterPath close(const QRectF &bound, Style style = Round);
    BLIB_EXPORT QPainterPath min(const QRectF &bound, Style style = Round);
    BLIB_EXPORT QPainterPath max(const QRectF &bound, Style style = Round);
    BLIB_EXPORT QPainterPath dockControl(const QRectF &bound, bool floating, Style style = Round);
    BLIB_EXPORT QPainterPath restore(const QRectF &bound, Style style = Round);
    BLIB_EXPORT QPainterPath stick(const QRectF &bound, Style style = Round);
    BLIB_EXPORT QPainterPath unstick(const QRectF &bound, Style style = Round);
    BLIB_EXPORT QPainterPath keepAbove(const QRectF &bound, Style style = Round);
    BLIB_EXPORT QPainterPath keepBelow(const QRectF &bound, Style style = Round);
    BLIB_EXPORT QPainterPath unAboveBelow(const QRectF &bound, Style style = Round);
    BLIB_EXPORT QPainterPath menu(const QRectF &bound, bool leftSide, Style style = Round);
    BLIB_EXPORT QPainterPath help(const QRectF &bound, Style style = Round);
    BLIB_EXPORT QPainterPath shade(const QRectF &bound, Style style = Round);
    BLIB_EXPORT QPainterPath unshade(const QRectF &bound, Style style = Round);
    BLIB_EXPORT QPainterPath exposee(const QRectF &bound, Style style = Round);
    BLIB_EXPORT QPainterPath info(const QRectF &bound, Style style = Round);
    BLIB_EXPORT QPainterPath logo(const QRectF &bound);
}
}
#endif // PATHS_H

