/* Bespin widget style for Qt4
 * Copyright (C) 2011 Thomas Luebking <thomas.luebking@web.de>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License version 2 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */


#ifndef SPLITTERPROXY_H
#define SPLITTERPROXY_H

#include <QHoverEvent>
#include <QMainWindow>
#include <QMouseEvent>
#include <QSplitterHandle>
#include <QTimerEvent>
#include <QWidget>

static const int PADDING = 16;

class SplitterProxy;
static SplitterProxy *splitterProxy = 0;

class StdChildAdd : public QObject
{
public:
    bool eventFilter( QObject *, QEvent *ev)
    {
        return (ev->type() == QEvent::ChildAdded
#ifdef QT3_SUPPORT
                || ev->type() == QEvent::ChildInserted
#endif
                );
    }
};

static StdChildAdd stdChildAdd;

class SplitterProxy : public QWidget
{
public:
    static bool manage(QWidget *w)
    {
        if (qobject_cast<QMainWindow*>(w) || qobject_cast<QSplitterHandle*>(w))
        {
            if (!splitterProxy)
                splitterProxy = new SplitterProxy;
            // avoid double filtering
            w->removeEventFilter(splitterProxy);
            w->installEventFilter(splitterProxy);
            return true;
        }
        return false;
    }
    static void cleanUp() { splitterProxy->deleteLater(); }

    SplitterProxy() : QWidget(), mySplitter(0), myHoverChecker(0) { hide(); }
    ~SplitterProxy() { if (this == splitterProxy) splitterProxy = 0; }

protected:
    bool event(QEvent *e)
    {
        switch (e->type())
        {
        case QEvent::Paint:
        {
//             QPainter p(this);
//             p.fillRect(rect(), Qt::red);
//             p.end();
            return true;
        }
        case QEvent::MouseMove:
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonRelease:
        {
            e->accept();

            if (e->type() == QEvent::MouseButtonPress)
                grabMouse();

            if (parentWidget())
                parentWidget()->setUpdatesEnabled(false);
            resize(1,1);
            if (parentWidget())
                parentWidget()->setUpdatesEnabled(true);

            QMouseEvent *me = static_cast<QMouseEvent*>(e);
            const QPoint pos = (e->type() == QEvent::MouseButtonPress) ? myHook : mySplitter->mapFromGlobal(QCursor::pos());
            QMouseEvent me2(me->type(), pos, mySplitter->mapToGlobal(pos), me->button(), me->buttons(), me->modifiers());
            QCoreApplication::sendEvent(mySplitter, &me2);

            if (e->type() == QEvent::MouseButtonRelease)
            if (mouseGrabber() == this)
                releaseMouse();
//                 setSplitter(0);
            return true;
        }
        case QEvent::Timer:
            if (static_cast<QTimerEvent*>(e)->timerId() != myHoverChecker)
                return QWidget::event(e);
            if (mouseGrabber() == this)
                return true;
            //  ===> FALL THROUGH IS INTENDED! We somehow lost a QEvent::Leave and gonna fix that from here!
        case QEvent::HoverLeave:
        case QEvent::Leave:
//             QWidget::leaveEvent(e);
            if (!rect().contains(mapFromGlobal(QCursor::pos())))
                setSplitter(0);
            return true;
        default:
            return QWidget::event(e);
        }
    }
    bool eventFilter(QObject *o, QEvent *e)
    {
        if (/*o == mySplitter || */mouseGrabber())
            return false;

        switch (e->type())
        {
        case QEvent::HoverEnter:
            if (!isVisible())
            if (QSplitterHandle *handle = qobject_cast<QSplitterHandle*>(o))
            {
                setSplitter(handle);
                return false;
            }
        case QEvent::HoverMove:
        case QEvent::HoverLeave:
            if (isVisible() && o == mySplitter)
                return true;
        case QEvent::MouseMove:
        case QEvent::Timer:
        case QEvent::Move:
            return false; // just for performance - they can occur really often
        case QEvent::CursorChange:
        {
            if (QWidget *window = qobject_cast<QMainWindow*>(o))
            if (window->cursor().shape() == Qt::SplitHCursor ||
                window->cursor().shape() == Qt::SplitVCursor)
                setSplitter(window);
            return false;
        }
        case QEvent::MouseButtonRelease:
            if (qobject_cast<QSplitterHandle*>(o) || qobject_cast<QMainWindow*>(o))
                setSplitter(0);
            return false;
        default:
            return false;
        }
    }
private:
    void setSplitter(QWidget *splt)
    {
        if (!splt)
        {
            if (mouseGrabber() == this)
                releaseMouse();
            if (QWidget *dad = parentWidget())
            {
                dad->setUpdatesEnabled(false);
                setParent(0);
                dad->setUpdatesEnabled(true);
            }
            if (mySplitter)
            {
                QHoverEvent he(qobject_cast<QSplitterHandle*>(mySplitter) ? QEvent::HoverLeave : QEvent::HoverMove,
                               mySplitter->mapFromGlobal(QCursor::pos()), myHook);
                QCoreApplication::sendEvent(mySplitter, &he);
                killTimer(myHoverChecker);
                myHoverChecker = 0;
            }
            mySplitter = splt;
            return;
        }

        mySplitter = splt;
        myHook = mySplitter->mapFromGlobal(QCursor::pos());

        QWidget *w = mySplitter->window();
        QRect r(0,0,2*PADDING,2*PADDING);
        r.moveCenter(w->mapFromGlobal(QCursor::pos()));

        w->setUpdatesEnabled(false);
        w->installEventFilter(&stdChildAdd);
        setParent(w);
        w->removeEventFilter(&stdChildAdd);
        setGeometry(r);
        setCursor( mySplitter->cursor().shape() );

        raise();
        show();
        w->setUpdatesEnabled(true);
        myHoverChecker = startTimer(150); // sometimes Qt looses a leave event? Dolphin's fault? Mine?
    }
private:
    QWidget *mySplitter;
    QPoint myHook;
    int myHoverChecker;
};

#endif // SPLITTERPROXY_H
