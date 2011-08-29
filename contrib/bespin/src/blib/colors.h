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

#ifndef COLORS_H
#define COLORS_H

class QWidget;
#include <QColor>
#include <QPalette>

namespace Bespin {
namespace Colors {

    BLIB_EXPORT const QColor &bg(const QPalette &pal, const QWidget *w);
    BLIB_EXPORT int contrast(const QColor &a, const QColor &b);
    BLIB_EXPORT QPalette::ColorRole counterRole(QPalette::ColorRole role);
    BLIB_EXPORT bool counterRole( QPalette::ColorRole &from, QPalette::ColorRole &to,
                                  QPalette::ColorRole defFrom = QPalette::WindowText,
                                  QPalette::ColorRole defTo = QPalette::Window);
    BLIB_EXPORT QColor emphasize(const QColor &c, int value = 10);
    BLIB_EXPORT bool haveContrast(const QColor &a, const QColor &b);
    BLIB_EXPORT QColor light(const QColor &c, int value);
    BLIB_EXPORT QColor mid(const QColor &oc1, const QColor &c2, int w1 = 1, int w2 = 1);
    BLIB_EXPORT int value(const QColor &c);

}
}

#endif //COLORS_H
