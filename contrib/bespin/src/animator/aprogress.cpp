#include <QProgressBar>
#include <QTimerEvent>
#include "aprogress.h"

#include <QtDebug>

using namespace Animator;

bool animationUpdate;

INSTANCE(Progress)
MANAGE(Progress)
RELEASE(Progress)
STEP(Progress)

static const float _speed = 1.8;  // NOT!!! 0.0! reasonable: 0.5 - 3.0
float
Progress::speed(){ return _speed; }


int
Progress::_step(const QWidget *widget, long int index) const
{
   return qAbs(info(widget, index).step(index));
}

void
Progress::timerEvent(QTimerEvent * event)
{
    if (event->timerId() != timer.timerId() || noAnimations())
        return;

    //Update the registered progressbars.
    Items::iterator iter;
    QProgressBar *pb;
    bool mkProper = false;
    animationUpdate = true;
    for (iter = items.begin(); iter != items.end(); iter++)
    {
        if (!iter.key()) // not a progressbar - shouldn't be in items, btw...
            { mkProper = true; continue; }
            
        pb = const_cast<QProgressBar*>(qobject_cast<const QProgressBar*>(iter.key()));
        if (!pb)
            continue; // not a progressbar - shouldn't be in items, btw...

        if (pb->maximum() != 0 || pb->minimum() != 0 || pb->paintingActive() || !pb->isVisible())
        {
            pb->setAttribute(Qt::WA_OpaquePaintEvent, false);
            continue; // no paint necessary
        }

        pb->setAttribute(Qt::WA_OpaquePaintEvent);

        ++iter.value();

        // dump pb geometry
        int x,y,l,t, *step = &iter.value()._step;
        if ( pb->orientation() == Qt::Vertical ) // swapped values
            pb->rect().getRect(&y,&x,&t,&l);
        else
            pb->rect().getRect(&x,&y,&l,&t);

        if (*step > l/_speed)
            *step = l/36-(int)(l/_speed);
        else if (*step == -1)
            *step = l/36-1;

        int s = qMin(qMax(l / 10, 16), qMin(t, 20));
        int ss = (3*s)/4;
        int n = l/s;
        if ( pb->orientation() == Qt::Vertical)
            { x = pb->rect().bottom(); x -= (l - n*s)/2 + ss; /*s = -s;*/ }
        else
            { x += (l - n*s)/2; /*s = qAbs(s);*/ }

        x += qMax((int)(_speed*qAbs(*step)*n*s/l) - s, 0);
        if ( pb->orientation() == Qt::Vertical )
            pb->repaint(y,x-s,s,3*s);
        else
            pb->repaint(x-s,y,3*s,s);
    }
    animationUpdate = false;
    if (mkProper)
        _release(NULL);
}
