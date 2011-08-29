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

#include <QAbstractScrollArea>
#include <QApplication>
#include <QCoreApplication>
#include <QDesktopWidget>
#include <QDockWidget>
#include <QEvent>
#include <QGroupBox>
#include <QLabel>
#include <QLayout>
#include <QMenuBar>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include "bepointer.h"
#include <QScrollBar>
#include <QWindowStateChangeEvent>
#include <QStatusBar>
#include <QStyle>
#include <QStyleOption>
#include <QStyleOptionGroupBox>
#include <QTabBar>
#include <QToolBar>
#include <QToolButton>

#ifdef Q_WS_X11

#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusReply>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <QX11Info>
//#include "blib/fixx11h.h"
#include "blib/xproperty.h"


#include <qmath.h>

static Atom netMoveResize = XInternAtom(QX11Info::display(), "_NET_WM_MOVERESIZE", False);
#endif

#include <QtDebug>
#include "blib/colors.h"
#include "blib/gradients.h"
#include "hacks.h"

using namespace Bespin;

#define ENSURE_INSTANCE if (!bespinHacks) bespinHacks = new Hacks
#define FILTER_EVENTS(_WIDGET_) { _WIDGET_->removeEventFilter(bespinHacks); _WIDGET_->installEventFilter(bespinHacks); } // skip semicolon


static Hacks *bespinHacks = 0L;
static Hacks::HackAppType *appType = 0L;
// const char *SMPlayerVideoWidget = "MplayerLayer" ;// MplayerWindow
// const char *DragonVideoWidget = "Phonon::VideoWidget"; // Codeine::VideoWindow, Phonon::Xine::VideoWidget
const char *VLCVideoWidget = "BackgroundWidget"; // "VideoWidget"; //
static BePointer<QWidget> dragCandidate = 0L;
static BePointer<QWidget> dragWidget = 0L;
static bool dragWidgetHadTrack = false;
static QMenu *lockToggleMenu = 0L;
static QToolBar *lockToggleBar = 0L;
static QAction *lockToggleAction = 0L;

static void
triggerWMMove(const QWidget *w, const QPoint &p)
{
#ifdef Q_WS_X11
   // stolen... errr "adapted!" from QSizeGrip
   QX11Info info;
   XEvent xev;
   xev.xclient.type = ClientMessage;
   xev.xclient.message_type = netMoveResize;
   xev.xclient.display = QX11Info::display();
   xev.xclient.window = w->window()->winId();
   xev.xclient.format = 32;
   xev.xclient.data.l[0] = p.x();
   xev.xclient.data.l[1] = p.y();
   xev.xclient.data.l[2] = 8; // NET::Move
   xev.xclient.data.l[3] = Button1;
   xev.xclient.data.l[4] = 0;
   XUngrabPointer(QX11Info::display(), QX11Info::appTime());
   XSendEvent(QX11Info::display(), QX11Info::appRootWindow(info.screen()), False,
               SubstructureRedirectMask | SubstructureNotifyMask, &xev);
#endif // Q_WS_X11
}

inline static bool
hackMessageBox(QMessageBox* box, QEvent *e)
{
    switch (e->type())
    {
    case QEvent::Paint:
    {
        int s = qMin(164, 3*box->height()/2);
        QStyleOption opt; opt.rect = QRect(0,0,s,s); opt.palette = box->palette();
        QStyle::StandardPixmap logo = (QStyle::StandardPixmap)0;
        switch (box->icon())
        {
        case QMessageBox::NoIcon:
        default:
            break;
        case QMessageBox::Question:
            logo = QStyle::SP_MessageBoxQuestion; break;
        case QMessageBox::Information:
            logo = QStyle::SP_MessageBoxInformation; break;
        case QMessageBox::Warning:
            logo = QStyle::SP_MessageBoxWarning; break;
        case QMessageBox::Critical:
            logo = QStyle::SP_MessageBoxCritical; break;
        }
        QPixmap icon = box->style()->standardPixmap ( logo, &opt, box );
        QPainter p(box);
        if (logo)
        {
            const int y = (box->height()-s)/2 - qMax(0,(box->height()-164)/3);
            p.drawPixmap(-s/3,y, icon);
        }
        p.setPen(Colors::mid(box->palette().color(QPalette::Window), box->palette().color(QPalette::WindowText)));
        p.drawRect(box->rect().adjusted(0,0,-1,-1));
        p.end();
        return true;
    }
    case QEvent::MouseButtonPress:
    {
        QMouseEvent *mev = static_cast<QMouseEvent*>(e);
        if (mev->button() == Qt::LeftButton)
            triggerWMMove(box, mev->globalPos());
        return false;
    }
    case QEvent::Show:
    {
        if (box->layout())
            box->layout()->setSpacing(8);
        if (QLabel *icon = box->findChild<QLabel*>("qt_msgboxex_icon_label"))
        {
            icon->setPixmap(QPixmap());
            icon->setFixedSize(box->height()/3,10);
        }
        if (QLabel *text = box->findChild<QLabel*>("qt_msgbox_label"))
        {
            text->setAutoFillBackground(true);
            QPalette pal = text->palette();
            QColor c = pal.color(QPalette::Base);
            c.setAlpha(220);
            pal.setColor(QPalette::Base, c);
            text->setPalette(pal);
            text->setBackgroundRole(QPalette::Base);
            text->setForegroundRole(QPalette::Text);
//             text->setContentsMargins(16, 8, 16, 8);
            text->setMargin(8);
            text->setFrameStyle ( QFrame::StyledPanel | QFrame::Sunken );
            text->setLineWidth ( 0 );
            if (!text->text().contains("<h2>"))
            {
                if (box->windowTitle().isEmpty())
                {
                    QFont bold = text->font(); bold.setBold(true); text->setFont(bold);
                }
                else
                {
                    QString head = "<qt><h2>" + box->windowTitle() + "</h2>";
//             switch (box->icon()){
//             case QMessageBox::NoIcon:
//             default:
//                break;
//             case QMessageBox::Question:
//                head = "<qt><h2>Question...</h2>"; break;
//             case QMessageBox::Information:
//                head = "<qt><h2>Info:</h2>"; break;
//             case QMessageBox::Warning:
//                head = "<qt><h2>Warning!</h2>"; break;
//             case QMessageBox::Critical:
//                head = "<qt><h2>Error!</h2>"; break;
//             }
                    QString newText = text->text();
                    if (newText.contains("<qt>"))
                    {
                        newText.replace(QRegExp("^(<qt>)*"), head);
                        if (!newText.endsWith("</qt>"))
                            newText.append("</qt>");
                    }
                    else
                        newText.prepend(head);
                    text->setText(newText);
                }
            }
        }
        if (QLabel *info = box->findChild<QLabel*>("qt_msgbox_informativelabel"))
        {
            info->setAutoFillBackground(true);
            QPalette pal = info->palette();
            QColor c = pal.color(QPalette::Base);
            c.setAlpha(220);
            pal.setColor(QPalette::Base, c);
            c = Colors::mid(pal.color(QPalette::Base), pal.color(QPalette::Text),1,3);
            pal.setColor(QPalette::Text, c);
            info->setPalette(pal);
            info->setBackgroundRole(QPalette::Base);
            info->setForegroundRole(QPalette::Text);
            info->setMargin(4);
        }
        return false;
    }
    default:
        return false;
    }
    return false;
}

static bool
isWindowDragWidget(QObject *o, const QPoint *pt = 0L)
{
    if (!Hacks::config.windowMovement)
        return false;

    if ( qobject_cast<QDialog*>(o) ||
         (qobject_cast<QMenuBar*>(o) && !static_cast<QMenuBar*>(o)->activeAction()) ||
         qobject_cast<QGroupBox*>(o) ||

         (qobject_cast<QToolButton*>(o) && !static_cast<QWidget*>(o)->isEnabled()) ||
         qobject_cast<QToolBar*>(o) || qobject_cast<QDockWidget*>(o) ||

         qobject_cast<QStatusBar*>(o) ||

        // now catched by QMainWindow
//          ((*appType == Hacks::SMPlayer) && o->inherits(SMPlayerVideoWidget)) ||
//          ((*appType == Hacks::Dragon) && o->inherits(DragonVideoWidget)) ||
        // whatever the videowidget is, VLC does not work since it apparently does not get polished...
//         ((*appType == Hacks::VLC) && o->inherits(VLCVideoWidget)) ||

            (o->inherits("QMainWindow") ) )
        return true;

    if ( QLabel *label = qobject_cast<QLabel*>(o) )
    if (!(label->textInteractionFlags() & Qt::TextSelectableByMouse))
    if (qobject_cast<QStatusBar*>(o->parent()))
        return true;

    if ( QTabBar *bar = qobject_cast<QTabBar*>(o) )
    if ( !pt || (bar->tabAt(*pt) < 0 && !bar->childAt(*pt)) )
        return true;

    return false;
}

static bool
hackMoveWindow(QWidget* w, QEvent *e)
{
    if (w->mouseGrabber())
        return false;
    // general validity ================================
    QMouseEvent *mev = static_cast<QMouseEvent*>(e);
//         !w->rect().contains(w->mapFromGlobal(QCursor::pos()))) // KColorChooser etc., catched by mouseGrabber ?!
    // avoid if we click a menu action ========================================
    if (QMenuBar *bar = qobject_cast<QMenuBar*>(w))
    if (bar->activeAction() || bar->inherits("QDesignerMenuBar")) // ....
        return false;

    // avoid if we try to (un)check a groupbx ==============================
    if (QGroupBox *gb = qobject_cast<QGroupBox*>(w))
    if (gb->isCheckable())
    {
        // gather options, fucking protected functions... :-(
        QStyleOptionGroupBox opt;
        opt.initFrom(gb);
        if (gb->isFlat())
            opt.features |= QStyleOptionFrameV2::Flat;
        opt.lineWidth = 1; opt.midLineWidth = 0;

        opt.text = gb->title();
        opt.textAlignment = gb->alignment();

        opt.subControls = (QStyle::SC_GroupBoxFrame | QStyle::SC_GroupBoxCheckBox);
        if (!gb->title().isEmpty())
            opt.subControls |= QStyle::SC_GroupBoxLabel;

        opt.state |= (gb->isChecked() ? QStyle::State_On : QStyle::State_Off);

        if (gb->style()->subControlRect(QStyle::CC_GroupBox, &opt, QStyle::SC_GroupBoxCheckBox, gb).contains(mev->pos()))
            return false;
    }

    // preserve dock / toolbar internal move float trigger on dock titles =================
    if (w->cursor().shape() != Qt::ArrowCursor ||
        (mev->pos().y() < w->fontMetrics().height()+4 && qobject_cast<QDockWidget*>(w)))
        return false;

    triggerWMMove(w, mev->globalPos());

    return true;
}


// obviously gwenview is completely incapable to keep the view position when watching an image
// -> we need a beter image browser :(
static int gwenview_position = 0;

bool
Hacks::eventFilter(QObject *o, QEvent *e)
{
    if (dragWidget && e->type() == QEvent::MouseMove)
    {
        qApp->removeEventFilter(this);
        dragWidget->setMouseTracking(dragWidgetHadTrack);
        // the widget needs an leave/enter to update the mouse state
        // sending events doesn't work, so we generate a wink-of-an-eye cursor repositioning ;-P
        const QPoint cursor = QCursor::pos();
        QWidget *window = dragWidget->window();
        QCursor::setPos(window->mapToGlobal( window->rect().topRight() ) + QPoint(2, 0) );
        QCursor::setPos(cursor);
        dragWidget = 0L;
        dragCandidate = 0L;
        return false;
    }

    if (e->type() == QEvent::Timer || e->type() == QEvent::Move)
        return false;

    if (QMessageBox* box = qobject_cast<QMessageBox*>(o))
        return hackMessageBox(box, e);

    if ( e->type() == QEvent::Paint )
        return false;

    if (e->type() == QEvent::MouseButtonPress)
    {
        QMouseEvent *mev = static_cast<QMouseEvent*>(e);
        QWidget *w = qobject_cast<QWidget*>(o);
        if (config.lockToolBars &&  (mev->modifiers() & Qt::ControlModifier) && qobject_cast<QToolBar*>(w) && !w->inherits("KToolBar"))
        {
            lockToggleBar = static_cast<QToolBar*>(w);
            lockToggleAction->setChecked(!lockToggleBar->isMovable());
            lockToggleMenu->popup(QCursor::pos());
            return true;
        }
        if ( !w || w->mouseGrabber() || // someone else is more interested in this
             (mev->modifiers() != Qt::NoModifier) || // allow forcing e.g. ctrl + click
             (mev->button() != Qt::LeftButton)) // rmb shall not move, maybe resize?!
                return false;
        if (isWindowDragWidget(o, &mev->pos()))
        {
            dragCandidate = w;
            qApp->installEventFilter(this);
        }
        return false;
    }

    if (dragCandidate && e->type() == QEvent::MouseButtonRelease)
    {   // was just a click
        qApp->removeEventFilter(this);
        dragCandidate = 0L;
        return false;
    }

    if (dragCandidate && e->type() == QEvent::MouseMove) // gonna be draged
    {   // we perhaps want to drag
        const bool wmDrag = hackMoveWindow(dragCandidate, e);
        if ( wmDrag )
        {
            dragWidget = dragCandidate;

            // the release would set "dragCandidate = 0L;", therfore it cannot be done in hackMoveWindow
            // it's probably not required either
//             QMouseEvent *mev = static_cast<QMouseEvent*>(e);
//             QMouseEvent mbr(QEvent::MouseButtonRelease, mev->pos(), mev->button(), mev->buttons(), mev->modifiers());
//             QCoreApplication::sendEvent( dragWidget, &mbr );
            dragWidgetHadTrack = dragWidget->hasMouseTracking();
            dragWidget->setMouseTracking(true);
        }
        dragCandidate = 0L;
        return wmDrag;
    }

    if ( *appType == Gwenview &&
         ( e->type() == QEvent::Wheel || e->type() == QEvent::Show || e->type() == QEvent::Hide ) &&
         o->objectName() == "qt_scrollarea_viewport" )
    {
        if ( QAbstractScrollArea *list = qobject_cast<QAbstractScrollArea*>(o->parent()) )
        {
            QScrollBar *bar = list->verticalScrollBar();
            if ( !bar )
                return false;
            // Gwenview scrolls three rows at once, what drives me crazy because you loose track on images
            // and waste time to find yourself back into context.
            if ( e->type() == QEvent::Wheel )
            {
                int step = bar->singleStep();
                bar->setSingleStep(step/3);
//                 list->setVerticalScrollMode( QAbstractItemView::ScrollPerPixel );
                QCoreApplication::sendEvent(bar, e); // tell the scrollbar to do this ;-P
                bar->setSingleStep(step);
                return true; // eat it
            }
            // another funny crap is that it scrolls around whenever you start looking a an image
            // so we save the position and re-set it when the thumbview comes back
            // i'd  make a bugreport but it's been like this since KDE4 times, this cannot be a bug but
            // must be stubborness :-(
            if ( e->type() == QEvent::Hide )
                gwenview_position = bar->value();
            else if ( gwenview_position )
            {
                bar->setValue( gwenview_position );
                connect( bar, SIGNAL(valueChanged(int)), this, SLOT(fixGwenviewPosition()) );
            }
        }
        return false;
    }

#ifdef Q_WS_X11
    if (e->type() == QEvent::WindowStateChange)
    {
        if (config.suspendFullscreenPlayers)
        if (*appType == SMPlayer || *appType == VLC || *appType == Dragon)
        if (QWidget *w = qobject_cast<QWidget*>(o))
        if (w->isWindow())
        {
            if (w->windowState() & Qt::WindowFullScreen)
                XProperty::setAtom( w->winId(), XProperty::blockCompositing );
            else if (static_cast<QWindowStateChangeEvent*>(e)->oldState() & Qt::WindowFullScreen)
                XProperty::remove(w->winId(), XProperty::blockCompositing);
        }
    }
#endif

    if (e->type() == QEvent::Show)
    {
        FILTER_EVENTS(o);
        return false;
    }  // >-)

    return false;
}

void
Hacks::fixGwenviewPosition()
{
    QScrollBar *bar = qobject_cast<QScrollBar*>(sender());
    if ( !bar )
        return;
    disconnect( bar, SIGNAL(valueChanged(int)), this, SLOT(fixGwenviewPosition()) );
    bar->setValue( gwenview_position );
}

void
Hacks::toggleToolBarLock()
{
    if (lockToggleBar)
        lockToggleBar->setMovable(!lockToggleBar->isMovable());
    lockToggleBar = 0;
}

bool
Hacks::add(QWidget *w)
{
    if (!appType)
    {
        appType = new HackAppType((HackAppType)Unknown);
        if (qApp->inherits("GreeterApp")) // KDM segfaults on QCoreApplication::arguments()...
            *appType = KDM;
        else if (QCoreApplication::applicationName() == "gwenview")
            *appType = Gwenview;
        else if (QCoreApplication::applicationName() == "dragonplayer")
            *appType = Dragon;
        else if (!QCoreApplication::arguments().isEmpty() &&
                 QCoreApplication::arguments().at(0).endsWith("smplayer"))
            *appType = SMPlayer;
        else if (!QCoreApplication::arguments().isEmpty() &&
            QCoreApplication::arguments().at(0).endsWith("vlc"))
            *appType = VLC;
    }

    if (config.suspendFullscreenPlayers)
    if (*appType == SMPlayer || *appType == VLC || *appType == Dragon)
    if (w->isWindow())
    {
        ENSURE_INSTANCE;
        w->removeEventFilter(bespinHacks); // just to be sure
        w->installEventFilter(bespinHacks);
    }

    if (config.messages && qobject_cast<QMessageBox*>(w))
    {
        ENSURE_INSTANCE;
        w->setWindowFlags ( Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint | Qt::FramelessWindowHint);
        w->removeEventFilter(bespinHacks); // just to be sure
        w->installEventFilter(bespinHacks);
        return true;
    }

    if (config.lockToolBars && qobject_cast<QToolBar*>(w) && !w->inherits("KToolBar"))
    {
        ENSURE_INSTANCE;
        if (!lockToggleMenu)
        {
            lockToggleMenu = new QMenu();
            lockToggleAction = lockToggleMenu->addAction( "Lock Toolbar Position", bespinHacks, SLOT(toggleToolBarLock()) );
            lockToggleAction->setCheckable(true);
        }
        static_cast<QToolBar*>(w)->setMovable(false);
        FILTER_EVENTS(w);
    }

    if ( *appType == Gwenview && config.fixGwenview )
    if ( QAbstractScrollArea *area = qobject_cast<QAbstractScrollArea*>(w) )
    if ( area->inherits("Gwenview::ThumbnailView") )
    {
        ENSURE_INSTANCE;
        FILTER_EVENTS(area->viewport());
    }

    if ( w->objectName() == "qt_scrollarea_viewport" )

    if (config.KHTMLView)
    if (QFrame *frame = qobject_cast<QFrame*>(w))
    if (frame->inherits("KHTMLView"))
    {
        frame->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
        return true;
    }

    if ( config.killThrobber )
    if ( QMenuBar *mbar = qobject_cast<QMenuBar*>(w->parentWidget()) )
    if ( w->inherits("KAnimatedButton") )
    {   // this is konquerors throbber...
        w->hide();
        w->setParent(mbar->parentWidget());
        mbar->setCornerWidget(NULL);
        return true;
    }

    if (isWindowDragWidget(w))
    {
        ENSURE_INSTANCE;
        FILTER_EVENTS(w);
        return true;
    }
#if 0
    ENSURE_INSTANCE;
    FILTER_EVENTS(w);
#endif

//    if (config.hack.konsole)
//    if (w->inherits("Konsole::TerminalDisplay")) {
//       w->setAttribute(Qt::WA_StyledBackground);
//       w->setAttribute(Qt::WA_MacBrushedMetal);
//       return true;
//    }
    return false;
}

void
Hacks::remove(QWidget *w)
{
    w->removeEventFilter(bespinHacks);
    if (w->inherits("KHTMLView"))
        static_cast<QFrame*>(w)->setFrameStyle(QFrame::NoFrame);
}

void
Hacks::releaseApp()
{
    bespinHacks->deleteLater(); bespinHacks = 0L;
    lockToggleMenu->deleteLater(); lockToggleMenu = 0L;
}

#undef FILTER_EVENTS
#undef ENSURE_INSTANCE
