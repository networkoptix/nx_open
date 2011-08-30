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

#ifndef HOVER_COMPLEX_ANIMATOR_H
#define HOVER_COMPLEX_ANIMATOR_H

#include <QStyle>
#include "hoverindex.h"

namespace Animator {

class ComplexInfo
{
    public:
        ComplexInfo() {
            active = fades[In] = fades[Out] = QStyle::SC_None;
        }
        QStyle::SubControls active, fades[2];
        inline int step(QStyle::SubControl sc) const {return steps.value(sc);}
    private:
        friend class HoverComplex;
        QMap<QStyle::SubControl, int> steps;
};

class HoverComplex : public HoverIndex
{
    public:
        static const ComplexInfo *info(const QWidget *widget, QStyle::SubControls active);
        static void setDuration(uint ms);
        static void setFPS(uint fps);
    protected:
        const ComplexInfo *_info(const QWidget *widget, QStyle::SubControls active) const;
        void timerEvent(QTimerEvent * event);
        typedef BePointer<QWidget> WidgetPtr;
        typedef QMap<WidgetPtr, ComplexInfo> Items;
        Items items;
    private:
        Q_DISABLE_COPY(HoverComplex)
        HoverComplex() {}
};

} // namespace

#endif // HOVER_COMPLEX_ANIMATOR_H

