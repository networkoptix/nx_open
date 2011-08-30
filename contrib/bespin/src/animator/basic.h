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

#ifndef BASIC_ANIMATOR_H
#define BASIC_ANIMATOR_H

#include <QBasicTimer>
#include <QMap>
#include "../bepointer.h"

namespace Animator {

class Info
{
public:
    Info(int s = 0, bool bwd = false);
    virtual ~Info(){}
    virtual int step(long int idx = 0) const;
    bool bwd() const;
    int & operator++ (){return ++_step;}
    int operator++ ( int ) {return _step++;}
    int & operator-- () {return --_step;}
    int operator-- ( int ) {return _step--;}
protected:
    friend class Basic;
    friend class Progress;
    friend class Hover;
    int _step; bool backwards;
    void init(int s = 0, bool bwd = false);
};

class Basic : public QObject
{
    Q_OBJECT
public:
    static bool manage(QWidget *w);
    static void release(QWidget *w);
    static int step(const QWidget *widget);
    virtual const Info &info(const QWidget *widget, long int index = 0) const;
    static void setFPS(uint fps);

protected:
    Basic();
    virtual ~Basic(){}
    virtual bool eventFilter( QObject *object, QEvent *event );
    virtual bool noAnimations() const;
    virtual void play(QWidget *widget, bool bwd = false);
    virtual bool _manage(QWidget *w);
    virtual void _release(QWidget *w);
    virtual int _step(const QWidget *widget, long int index = 0) const;
    virtual void timerEvent(QTimerEvent * event);
    virtual void _setFPS(uint fps);
    QBasicTimer timer;
    uint timeStep;
    uint count;
    typedef BePointer<QWidget> WidgetPtr;
    typedef QMap<WidgetPtr, Info> Items;
    Items items;
protected slots:
    virtual void release_s(QObject*);
//    void pause(QWidget *w);
private:
    Q_DISABLE_COPY(Basic)
};

} // namespace

#define INSTANCE(_CLASS_) static _CLASS_ *instance = 0;
#define MANAGE(_CLASS_)\
bool _CLASS_::manage(QWidget *w)\
{\
   if (!w) return false;\
   if (!instance) instance = new _CLASS_;\
   return instance->_manage(w);\
}

// TODO check whether the eventFilter is used at all!
//    if (!instance->count) {
//       delete instance; instance = 0;
//    }

#define RELEASE(_CLASS_)\
void _CLASS_::release(QWidget *w)\
{\
   if (!(w && instance)) return;\
   instance->_release(w);\
}

#define STEP(_CLASS_)\
int _CLASS_::step(const QWidget *widget)\
{\
   if (!instance) return 0;\
   return instance->_step(widget);\
}

#define SET_FPS(_CLASS_)\
static uint _timeStep = 50;\
void _CLASS_::setFPS(uint fps)\
{\
   _timeStep = 1000/fps;\
   if (!instance) return;\
   instance->_setFPS(fps);\
}


#endif // BASIC_ANIMATOR_H
