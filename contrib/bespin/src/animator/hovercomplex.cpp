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

#include <QTimerEvent>
#include <QWidget>

#define ANIMATOR_IMPL 1
#include "hovercomplex.h"

using namespace Animator;

INSTANCE(HoverComplex)
SET_FPS(HoverComplex)
SET_DURATION(HoverComplex)
#undef ANIMATOR_IMPL

const ComplexInfo *
HoverComplex::info(const QWidget *widget, QStyle::SubControls active)
{
   if (!widget)
       return 0;
   if (!instance)
       instance = new HoverComplex;
   return instance->_info(widget, active);
}

const ComplexInfo *
HoverComplex::_info(const QWidget *widget, QStyle::SubControls active) const
{
    QWidget *w = const_cast<QWidget*>(widget);
    HoverComplex *that = const_cast<HoverComplex*>(this);
    Items::iterator it = that->items.find(w);
    if (it == items.end())
    {
        // we have no entry yet
        if (active == QStyle::SC_None)
            return 0; // no need here
        // ...but we'll need one
        it = that->items.insert(w, ComplexInfo());
        connect(w, SIGNAL(destroyed(QObject*)), this, SLOT(release(QObject*)));
        that->timer.start(timeStep, that);
    }
    // we now have an entry - check for validity and update in case
    ComplexInfo *info = &it.value();
    if (info->active != active)
    {   // sth. changed
        QStyle::SubControls diff = info->active ^ active;
        QStyle::SubControls newActive = diff & active;
        QStyle::SubControls newDead = diff & info->active;
        info->fades[In] &= ~newDead; info->fades[In] |= newActive;
        info->fades[Out] &= ~newActive; info->fades[Out] |= newDead;
        info->active = active;
        for (QStyle::SubControl control = (QStyle::SubControl)0x01;
                                control <= (QStyle::SubControl)0x80;
                                control = (QStyle::SubControl)(control<<1))
        {
            if (newActive & control)
                info->steps[control] = 1;
//             else if (newDead & control)
//             {
//                 info->steps[control] = maxSteps;
//             }
        }
    }
    return info;
}


void
HoverComplex::timerEvent(QTimerEvent * event)
{
    if (event->timerId() != timer.timerId() || items.isEmpty())
        return;

    bool update;
    Items::iterator it = items.begin();
    ComplexInfo *info;
    while (it != items.end())
    {
        if (!it.key())
        {
            it = items.erase(it);
            continue;
        }
        info = &it.value();
        update = false;
        for (QStyle::SubControl control = (QStyle::SubControl)0x01;
            control <= (QStyle::SubControl)0x80;
            control = (QStyle::SubControl)(control<<1))
            {
                if (info->fades[In] & control)
                {
                    update = true;
                    info->steps[control] += 2;
                    if (info->steps.value(control) > 4)
                        info->fades[In] &= ~control;
                }
                else if (info->fades[Out] & control)
                {
                    update = true;
                    --info->steps[control];
                    if (info->steps.value(control) < 1)
                        info->fades[Out] &= ~control;
                }
            }
        if (update)
            it.key()->update();
        if (info->active == QStyle::SC_None && // needed to detect changes!
                                                info->fades[Out] == QStyle::SC_None &&
                                                info->fades[In] == QStyle::SC_None)
            it = items.erase(it);
        else
            ++it;
    }

    if (items.isEmpty())
        timer.stop();
}
