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
#include "hoverindex.h"

using namespace Animator;

int
IndexInfo::step(long int index) const
{
    Fades::const_iterator i;
    for (int dir = 0; dir < 2; ++dir)
    {
        for (i = fades[dir].begin(); i != fades[dir].end(); ++i)
            if (i.key() == index)
                return i.value();
    }
    return 0;
}

INSTANCE(HoverIndex)
SET_FPS(HoverIndex)
SET_DURATION(HoverIndex)
#undef ANIMATOR_IMPL

HoverIndex::HoverIndex() : QObject(),
timeStep(_timeStep), count(0), maxSteps(_duration/_timeStep) {}

void
HoverIndex::_setFPS(uint fps)
{
   maxSteps = (1000 * maxSteps) / (timeStep * fps);
   timeStep = 1000/fps;
   if (timer.isActive())
      timer.start(timeStep, this);
}

const IndexInfo *
HoverIndex::info(const QWidget *widget, long int idx)
{
   if (!widget) return 0;
   if (!instance)
       instance = new HoverIndex;
   return instance->_info(widget, idx);
}

const IndexInfo *
HoverIndex::_info(const QWidget *widget, long int idx) const
{
    HoverIndex *that = const_cast<HoverIndex*>(this);
    QWidget *w = const_cast<QWidget*>(widget);
    Items::iterator it = that->items.find(w);
    if (it == items.end())
    {   // we have no entry yet
        if (idx == 0L)
            return 0L;
        // ... but we'll need one
        it = that->items.insert(w, IndexInfo(0L));
        connect(widget, SIGNAL(destroyed(QObject*)), this, SLOT(release(QObject*)));
//         if (!timer.isActive())
            that->timer.start(timeStep, that);
    }
    // we now have an entry - check for validity and update in case
    IndexInfo &info = it.value();
    if (info.index != idx)
    {   // sth. changed
        info.fades[In][idx] = 1;
        if (info.index)
        {
            int v = maxSteps;
            IndexInfo::Fades::iterator old = info.fades[In].find(info.index);
            if (old != info.fades[In].end())
            {
                v = old.value();
                info.fades[In].erase(old);
            }
            info.fades[Out][info.index] = v;
        }
        info.index = idx;
    }
    return &info;
}

void
HoverIndex::release(QObject *o)
{
    QWidget *w = qobject_cast<QWidget*>(o);
    if (!w) return;

    items.remove(w);
    if (items.isEmpty())
        timer.stop();
}


void
HoverIndex::timerEvent(QTimerEvent * event)
{
    if (event->timerId() != timer.timerId() || items.isEmpty())
        return;

    Items::iterator it;
    IndexInfo::Fades::iterator step;
    it = items.begin();
    QWidget *w;
    while (it != items.end())
    {
        if (!it.key())
        {
            it = items.erase(it);
            continue;
        }
#if QT_VERSION >= 0x040400
        // below does work in general, but is... ugly?!
        // another way would be to map to a const widget first or perform a static_cast - ughh
        w = const_cast<QWidget*>(it.key().data());
#else
        w = const_cast<QWidget*>(&(*it.key()));
#endif
        IndexInfo &info = it.value();
        if (info.fades[In].isEmpty() && info.fades[Out].isEmpty())
        {
            ++it; continue;
        }

        step = info.fades[In].begin();
        while (step != info.fades[In].end())
        {
            step.value() += 2;
            if ((uint)step.value() > (maxSteps-2))
                step = info.fades[In].erase(step);
            else
                ++step;
        }

        step = info.fades[Out].begin();
        while (step != info.fades[Out].end())
        {
            step.value() -= 2;
            if (step.value() < 1)
                step = info.fades[Out].erase(step);
            else
                ++step;
        }

        w->update();

        if (info.index == 0L && // nothing actually hovered
            info.fades[In].isEmpty() && // no fade ins
            info.fades[Out].isEmpty()) // no fade outs
            it = items.erase(it); // so remove this item
        else
            ++it;
    }

    if (items.isEmpty())
        timer.stop();
}
