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

#ifndef HOVER_INDEX_ANIMATOR_H
#define HOVER_INDEX_ANIMATOR_H

#include <QBasicTimer>
#include <QMap>
#include "../bepointer.h"

namespace Animator {

   enum Dir { In = 0, Out };
   
class IndexInfo {
public:
   IndexInfo(long int idx) {index = idx;}
   virtual ~IndexInfo() {}
   virtual int step(long int idx = 0) const;
protected:
   friend class HoverIndex;
   typedef QMap<long int, int> Fades;
   Fades fades[2];
   long int index;
};
   
class HoverIndex : public QObject {
   Q_OBJECT
public:
   static const IndexInfo *info(const QWidget *widget, long int index);
   static void setDuration(uint ms);
   static void setFPS(uint fps);
protected:
   HoverIndex();
   virtual ~HoverIndex(){}
   virtual const IndexInfo *_info(const QWidget *widget, long int index) const;
   virtual void _setFPS(uint fps);
   virtual void timerEvent(QTimerEvent * event);
   QBasicTimer timer;
   uint timeStep, count, maxSteps;
   typedef QPointer<QWidget> WidgetPtr;
   typedef QMap<WidgetPtr, IndexInfo> Items;
   Items items;
protected slots:
   void release(QObject *o);
private:
    Q_DISABLE_COPY(HoverIndex)
};

}

#ifndef ANIMATOR_IMPL
#define ANIMATOR_IMPL 0
#endif

#if ANIMATOR_IMPL

#define INSTANCE(_CLASS_) static _CLASS_ *instance = 0;

#define SET_FPS(_CLASS_)\
static uint _timeStep = 50;\
void _CLASS_::setFPS(uint fps)\
{\
_timeStep = fps/1000;\
if (instance) instance->_setFPS(fps);\
   }
   #define SET_DURATION(_CLASS_)\
   static uint _duration = 300;\
   void _CLASS_::setDuration(uint ms)\
   {\
   _duration = ms;\
   if (instance) instance->maxSteps = ms/_timeStep;\
   }

#undef ANIMATOR_IMPL
   
#endif //ANIMATOR_IMPL

#endif //HOVER_INDEX_ANIMATOR_H
