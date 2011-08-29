/* Bespin widget style for Qt4
   Copyright (C) 2007-2010 Thomas Luebking <thomas.luebking@web.de>

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

#include <QtGui/QStylePlugin>

#include "bespin.h"

class BespinStylePlugin : public QStylePlugin
{
public:
    QStringList keys() const {
        return QStringList() << QLatin1String("Bespin");
    }

    QStyle *create(const QString &key) {
        return key == QLatin1String("bespin") ? new Bespin::Style : 0;
    }
};

Q_EXPORT_PLUGIN2(Bespin, BespinStylePlugin)
