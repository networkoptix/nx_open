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

#include "visualframe.h"
#include <QBitmap>
#include <QCoreApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QStyleOption>
#include <QStyle>
#include <QWheelEvent>
#include <QTimer>

#define DEBUG_VF 0
#if DEBUG_VF
#include <QtDebug>
#define VDebug(_STREAM_) qDebug() << "BESPIN:" << _STREAM_
#else
#define VDebug(_STREAM_) //
#define DEBUG //
#endif
#include <QtDebug>

QStyle *VisualFrame::ourStyle = 0L;

using namespace VFrame;

static QRegion corner[4];
static int sizes[3][4];
static int extends[3][4];
static int notInited = 0x7;

static VFrame::Type
type(QFrame::Shadow shadow)
{
   switch (shadow) {
   default:
   case QFrame::Plain: return VFrame::Plain;
   case QFrame::Raised: return VFrame::Raised;
   case QFrame::Sunken: return VFrame::Sunken;
   }
}

void
VisualFrame::setGeometry(QFrame::Shadow shadow, const QRect &inner, const QRect &outer)
{

    // first call, generate corner regions
    if (corner[North].isEmpty())
    {
        int f5 = 4; // TODO: make this value dynamic!
        QBitmap bm(2*f5, 2*f5);
        bm.fill(Qt::black);
        QPainter p(&bm);
        p.setPen(Qt::NoPen);
        p.setBrush(Qt::white);
        p.drawEllipse(0,0,2*f5,2*f5);
        p.end();
        QRegion circle(bm);
        corner[North] = circle & QRegion(0,0,f5,f5); // tl
        corner[South] = circle & QRegion(f5,0,f5,f5); // tr
        corner[South].translate(-corner[South].boundingRect().left(), 0);
        corner[West] = circle & QRegion(0,f5,f5,f5); // bl
        corner[West].translate(0, -corner[West].boundingRect().top());
        corner[East] = circle & QRegion(f5,f5,f5,f5); // br
        corner[East].translate(-corner[East].boundingRect().topLeft());
    }

    const Type t = type(shadow);
    notInited &= ~(1 << t);

    sizes[t][North] = inner.y() - outer.y();
    sizes[t][South] = outer.bottom() - inner.bottom();
    sizes[t][East] = outer.right() - inner.right();
    sizes[t][West] = inner.x() - outer.x();
    extends[t][North] = -outer.y();
    extends[t][South] = outer.bottom() - 99;
    extends[t][East] = outer.right() - 99;
    extends[t][West] = -outer.x();
}

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

bool
VisualFrame::manage(QFrame *frame)
{
    if (!frame)
        return false;
    VDebug ("======= MANAGE" << frame << "===============");
    QList<VisualFrame*> vfs = frame->window()->findChildren<VisualFrame*>();
    foreach (VisualFrame* vf, vfs)
    {
        VDebug ("test" << vf);
        if (vf->myFrame == frame)
        {
            VDebug (frame << "is allready managed by" << vf);
            return false;
        }
    }
//    if (!vfs.isEmpty()) return false; // avoid double adds

    VDebug ("add new visual frame for" << frame);
    new VisualFrame(frame);
    return true;
}

void
VisualFrame::release(QFrame *frame)
{
    if (!frame) return;
    VDebug ("======= RELEASE" << frame << "===============");
    QList<VisualFrame*> vfs = frame->window()->findChildren<VisualFrame*>();
    foreach (VisualFrame* vf, vfs)
    {
        VDebug ("test" << vf);
        if (vf->myFrame == frame)
        {
            VDebug (frame << "matches" << vf << "... releasing");
            frame->clearMask();
            vf->hide(); vf->deleteLater();
        }
    }
}

// TODO: mange ALL frames and catch shape/shadow changes!!!
VisualFrame::VisualFrame(QFrame *parent) : QObject(0),
myFrame(0), myWindow(0), top(0), bottom(0), left(0), right(0), hidden(true)
{
    myStyle = (QFrame::Shape)-1;
    if (notInited)
    {
        qWarning("You need to initialize the VisualFrames with\n\
                VisualFrame::setGeometry()\n\
                for all three QFrame::Shadow types first!\n\
                No Frame added.");
        deleteLater(); return;
    }
    if (!parent)
        { deleteLater(); return; }

   // create frame elements
   myFrame = parent;
   myFrame->installEventFilter(this);
   connect(myFrame, SIGNAL(destroyed(QObject*)), this, SLOT(hideMe()));
   connect(myFrame, SIGNAL(destroyed(QObject*)), this, SLOT(deleteMuchLater()));
   updateShape();
}

void
VisualFrame::deleteMuchLater() // more or less ensure we get deleted after the parts
{
    QTimer::singleShot(500, this, SLOT(deleteLater()));
}


void
VisualFrame::updateShape()
{
    myStyle = myFrame->frameShape();

    if (myStyle != QFrame::StyledPanel)
    {
        if (top) { top->hide(); top->deleteLater(); top = 0L; }
        if (bottom) { bottom->hide(); bottom->deleteLater(); bottom = 0L; }
        if (left) { left->hide(); left->deleteLater(); left = 0L; }
        if (right) { right->hide(); right->deleteLater(); right = 0L; }
        myFrame->clearMask();

        QWidget *runner = myFrame->parentWidget();
        while (runner && runner != myWindow)
        {
            runner->removeEventFilter(this);
            runner = runner->parentWidget();
        }
        hidden = true;
        return;
    }

    // we start out with myWindow == myFrame, the actual parent widget is recalculated on each show()
    // to capture e.g. dock floats etc.
    myWindow = myFrame;
    myWindow->installEventFilter(&stdChildAdd);
    if (!top) top = new VisualFramePart(myWindow, myFrame, this, North);
    if (!bottom) bottom = new VisualFramePart(myWindow, myFrame, this, South);
    if (!left) left = new VisualFramePart(myWindow, myFrame, this, West);
    if (!right) right = new VisualFramePart(myWindow, myFrame, this, East);
    myWindow->removeEventFilter(&stdChildAdd);
    // manage events
    top->removeEventFilter(this);
    top->installEventFilter(this);
    if (myFrame->isVisible())
        show();
    else
        hide();
    QTimer::singleShot(0, this, SLOT(correctPosition()));
}

inline static QRect
correctedRect(QFrame *frame)
{
    QRect rect = frame->frameRect();
    if (frame->autoFillBackground() && frame->inherits("QLabel"))
    {
        int l,r,t,b;
        frame->getContentsMargins(&l, &t, &r, &b);
        rect.adjust(-l, -t, r, b);
    }
    // NOTICE: this works around a Qt rtl (bug(?))!
    else if ( (frame->layoutDirection() == Qt::RightToLeft) &&
               rect.right() != frame->rect().right() &&
               frame->inherits("QAbstractScrollArea") )
        rect.moveLeft(rect.x() + (frame->rect().right() - rect.right()));
    return rect;
}

void
VisualFrame::correctPosition()
{
    if (hidden) return;
    if (myStyle != QFrame::StyledPanel) return;

    VFrame::Type t = type(myFrame->frameShadow());
    QRect rect = myFrameRect = correctedRect(myFrame);
#if 0
    int x,y,r,b;
    rect.getRect(&x, &y, &r, &b);
    ++y; --b; // I have NO idea why this is required
    QRegion mask(x+4, y, r-8, b);
    mask += QRegion(x, y+4, r, b-8);
    mask += QRegion(x+2, y+1, r-4, b-2);
    mask += QRegion(x+1, y+2, r-2, b-4);
    myFrame->setMask(mask);
    r += x; b += y;
#else
    // mask
    int x,y,r,b;
    rect.getRect(&x, &y, &r, &b); r += x; b += y;
    QRegion mask(myFrame->QWidget::rect());// myFrame->mask().isEmpty() ? myFrame->rect() : myFrame->mask();
    mask -= corner[North].translated(x, y); // tl
    QRect br = corner[South].boundingRect();
    mask -= corner[South].translated(r-br.width(), y); // tr
    br = corner[West].boundingRect();
    mask -= corner[West].translated(x, b-br.height()); // bl
    br = corner[East].boundingRect();
    mask -= corner[East].translated(r-br.width(), b-br.height()); // br
    myFrame->setMask(mask);
#endif
    // position
    rect.translate(myFrame->mapTo(myWindow, QPoint(0,0)));
    rect.getRect(&x, &y, &r, &b);
    int offs = extends[t][West] + extends[t][East];

    // north element
    top->resize(rect.width()+offs, sizes[t][North]);
    top->move(x - extends[t][West], y - extends[t][North]);

    // South element
    bottom->resize(rect.width() + offs, sizes[t][South]);
    bottom->move(x - extends[t][West], rect.bottom()  + extends[t][South] - sizes[t][South]);

    offs = (sizes[t][North] + sizes[t][South]) - (extends[t][North] + extends[t][South]);

    // West element
    left->resize(sizes[t][West], rect.height() - offs);
    left->move(x - extends[t][West], y + sizes[t][North] - extends[t][North]);

    // East element
    right->resize(sizes[t][East], rect.height() - offs);
    right->move(rect.right() + 1 - sizes[t][East] + extends[t][East], y + sizes[t][North] - extends[t][North]);
    //    qDebug() << myFrame->frameRect() << rect << right->geometry();
}

#define PARTS(_FUNC_) if (top) { top->_FUNC_; left->_FUNC_; right->_FUNC_; bottom->_FUNC_; } void(0)

void
VisualFrame::show()
{
    hidden = false;
    if (myFrame->style() != style())
    {
        hide();
        return;
    }
    if (myStyle != QFrame::StyledPanel)
        return;

    if (!top)
    {
        updateShape();
        return; // calls "show" anyway
    }

    QWidget *window = myFrame;
    while (window->parentWidget())
    {
        window->removeEventFilter(this);
        window->installEventFilter(this);
        window = window->parentWidget();
        if ((window->isWindow() || window->inherits("QMdiSubWindow") ||
            (window != myFrame && window->inherits("QAbstractScrollArea"))))
            break;
    }

    if (window != myWindow)
    {
        if (top->parentWidget() != myWindow)
        {
            myWindow->installEventFilter(&stdChildAdd);
            PARTS(setParent(myWindow));
            myWindow->removeEventFilter(&stdChildAdd);
        }
    }

    raise();
    correctPosition();
    PARTS(show());
}

void
VisualFrame::hideMe()
{
    hidden = true;
}

void
VisualFrame::hide()
{
    hideMe();
    QWidget *window = myFrame;
    while ( (window = window->parentWidget()) )
    {
        window->removeEventFilter(this);
        if ((window->isWindow() || window->inherits("QMdiSubWindow") ||
            (window != myFrame && window->inherits("QAbstractScrollArea"))))
            break;
    }
    if (myStyle != QFrame::StyledPanel)
        return;
    PARTS(hide());
}

static bool blockRaise = false;

void
VisualFrame::raise()
{
    if (hidden || blockRaise || myStyle != QFrame::StyledPanel)
        return;

    blockRaise = true;
    QWidget *sibling = myFrame;
    while (sibling)
    {
        if (sibling->parentWidget() == myWindow)
            break;
        sibling = sibling->parentWidget();
    }
    const QObjectList &kids = myWindow->children();
    bool above = false;
    for (QObjectList::const_iterator i = kids.constBegin(); i != kids.constEnd(); ++i)
    {
        if (above)
        {
            if (QWidget *w = qobject_cast<QWidget*>(*i))
            {
                if (qobject_cast<VisualFramePart*>(w))
                    continue;
                sibling = w;
                break;
            }
        }
        else if (*i == sibling)
        {
            above = true;
            sibling = 0L;
        }
    }
    if (sibling)
    {
        PARTS(stackUnder(sibling));
    }
    else
    {
        PARTS(raise());
    }
    blockRaise = false;
}

void
VisualFrame::repaint()
{
    if (hidden || myStyle != QFrame::StyledPanel)
        return;
    PARTS(repaint());
}

void
VisualFrame::update()
{
    if (hidden || myStyle != QFrame::StyledPanel)
        return;
    PARTS(update());
}

#undef PARTS

static bool blockZEvent = false;
bool
VisualFrame::eventFilter ( QObject * o, QEvent * ev )
{
    if (o == top)
    {   // "top" is the only monitored framepart!
        if (ev->type() == QEvent::ZOrderChange && !(blockZEvent || hidden))
        {
            blockZEvent = true;
            raise();
            blockZEvent = false;
        }
        return false;
    }

    if (ev->type() == QEvent::Move)
    {   // for everyone between frame and parent...
        correctPosition();
        return false;
    }

    if (ev->type() == QEvent::ZOrderChange && !blockZEvent)
    {   // necessary for all widgets from frame to parent
        blockZEvent = true;
        raise();
        blockZEvent = false;
        return false;
    }

    if (ev->type() == QEvent::StyleChange)
    {
        if (myFrame->style() == style() && myFrame->isVisible())
            show();
        else
            hide();
        return false;
    }

//     if (ev->type() == QEvent::ParentChange) {
//         myWindow = myFrame->window();
//         qDebug() << "reparented" << o << myWindow << myFrame->window();
//         return false;
//     }

    if (o != myFrame)
    {   // now we're only interested in frame events
        return false;
    }

    if (ev->type() == QEvent::Paint)
    {
        if (myFrame->frameShape() != myStyle)
            updateShape();
        return false;
    }

    if (myStyle != QFrame::StyledPanel)
        return false;

    if (ev->type() == QEvent::Show)
    {
        show();
        return false;
    }

    if (ev->type() == QEvent::Resize || ev->type() == QEvent::LayoutDirectionChange)
    {
        correctPosition();
        return false;
    }

    if (ev->type() == QEvent::FocusIn || ev->type() == QEvent::FocusOut)
    {
        update();
        return false;
    }

    if (ev->type() == QEvent::Hide)
    {
        hide();
        return false;
    }

    return false;
//    if (ev->type() == QEvent::ParentChange) {
//       qWarning("parent changed?");
//       myFrame->parentWidget() ?
//          setParent(myFrame->parentWidget() ) :
//          setParent(myFrame );
//       raise();
//    }
//    return false;
}

VisualFramePart::VisualFramePart(QWidget * parent, QFrame *frame, VisualFrame *vFrame, Side side) :
QWidget(parent), myFrame(frame), _vFrame(vFrame), _side(side)
{
   connect(myFrame, SIGNAL(destroyed(QObject*)), this, SLOT(hide())); // done by vframe
   connect(myFrame, SIGNAL(destroyed(QObject*)), this, SLOT(deleteLater()));
//    setMouseTracking ( true );
//    setAcceptDrops(true);
}

void
VisualFramePart::paintEvent ( QPaintEvent * event )
{
    if (!_vFrame->style())
        return;
    QPainter p(this);
    p.setClipRegion(event->region() & rect(), Qt::IntersectClip);
    QStyleOption opt; Type t;
    if (myFrame->frameShadow() == QFrame::Raised)
    {
        opt.state |= QStyle::State_Raised;
        t = VFrame::Raised;
    }
    else if (myFrame->frameShadow() == QFrame::Sunken)
    {
        opt.state |= QStyle::State_Sunken;
        t = VFrame::Sunken;
    }
    else
        t = VFrame::Plain;

    if (myFrame->hasFocus())
        opt.state |= QStyle::State_HasFocus;

    if (myFrame->isEnabled())
        opt.state |= QStyle::State_Enabled;

    opt.rect = _vFrame->frameRect();

    switch (_side)
    {
    case North:
        opt.rect.setWidth(opt.rect.width() + extends[t][West] + extends[t][East]);
        opt.rect.setHeight(opt.rect.height() + extends[t][North]);
        opt.rect.moveTopLeft(rect().topLeft());
        break;
    case South:
        opt.rect.setWidth(opt.rect.width() + extends[t][West] + extends[t][East]);
        opt.rect.setHeight(opt.rect.height() + extends[t][South]);
        opt.rect.moveBottomLeft(rect().bottomLeft());
        break;
    case West:
        opt.rect.setWidth(opt.rect.width() + extends[t][West]);
        opt.rect.setHeight(opt.rect.height() + sizes[t][North] + sizes[t][South]);
        opt.rect.moveTopLeft(QPoint(0, -sizes[t][North]));
        break;
    case East:
        opt.rect.setWidth(opt.rect.width() + extends[t][East]);
        opt.rect.setHeight(opt.rect.height() + sizes[t][North] + sizes[t][South]);
        opt.rect.moveTopRight(QPoint(width()-1, -sizes[t][North]));
        break;
    }
    _vFrame->style()->drawPrimitive(QStyle::PE_Frame, &opt, &p, this);
    p.end();
}

void
VisualFramePart::passDownEvent(QEvent *ev, const QPoint &gMousePos)
{
    // the raised frames don't look like you could click in, we'll see if this should be changed...
    if (myFrame->frameShadow() != QFrame::Sunken)
        return;
    const QObjectList &candidates = myFrame->children();
    QObjectList::const_iterator i = candidates.constEnd();
    QWidget *match = 0;
    while (i != candidates.constBegin())
    {
        --i;
        if (*i == this)
            continue;
        if (QWidget *w = qobject_cast<QWidget*>(*i))
        {
            if (w->rect().contains(w->mapFromGlobal(gMousePos)))
            {
                match = w;
                break;
            }
        }
    }
    if (!match)
        match = myFrame;
    QEvent *nev = 0;
    if (ev->type() == QEvent::Wheel)
    {
        QWheelEvent *wev = static_cast<QWheelEvent *>(ev);
        QWheelEvent wev2( match->mapFromGlobal(gMousePos), gMousePos,
                          wev->delta(), wev->buttons(), wev->modifiers(),
                          wev->orientation() );
        nev = &wev2;
    }
    else
    {
        QMouseEvent *mev = static_cast<QMouseEvent *>(ev);
        QMouseEvent mev2( mev->type(), match->mapFromGlobal(gMousePos), gMousePos,
                          mev->button(), mev->buttons(), mev->modifiers() );
        nev = &mev2;
    }
    QCoreApplication::sendEvent( match, nev );
}

#define HANDLE_EVENT(_EV_) \
void VisualFramePart::_EV_ ( QMouseEvent * event ) {\
passDownEvent((QEvent *)event, event->globalPos());\
}

HANDLE_EVENT(mouseDoubleClickEvent)
HANDLE_EVENT(mouseMoveEvent)
HANDLE_EVENT(mousePressEvent)
HANDLE_EVENT(mouseReleaseEvent)

void
VisualFramePart::wheelEvent ( QWheelEvent * event ) {
   passDownEvent((QEvent *)event, event->globalPos());
}

#undef HANDLE_EVENT
