/* Bespin widget style for Qt4
   Copyright (C) 2011 Thomas Luebking <thomas.luebking@web.de>

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

#ifndef BESPIN_SHADOWS_H
#define BESPIN_SHADOWS_H

class QWidget;

namespace Bespin {
namespace Shadows {
    enum BLIB_EXPORT Type { None = 0, Small, Large };
    BLIB_EXPORT void cleanUp();
    BLIB_EXPORT void manage(QWidget *w);
    BLIB_EXPORT void set(WId id, Shadows::Type t, bool storeToRoot = false);
} }

#endif // BESPIN_SHADOWS_H
