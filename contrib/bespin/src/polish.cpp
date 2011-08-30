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

#include <QAbstractItemView>
// #include <QAbstractScrollArea>
#include <QAbstractSlider>
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDockWidget>
#include <QHeaderView>
// #include <QLabel>
#include <QLayout>
#include <QLCDNumber>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>
#include <QToolBar>
#include <QToolTip>
#include <QTreeView>
#include <QWizard>

#include <QtDebug>

#include <qmath.h>

#include "blib/colors.h"
#include "blib/FX.h"
#include "blib/shadows.h"

#ifdef Q_WS_X11
#ifdef BESPIN_MACMENU
#include "macmenu.h"
#endif
#include "blib/xproperty.h"
#endif

#include "splitterproxy.h"
#include "visualframe.h"
#include "hacks.h"
#include "bespin.h"

#include "animator/hover.h"
#include "animator/aprogress.h"
#include "animator/tab.h"

#include "makros.h"
#undef CCOLOR
#undef FCOLOR
#define CCOLOR(_TYPE_, _FG_) PAL.color(QPalette::Active, Style::config._TYPE_##_role[_FG_])
#define FCOLOR(_TYPE_) PAL.color(QPalette::Active, QPalette::_TYPE_)
#define FILTER_EVENTS(_WIDGET_) { _WIDGET_->removeEventFilter(this); _WIDGET_->installEventFilter(this); } // skip semicolon

#define BESPIN_MOUSE_DEBUG 0
#define I_AM_THE_ROB 0

using namespace Bespin;

Hacks::Config Hacks::config;

static inline void
setBoldFont(QWidget *w, bool bold = true)
{
    if (w->font().pointSize() < 1)
        return;
    QFont fnt = w->font();
    fnt.setBold(bold);
    w->setFont(fnt);
}

void Style::polish ( QApplication * app, bool initVFrame )
{
    if (initVFrame)
        VisualFrame::setStyle(this);
    QPalette pal = app->palette();
    polish(pal);
    QPalette *opal = originalPalette;
    originalPalette = 0; // so our eventfilter won't react on this... ;-P
    app->setPalette(pal);
    originalPalette = opal;
    disconnect ( app, SIGNAL( focusChanged(QWidget*, QWidget*) ), this, SLOT( focusWidgetChanged(QWidget*, QWidget*)) );
    connect ( app, SIGNAL( focusChanged(QWidget*, QWidget*) ), this, SLOT( focusWidgetChanged(QWidget*, QWidget*)) );
}

#define _SHIFTCOLOR_(clr) clr = QColor(CLAMP(clr.red()-10,0,255),CLAMP(clr.green()-10,0,255),CLAMP(clr.blue()-10,0,255))

#undef PAL
#define PAL pal

void Style::polish( QPalette &pal, bool onInit )
{
    QColor c = pal.color(QPalette::Active, QPalette::Window);

    if (config.bg.mode > Plain)
    {
        int h,s,v,a;
        c.getHsv(&h,&s,&v,&a);
        if (v < config.bg.minValue) // very dark colors won't make nice backgrounds ;)
            c.setHsv(h, s, config.bg.minValue, a);
// #if BESPIN_ARGB_WINDOWS
//         c.setAlpha(config.bg.opacity);
// #endif
    }
    pal.setColor( QPalette::Window, c );

    // AlternateBase
    pal.setColor(QPalette::AlternateBase, Colors::mid(pal.color(QPalette::Active, QPalette::Base),
                                                      pal.color(QPalette::Active, config.view.shadeRole),
                                                      100,config.view.shadeLevel));
    int h,s,v;
    // highlight colors
    h = qGray(pal.color(QPalette::Active, QPalette::Highlight).rgb());
//     pal.setColor(QPalette::Inactive, QPalette::Highlight, Colors::mid(QColor(h,h,h), pal.color(QPalette::Active, QPalette::Highlight), 3, 1));
    pal.setColor(QPalette::Inactive, QPalette::Highlight, QColor(h,h,h));
    pal.setColor(QPalette::Disabled, QPalette::Highlight, Colors::mid(pal.color(QPalette::Inactive, QPalette::Highlight), pal.color(QPalette::Active, QPalette::HighlightedText), 3, 1));

    // Link colors can not be set through qtconfig - and the colors suck
    QColor link = pal.color(QPalette::Active, QPalette::Highlight);
    const int vwt = Colors::value(pal.color(QPalette::Active, QPalette::Window));
    const int vt = Colors::value(pal.color(QPalette::Active, QPalette::Base));
    link.getHsv(&h,&s,&v);
    s = sqrt(s/255.0)*255.0;

    if (vwt > 128 && vt > 128)
        v = 3*v/4;
    else if (vwt < 128 && vt < 128)
        v = qMin(255, 7*v/6);
    link.setHsv(h, s, v);

    pal.setColor(QPalette::Link, link);

    link = Colors::mid(link, Colors::mid(pal.color(QPalette::Active, QPalette::Text),
                                         pal.color(QPalette::Active, QPalette::WindowText)), 4, 1);
                                         pal.setColor(QPalette::LinkVisited, link);

    if (onInit)
    {
#if 1 // while working as expected, it will "break" the colors in the mail client, where opera apparently paints
      // WindowText on Base - auauauauaaa!
        if ( appType == Opera )
        {   // WORKAROUND for opera which uses QPalette::Text to paint the UI foreground
            // (i.e. WindowText, ButtonText & -sometimes- "even" Text...)
            // there isn't much we can do about since opera paints text by itself, so we merge colors
            // and just hope for the best :-(
            QColor c = pal.color(QPalette::Active, QPalette::Text);
            if ( Colors::haveContrast(c, pal.color(QPalette::Active, QPalette::Base)) )
            {
                if ( !Colors::haveContrast(c, pal.color(QPalette::Active, QPalette::Window)) )
                    c = Colors::mid(c, pal.color(QPalette::Active, QPalette::WindowText) );
                if ( !Colors::haveContrast(c, pal.color(QPalette::Active, QPalette::Button)) )
                    c = Colors::mid(c, pal.color(QPalette::Active, QPalette::ButtonText) );
                if ( c != pal.color(QPalette::Active, QPalette::Text) )
                {

                    qWarning("Bespin warning:\nOpera uses the wrong color to paint UI text and there's no contrast to the background\n"
                             "Merging foreground colors - sorry :-(");
                    pal.setColor( QPalette::Text, c );
                }
            }
        }
#endif
        // dark, light & etc are tinted... no good:
        pal.setColor(QPalette::Dark, QColor(70,70,70));
        pal.setColor(QPalette::Mid, QColor(100,100,100));
        pal.setColor(QPalette::Midlight, QColor(220,220,220));
        pal.setColor(QPalette::Light, QColor(240,240,240));

        // tooltip (NOTICE not configurable by qtconfig, kde can, let's see what we're gonna do on this...)
        pal.setColor(QPalette::ToolTipBase, pal.color(QPalette::Active, config.bg.tooltip_role[Bg]));
        pal.setColor(QPalette::ToolTipText, pal.color(QPalette::Active, config.bg.tooltip_role[Fg]));
    }

    // inactive palette
    if (config.fadeInactive)
    { // fade out inactive foreground and highlight colors...
        pal.setColor(QPalette::Inactive, QPalette::WindowText,
                     Colors::mid(pal.color(QPalette::Active, QPalette::Window), pal.color(QPalette::Active, QPalette::WindowText), 1,4));
        pal.setColor(QPalette::Inactive, QPalette::ButtonText,
                     Colors::mid(pal.color(QPalette::Active, QPalette::Button), pal.color(QPalette::Active, QPalette::ButtonText), 1,4));
        pal.setColor(QPalette::Inactive, QPalette::Text,
                     Colors::mid(pal.color(QPalette::Active, QPalette::Base), pal.color(QPalette::Active, QPalette::Text), 1,4));
    }

    // fade disabled palette
    pal.setColor(QPalette::Disabled, QPalette::WindowText,
                 Colors::mid(pal.color(QPalette::Active, QPalette::Window), pal.color(QPalette::Active, QPalette::WindowText),2,1));
    pal.setColor(QPalette::Disabled, QPalette::Base,
                 Colors::mid(pal.color(QPalette::Active, QPalette::Window), pal.color(QPalette::Active, QPalette::Base),1,2));
    pal.setColor(QPalette::Disabled, QPalette::Text,
                 Colors::mid(pal.color(QPalette::Active, QPalette::Base), pal.color(QPalette::Active, QPalette::Text)));
    pal.setColor(QPalette::Disabled, QPalette::AlternateBase,
                 Colors::mid(pal.color(QPalette::Disabled, QPalette::Base), pal.color(QPalette::Disabled, QPalette::Text),15,1));

    // more on tooltips... (we force some colors...)
    if (!onInit)
        return;

    QPalette toolPal = QToolTip::palette();
    const QColor bg = pal.color(config.bg.tooltip_role[Bg]);
    const QColor fg = pal.color(config.bg.tooltip_role[Fg]);
    toolPal.setColor(QPalette::Window, bg);
    toolPal.setColor(QPalette::WindowText, fg);
    toolPal.setColor(QPalette::Base, bg);
    toolPal.setColor(QPalette::Text, fg);
    toolPal.setColor(QPalette::Button, bg);
    toolPal.setColor(QPalette::ButtonText, fg);
    toolPal.setColor(QPalette::Highlight, fg); // sic!
    toolPal.setColor(QPalette::HighlightedText, bg); // sic!
    toolPal.setColor(QPalette::ToolTipBase, bg);
    toolPal.setColor(QPalette::ToolTipText, fg);
    QToolTip::setPalette(toolPal);


#ifdef Q_WS_X11
    if (appType == GTK)
        setupDecoFor(NULL, pal, config.bg.mode, GRAD(kwin));
#endif
}


#if 0
static QMenuBar *
bar4popup(QMenu *menu)
{
    if (!menu->menuAction())
        return 0;
    if (menu->menuAction()->associatedWidgets().isEmpty())
        return 0;
    foreach (QWidget *w, menu->menuAction()->associatedWidgets())
        if (qobject_cast<QMenuBar*>(w))
            return static_cast<QMenuBar *>(w);
    return 0;
}
#endif


inline static void
polishGTK(QWidget * widget, const Config &config)
{
    enum MyRole{Bg = Style::Bg, Fg = Style::Fg};
    QColor c1, c2, c3, c4;
    if (widget->objectName() == "QPushButton" ||
        widget->objectName() == "QComboBox" ||
        widget->objectName() == "QCheckBox" ||
        widget->objectName() == "QRadioButton" )
    {
        QPalette pal = widget->palette();
        c1 = CCOLOR(btn.std, Bg);
        c2 = CCOLOR(btn.active, Bg);
        c3 = CCOLOR(btn.std, Fg);
        c4 = CCOLOR(btn.active, Fg);

        pal.setColor(QPalette::Disabled, QPalette::Button, Colors::mid(Qt::black, FCOLOR(Window),5,100));
        pal.setColor(QPalette::Disabled, QPalette::ButtonText, Colors::mid(FCOLOR(Window), FCOLOR(WindowText),3,1));

        pal.setColor(QPalette::Inactive, QPalette::Button, c1);
        pal.setColor(QPalette::Active, QPalette::Button, c2);
        pal.setColor(QPalette::Inactive, QPalette::ButtonText, c3);
        pal.setColor(QPalette::Active, QPalette::ButtonText, config.btn.backLightHover ? c3 : c4);

        widget->setPalette(pal);
    }

    if (widget->objectName() == "QTabWidget" ||
        widget->objectName() == "QTabBar")
    {
        QPalette pal = widget->palette();
        c1 = CCOLOR(tab.std, Bg);
        c2 = CCOLOR(tab.active, Bg);
        c3 = CCOLOR(tab.std, Fg);
        c4 = CCOLOR(tab.active, Fg);

        pal.setColor(QPalette::Disabled, QPalette::WindowText, Colors::mid(c1, c3, 3, 1));
        pal.setColor(QPalette::Inactive, QPalette::Window, c1);
        pal.setColor(QPalette::Active, QPalette::Window, c2);
        pal.setColor(QPalette::Inactive, QPalette::WindowText, c3);
        pal.setColor(QPalette::Active, QPalette::WindowText, c4);
        widget->setPalette(pal);
    }

    if (widget->objectName() == "QMenuBar" )
    {
        QPalette pal = widget->palette();
        c1 = Colors::mid(FCOLOR(Window), CCOLOR(UNO._, Bg),1,6);
        c2 = CCOLOR(menu.active, Bg);
        c3 = CCOLOR(UNO._, Fg);
        c4 = CCOLOR(menu.active, Fg);

        pal.setColor(QPalette::Inactive, QPalette::Window, c1);
        pal.setColor(QPalette::Active, QPalette::Window, c2);
        pal.setColor(QPalette::Inactive, QPalette::WindowText, c3);
        pal.setColor(QPalette::Active, QPalette::WindowText, c4);
        widget->setPalette(pal);
    }

    if (widget->objectName() == "QMenu" )
    {
        QPalette pal = widget->palette();
        c1 = CCOLOR(menu.std, Bg);
        c2 = CCOLOR(menu.active, Bg);
        c3 = CCOLOR(menu.std, Fg);
        c4 = CCOLOR(menu.active, Fg);

        pal.setColor(QPalette::Inactive, QPalette::Window, c1);
        pal.setColor(QPalette::Active, QPalette::Window, c2);
        pal.setColor(QPalette::Inactive, QPalette::WindowText, c3);
        pal.setColor(QPalette::Active, QPalette::WindowText, c4);
        widget->setPalette(pal);
    }
}

static QAction *dockLocker = 0;

void
Style::polish( QWidget * widget )
{
    // GTK-Qt gets a special handling - see above
    if (appType == GTK)
    {
        polishGTK(widget, config);
        return;
    }

    // !!! protect against polishing /QObject/ attempts! (this REALLY happens from time to time...)
    if (!widget)
        return;

//     if (widget->inherits("QGraphicsView"))
//         qDebug() << "BESPIN" << widget;
#if BESPIN_MOUSE_DEBUG
    FILTER_EVENTS(widget);
#endif

    // NONONONONO!!!!! ;)
    if (qobject_cast<VisualFramePart*>(widget))
        return;

    // apply any user selected hacks
    Hacks::add(widget);

    //BEGIN Window handling                                                                        -
    if ( widget->isWindow() &&
//          widget->testAttribute(Qt::WA_WState_Created) &&
//          widget->internalWinId() &&
            !(widget->inherits("QSplashScreen") || widget->inherits("KScreenSaver")
            || widget->objectName() == "decoration widget" /*|| widget->inherits("QGLWidget")*/ ) )
    {
//         QPalette pal = widget->palette();
        /// this is dangerous! e.g. applying to QDesktopWidget leads to infinite recursion...
        /// also doesn't work bgs get transparent and applying this to everything causes funny sideeffects...
        if ( widget->windowType() == Qt::ToolTip)
        {
            if (widget->inherits("QTipLabel") || widget->inherits("KToolTipWindow"))
            {
                if (config.menu.round && !serverSupportsShadows())
                    FILTER_EVENTS(widget)
                if ( !widget->testAttribute(Qt::WA_TranslucentBackground) && config.menu.round && FX::compositingActive() )
                    widget->setAttribute(Qt::WA_TranslucentBackground);
                widget->setProperty("BespinWindowHints", Shadowed|(config.menu.round?Rounded:0));
                Bespin::Shadows::manage(widget);
            }
        }
        else if ( widget->windowType() == Qt::Popup)
        {
            if ( widget->inherits("QComboBoxPrivateContainer") )
                Bespin::Shadows::manage(widget);
        }
        else if ( widget->testAttribute(Qt::WA_X11NetWmWindowTypeDND) && FX::compositingActive() )
        {
            widget->setAttribute(Qt::WA_TranslucentBackground);
            widget->clearMask();
        }
#if BESPIN_ARGB_WINDOWS
        else if (!(  config.bg.opacity == 0xff || // opaque
                widget->windowType() == Qt::Desktop || // makes no sense + QDesktopWidget is often misused
                widget->testAttribute(Qt::WA_X11NetWmWindowTypeDesktop) || // makes no sense
                widget->testAttribute(Qt::WA_TranslucentBackground) ||
                widget->testAttribute(Qt::WA_NoSystemBackground) ||
                widget->testAttribute(Qt::WA_OpaquePaintEvent) ) )
        {
#if 0
            bool skip = false;
            QList<QWidget*> kids = widget->findChildren<QWidget*>();
            QList<QWidget*>::const_iterator kid = kids.constBegin();
            while ( kid != kids.constEnd() )
            {
                if ( (*kid)->inherits("QX11EmbedContainer") || (*kid)->inherits("QX11EmbedWidget") ||
                    (*kid)->inherits("Phonon::VideoWidget") || (*kid)->inherits("KSWidget") ||
                    (*kid)->inherits("MplayerWindow") )
                {
                    skip = true;
                    break;
                }
                ++kid;
            }
            if (!skip)
#endif
            {
                QIcon icn = widget->windowIcon();
                widget->setAttribute(Qt::WA_TranslucentBackground);
                widget->setWindowIcon(icn);
                // WORKAROUND: somehow the window gets repositioned to <1,<1 and thus always appears in the upper left corner
                // we just move it faaaaar away so kwin will take back control and apply smart placement or whatever
                if (!widget->isVisible())
                    widget->move(10000,10000);
            }
        }
#endif
        if (config.bg.glassy)
            widget->setAttribute(Qt::WA_MacBrushedMetal);


        if ( config.bg.mode > Plain || (config.UNO.toolbar && !config.UNO.sunken) ||
             config.bg.opacity != 0xff || config.bg.ringOverlay || widget->testAttribute(Qt::WA_MacBrushedMetal) )
        {
            if (config.bg.opacity != 0xff)
                FILTER_EVENTS(widget);
            widget->setAttribute(Qt::WA_StyledBackground);
        }
        if ( QMainWindow *mw = qobject_cast<QMainWindow*>(widget) )
        {
#if QT_VERSION >= 0x040500
            if ( appType == Dolphin )
                mw->setDockOptions(mw->dockOptions()|QMainWindow::VerticalTabs);
//             mw->setTabPosition ( Qt::LeftDockWidgetArea|Qt::RightDockWidgetArea, QTabWidget::North );
#endif
            if ( Hacks::config.lockDocks )
            {
                if ( !dockLocker )
                {
                    dockLocker = new QAction( "Locked Dock Positions", qApp );
                    dockLocker->setShortcutContext( Qt::ApplicationShortcut );
                    dockLocker->setShortcuts( QList<QKeySequence>() << QKeySequence("Ctrl+Alt+D") );
                    dockLocker->setEnabled( true );
                    dockLocker->setCheckable( true );
                    dockLocker->setChecked( true );
                    connect ( dockLocker, SIGNAL(toggled(bool)), SLOT(unlockDocks(bool)) );
                }
                widget->addAction( dockLocker );
            }
//             if (!config.drawSplitters)
                SplitterProxy::manage(widget);
        }
        else if ( QWizard *wiz = qobject_cast<QWizard*>(widget) )
        {
            if (config.macStyle && wiz->pixmap(QWizard::BackgroundPixmap).isNull())
            {
                if (!wiz->pixmap(QWizard::WatermarkPixmap).isNull())
                    wiz->setPixmap( QWizard::BackgroundPixmap, wiz->pixmap(QWizard::WatermarkPixmap) );
                else
                {
                    QPixmap pix(468,128);
                    pix.fill(Qt::transparent);
                    QRect r(0,0,128,128);
                    QPainter p(&pix);
                    QColor c = wiz->palette().color(wiz->foregroundRole());
                    c.setAlpha(24);
                    p.setBrush(c);
                    p.setPen(Qt::NoPen);
                    Navi::Direction dir = wiz->layoutDirection() == Qt::LeftToRight ? Navi::E : Navi::W;
                    Style::drawArrow(dir, r, &p);
                    r.translate(170,0); Style::drawArrow(dir, r, &p);
                    r.translate(170,0); Style::drawArrow(dir, r, &p);
                    p.end();
                    wiz->setPixmap( QWizard::BackgroundPixmap, pix );
                }
            }
        }
        //BEGIN Popup menu handling                                                                -
        if (QMenu *menu = qobject_cast<QMenu *>(widget))
        {
            if (config.menu.glassy)
            {   // glass mode popups
                menu->setAttribute(Qt::WA_MacBrushedMetal);
                menu->setAttribute(Qt::WA_StyledBackground);
            }
            else
                menu->setAttribute(Qt::WA_MacBrushedMetal, false);
            // opacity
#if BESPIN_ARGB_WINDOWS
            if ( !menu->testAttribute(Qt::WA_TranslucentBackground) &&
                (config.menu.opacity != 0xff || (config.menu.round && FX::compositingActive())) )
            {
                menu->setAttribute(Qt::WA_TranslucentBackground);
                menu->setAttribute(Qt::WA_StyledBackground);
                menu->setAutoFillBackground(false);
            }
            else
#endif
                menu->setAutoFillBackground(true);
            // color swapping
            menu->setBackgroundRole ( config.menu.std_role[Bg] );
            menu->setForegroundRole ( config.menu.std_role[Fg] );
            if (config.menu.boldText)
                setBoldFont(menu);

            if (appType == Plasma) // GNARF!
                menu->setWindowFlags( menu->windowFlags()|Qt::Popup);

            menu->setProperty("BespinWindowHints", Shadowed|(config.menu.round?Rounded:0));
            Bespin::Shadows::manage(menu);

            // eventfiltering to reposition MDI windows, shaping, shadows, paint ARGB bg and correct distance to menubars
            FILTER_EVENTS(menu);
#if 0
            /// NOTE this was intended to be for some menu mock from nuno where the menu
            /// reaches kinda ribbon-like into the bar
            /// i'll keep it to remind myself and in case i get it to work one day ;-)
            if (bar4popup(menu))
            {
                QAction *action = new QAction( menu->menuAction()->iconText(), menu );
                connect (action, SIGNAL(triggered(bool)), menu, SLOT(hide()));
                menu->insertAction(menu->actions().at(0), action);
            }
#endif
        }
        //END Popup menu handling                                                                  -
        /// WORKAROUND Qt color bug, uses daddies palette and FGrole, but TooltipBase as background
        else if (widget->inherits("QWhatsThat"))
        {
//             FILTER_EVENTS(widget); // IT - LOOKS - SHIT - !
            widget->setPalette(QToolTip::palette()); // so this is Qt bug WORKAROUND
//             widget->setProperty("BespinWindowHints", Shadowed);
//             Bespin::Shadows::set(widget->winId(), Bespin::Shadows::Small);
        }
        else
        {
            // talk to kwin about colors, gradients, etc.
            Qt::WindowFlags ignore =    Qt::Sheet | Qt::Drawer | Qt::Popup | Qt::ToolTip |
                                        Qt::SplashScreen | Qt::Desktop |
                                        Qt::X11BypassWindowManagerHint;// | Qt::FramelessWindowHint; <- could easily change mind...?!
            ignore &= ~Qt::Dialog; // erase dialog, it's in drawer et al. but takes away window as well
            // this can be expensive, so avoid for popups, combodrops etc.
            if (!(widget->windowFlags() & ignore))
            {
                if (widget->isVisible())
                    setupDecoFor(widget, widget->palette(), config.bg.mode, GRAD(kwin));
                FILTER_EVENTS(widget); // catch show event and palette changes for deco
            }
        }
    }
    //END Window handling                                                                          -

    //BEGIN Frames                                                                                 -
    if ( QFrame *frame = qobject_cast<QFrame *>(widget) )
    {
        if ( !frame->isWindow() )
        {
#if 0 // i want them centered, but titlewidget fights back, and it's not worth the eventfilter monitor
            if (QLabel *label = qobject_cast<QLabel*>(frame))
            {   // i want them center aligned
                if (label->parentWidget() && label->parentWidget()->parentWidget() &&
                    label->parentWidget()->parentWidget()->inherits("KTitleWidget"))
                    label->setAlignment(Qt::AlignCenter);
            } else
#endif
            // sunken looks soo much nicer ;)
            if (frame->parentWidget() && frame->parentWidget()->inherits("KTitleWidget"))
            {
                if (config.bg.mode == Scanlines)
                    frame->setFrameShadow(QFrame::Sunken);
                else
                {
                    frame->setAutoFillBackground(false);
                    frame->setBackgroundRole(QPalette::Window);
                    frame->setForegroundRole(QPalette::WindowText);
                }
            }
            else if (frame->frameShape() != QFrame::NoFrame )
            {
#if  QT_VERSION < 0x040500 // 4.5 has a CE_ for this =)
                // Kill ugly line look (we paint our styled v and h lines instead ;)
                if (frame->frameShape() == QFrame::HLine || frame->frameShape() == QFrame::VLine)
                    FILTER_EVENTS(widget);
                else if (frame->frameShape() != QFrame::StyledPanel)
                {   // Kill ugly winblows frames... (qShadeBlablabla stuff)
                    if ( frame->frameShape() == QFrame::Box )
                        frame->setFrameShadow( QFrame::Plain );
                    frame->setFrameShape(QFrame::StyledPanel);
                }
#endif
//             if ( appType == KMail && frame->frameStyle() == (QFrame::StyledPanel|QFrame::Sunken) && frame->inherits("KHBox") )
//                 frame->setFrameShadow(QFrame::Raised);
            }
        }
        // just saw they're niftier in skulpture -> had to do sth. ;-P
        if ( QLCDNumber *lcd = qobject_cast<QLCDNumber*>(frame) )
        {
            if (lcd->frameShape() != QFrame::NoFrame)
                lcd->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
            lcd->setSegmentStyle(QLCDNumber::Flat);
            lcd->setAutoFillBackground(true);
        }

        // scrollarea hovering
        QAbstractScrollArea *area = 0;
        if ((area = qobject_cast<QAbstractScrollArea*>(frame))
#ifdef QT3_SUPPORT
            || frame->inherits("Q3ScrollView")
#endif
            )
        {
            Animator::Hover::manage(frame);
            if (QAbstractItemView *itemView = qobject_cast<QAbstractItemView*>(frame) )
            {
                if ( Hacks::config.extendDolphinViews && itemView->parentWidget() &&
                     QString(itemView->metaObject()->className()).startsWith("Dolphin") )
                {
                    if (QWidget *grampa = itemView->parentWidget()->parentWidget())
                    {
                        itemView->setFrameStyle(QFrame::NoFrame);
                        grampa->setBackgroundRole(QPalette::Base);
                        grampa->setForegroundRole(QPalette::Text);
                        QWidgetList kids = grampa->findChildren<QWidget*>();
                        foreach (QWidget *kid, kids)
                        {
                            kid->setBackgroundRole(QPalette::Base);
                            kid->setForegroundRole(QPalette::Text);
                            if ( kid->inherits("StatusBarMessageLabel") )
                            {   // hardcoded paint colors... bwuahahahaaaa :-(
                                QPalette pal = kid->palette();
                                pal.setColor( QPalette::WindowText, pal.color(QPalette::Text) );
                                kid->setPalette(pal);
                            }
                        }
                        grampa->setAutoFillBackground(true);
                        grampa->setContentsMargins(F(4),F(1),F(4),F(1));
                        FILTER_EVENTS(grampa);
                        int l,t,r,b;
                        grampa = grampa->window();
                        grampa->getContentsMargins(&l,&t,&r,&b);
                        grampa->setContentsMargins(l,t,r,qMax(b,F(3)));
                    }
                }
                if (QWidget *vp = itemView->viewport())
                {
                    if (!vp->autoFillBackground() || vp->palette().color(QPalette::Active, vp->backgroundRole()).alpha() < 128)
                    {
                        const bool solid = Hacks::config.opaqueAmarokViews || Hacks::config.opaqueDolphinViews ||
                                           (Hacks::config.opaquePlacesViews && itemView->inherits("KFilePlacesView"));
                        const bool alternate = !(appType == Amarok && itemView->inherits("Playlist::PrettyListView"));
                        fixViewPalette(itemView, solid, alternate);
                    }
                    else if (vp->backgroundRole() == QPalette::Window || vp->palette().color(vp->backgroundRole()) == itemView->palette().color(itemView->backgroundRole()))
                        fixViewPalette(itemView, 2, false);
                }
                if (appType == Amarok) // fix the palette anyway. amarok tries to reset it's slooww transparent one... gnagnagna
                    FILTER_EVENTS(itemView);

                if (itemView->inherits("KCategorizedView")) // fix scrolldistance...
                    FILTER_EVENTS(itemView);

#if QT_VERSION >= 0x040500
                itemView->viewport()->setAttribute(Qt::WA_Hover);
#endif
                if ( QTreeView* tv = qobject_cast<QTreeView*>(itemView) )
                {   // allow all treeviews to be animated! NOTICE: animation causes visual errors on non autofilling views...
                    if (Hacks::config.treeViews &&
                        tv->viewport()->autoFillBackground() &&
                        tv->viewport()->palette().color(tv->viewport()->backgroundRole()).alpha() > 200) // 255 would be perfect, though
                    tv->setAnimated(true);
//                     KMail::MainFolderView(0xa16fd68, name = )
                    if ( Hacks::config.fixKMailFolderList && tv->objectName() == "folderTree" )
                    {
                        fixViewPalette( tv, Hacks::config.opaqueDolphinViews ? 1 : 2, true );
                        tv->setHeaderHidden( true );
                        tv->setRootIsDecorated ( false );
                        tv->sortByColumn ( 0, Qt::AscendingOrder );
                        tv->setIconSize( QSize(22,22) );
                        tv->header()->setResizeMode( QHeaderView::Stretch );
                    }
                }
                else
                {   // Enable hover effects in listview, treeview hovering sucks, as the "tree" doesn't get an update
                    itemView->viewport()->setAttribute(Qt::WA_Hover);
                    if (widget->inherits("QHeaderView"))
                        widget->setAttribute(Qt::WA_Hover);
                }
            }
            // just <strike>broadsword</strike> gladius here - the stupid viewport should use the mouse...
            else  if (appType != Dolphin && area && area->viewport())
                area->viewport()->setAttribute(Qt::WA_NoMousePropagation);
#if 0 // does not work
            else if (appType == Amarok && widget->inherits("Context::ContextView"))
                FILTER_EVENTS(widget);
#endif
            // Dolphin Information panel still (again?) does this
            // *sigh* - this cannot be true. this CANNOT be true. this CAN NOT BE TRUE!
            if (area->viewport() && area->viewport()->autoFillBackground() && !area->viewport()->palette().color(area->viewport()->backgroundRole()).alpha() )
                area->viewport()->setAutoFillBackground(false);
        }

        /// Tab Transition animation,
        if (widget->inherits("QStackedWidget"))
            // NOTICE do NOT(!) apply this on tabs explicitly, as they contain a stack!
            Animator::Tab::manage(widget);
        else if (widget->inherits("KColorPatch"))
            widget->setAttribute(Qt::WA_NoMousePropagation);
        /// QToolBox handling - a shame they look that crap by default!
        else if (widget->inherits("QToolBox"))
        {   // get rid of QPalette::Button
            widget->setBackgroundRole(QPalette::Window);
            widget->setForegroundRole(QPalette::WindowText);
            if (widget->layout())
            {   // get rid of nasty indention
                widget->layout()->setMargin ( 0 );
                widget->layout()->setSpacing ( 0 );
            }
        }

        if ( !frame->isWindow() )
        {
            /// "Frame above Content" look, but ...
            if (isSpecialFrame(widget))
            {   // ...QTextEdit etc. can be handled more efficiently
                if (frame->lineWidth() == 1)
                    frame->setLineWidth(F(4)); // but must have enough indention
            }
            else if (!widget->inherits("KPIM::OverlayWidget"))
                VisualFrame::manage(frame);
        }
    }
    //END FRAMES                                                                                   -

    //BEGIN PUSHBUTTONS - hovering/animation                                                       -
    else if (qobject_cast<QAbstractButton*>(widget))
    {
//         widget->setBackgroundRole(config.btn.std_role[Bg]);
//         widget->setForegroundRole(config.btn.std_role[Fg]);
        widget->setAttribute(Qt::WA_Hover, false); // KHtml
        if (widget->inherits("QToolBoxButton") || IS_HTML_WIDGET )
            widget->setAttribute(Qt::WA_Hover); // KHtml
        else
        {
            if (QPushButton *pbtn = qobject_cast<QPushButton*>(widget))
            {
                if (widget->parentWidget() &&
                    widget->parentWidget()->inherits("KPIM::StatusbarProgressWidget"))
                    pbtn->setFlat(true);

                // HACK around "weird" original appearance ;-P
                // also see eventFilter
                if (pbtn->inherits("KUrlNavigatorButtonBase") || pbtn->inherits("BreadcrumbItemButton"))
                {
                    pbtn->setBackgroundRole(QPalette::Window);
                    pbtn->setForegroundRole(QPalette::Link);
                    QPalette pal = pbtn->palette();
                    pal.setColor(QPalette::Highlight, Qt::transparent);
                    pal.setColor(QPalette::HighlightedText, pal.color(QPalette::Active, QPalette::Window));
                    pbtn->setPalette(pal);
                    pbtn->setCursor(Qt::PointingHandCursor);
                    FILTER_EVENTS(pbtn);
                    widget->setAttribute(Qt::WA_Hover);
                }
            }
            else if (widget->inherits("QToolButton") &&
                // of course plasma needs - again - a WORKAROUND, we seem to be unable to use bg/fg-role, are we?
                !(appType == Plasma && widget->inherits("ToolButton")))
            {
                QPalette::ColorRole bg = QPalette::Window, fg = QPalette::WindowText;
                if (QWidget *dad = widget->parentWidget())
                {
                    bg = dad->backgroundRole();
                    fg = dad->foregroundRole();
                }
                widget->setBackgroundRole(bg);
                widget->setForegroundRole(fg);
            }
            if (!widget->testAttribute(Qt::WA_Hover))
                Animator::Hover::manage(widget);
        }

        // NOTICE WORKAROUND - this widget uses the style to paint the bg, but hardcodes the fg...
        // TODO: inform Joseph Wenninger <jowenn@kde.org> and really fix this
        // (fails all styles w/ Windowcolored ToolBtn and QPalette::ButtonText != QPalette::WindowText settings)
        if (widget->inherits("KMultiTabBarTab"))
        {
            QPalette pal = widget->palette();
            pal.setColor(QPalette::Active, QPalette::Button, pal.color(QPalette::Active, QPalette::Window));
            pal.setColor(QPalette::Inactive, QPalette::Button, pal.color(QPalette::Inactive, QPalette::Window));
            pal.setColor(QPalette::Disabled, QPalette::Button, pal.color(QPalette::Disabled, QPalette::Window));

            pal.setColor(QPalette::Active, QPalette::ButtonText, pal.color(QPalette::Active, QPalette::WindowText));
            pal.setColor(QPalette::Inactive, QPalette::ButtonText, pal.color(QPalette::Inactive, QPalette::WindowText));
            pal.setColor(QPalette::Disabled, QPalette::ButtonText, pal.color(QPalette::Disabled, QPalette::WindowText));
            widget->setPalette(pal);
        }
    }

    //BEGIN COMBOBOXES - hovering/animation                                                        -
    else if (QComboBox *cb = qobject_cast<QComboBox*>(widget))
    {
        if (cb->view())
            cb->view()->setTextElideMode( Qt::ElideMiddle);

        if (cb->parentWidget() && cb->parentWidget()->inherits("KUrlNavigator"))
            cb->setIconSize(QSize(0,0));

        if (IS_HTML_WIDGET)
            widget->setAttribute(Qt::WA_Hover);
        else
            Animator::Hover::manage(widget);
    }
    //BEGIN SLIDERS / SCROLLBARS / SCROLLAREAS - hovering/animation                                -
    else if (qobject_cast<QAbstractSlider*>(widget))
    {
        FILTER_EVENTS(widget); // finish animation

        widget->setAttribute(Qt::WA_Hover);
        // NOTICE
        // QAbstractSlider::setAttribute(Qt::WA_OpaquePaintEvent) saves surprisinlgy little CPU
        // so that'd just gonna add more complexity for literally nothing...
        // ...as the slider is usually not bound to e.g. a "scrollarea"
//         if ( appType == Amarok && widget->inherits("VolumeDial") )
//         {   // OMG - i'm hacking myself =D
//             QPalette pal = widget->palette();
//             pal.setColor( QPalette::Highlight, pal.color( QPalette::Active, QPalette::WindowText ) );
//             widget->setPalette( pal );
//         }
        if (widget->inherits("QScrollBar"))
        {
            // TODO: find a general catch for the plasma problem
            if (appType == Plasma) // yes - i currently don't know how to detect those things otherwise
                widget->setAttribute(Qt::WA_OpaquePaintEvent, false);
            else
            {
                QWidget *dad = widget;
                while ((dad = dad->parentWidget()))
                {   // digg for a potential KHTMLView ancestor, making this a html input scroller
                    if (dad->inherits("KHTMLView"))
                    {   // NOTICE this slows down things as it triggers a repaint of the frame
                        widget->setAttribute(Qt::WA_OpaquePaintEvent, false);
                        // ...but this would re-enbale speed - just: how to get the proper palette
                        // what if there's a bg image?
                        // TODO how's css/khtml policy on applying colors?
    //                     widget->setAutoFillBackground ( true );
    //                     widget->setBackgroundRole ( QPalette::Base ); // QPalette::Window looks wrong
    //                     widget->setForegroundRole ( QPalette::Text );
                        break;
                    }
                }
            }

            /// Scrollarea hovering - yes, this is /NOT/ redundant to the one above!
            if (QWidget *area = widget->parentWidget())
            {
                if ((area = area->parentWidget())) // sic!
                {
                    if (qobject_cast<QAbstractScrollArea*>(area))
                        area = 0; // this is handled for QAbstractScrollArea, but...
                    else // Konsole, Kate, etc. need a special handling!
                        area = widget->parentWidget();
                }
                if (area)
                    Animator::Hover::manage(area, true);
            }
        }
    }

    //BEGIN PROGRESSBARS - hover/animation and bold font                                           -
    else if (widget->inherits("QProgressBar"))
    {
        widget->setAttribute(Qt::WA_Hover);
        setBoldFont(widget);
        Animator::Progress::manage(widget);
    }

#if QT_VERSION >= 0x040500
    else if ( widget->inherits( "QTabWidget" ) )
        FILTER_EVENTS(widget)
#endif

    //BEGIN Tab animation, painting override                                                       -
    else if (QTabBar *bar = qobject_cast<QTabBar *>(widget))
    {
        widget->setAttribute(Qt::WA_Hover);
        if (bar->drawBase())
        {
            widget->setBackgroundRole(config.tab.std_role[Bg]);
            widget->setForegroundRole(config.tab.std_role[Fg]);
        }
        // the eventfilter overtakes the widget painting to allow tabs ABOVE the tabbar
        FILTER_EVENTS(widget);
    }
    else if (Hacks::config.invertDolphinUrlBar && widget->inherits("KUrlNavigator"))
    {
        widget->setContentsMargins(F(4),0,F(1), 0);
        widget->setAutoFillBackground(false);
        FILTER_EVENTS(widget);
        QPalette p = widget->palette();
        QColor ch = p.color(QPalette::Window);
        p.setColor(QPalette::Window, p.color(QPalette::WindowText));
        p.setColor(QPalette::Base, p.color(QPalette::Window));
        p.setColor(QPalette::WindowText, ch);
        p.setColor(QPalette::Text, ch);
        ch = Colors::mid(p.color(QPalette::Highlight), ch, 4, 1);
        p.setColor(QPalette::Link, ch);
        p.setColor(QPalette::LinkVisited, ch);
        widget->setPalette(p);
        QList<QAbstractButton*> btns = widget->findChildren<QAbstractButton*>();
        foreach (QAbstractButton *btn, btns)
        {
//             KUrlDropDownButton, KUrlNavigatorButton, KProtocolCombo
            if ( btn->inherits("KUrlNavigatorPlacesSelector") )
                btn->setFixedSize(0,0);
//             if ( btn->inherits("KFilePlacesSelector") || btn->inherits("KUrlToggleButton") )
//                 btn->setIconSize(QSize(10,10));
        }
    }
    else if ( QDockWidget *dock = qobject_cast<QDockWidget*>(widget) )
    {
        if ( Hacks::config.lockDocks )
        {
            disconnect( dock, SIGNAL(dockLocationChanged(Qt::DockWidgetArea)), this, SLOT(dockLocationChanged(Qt::DockWidgetArea)) );
            connect( dock, SIGNAL(dockLocationChanged(Qt::DockWidgetArea)), this, SLOT(dockLocationChanged(Qt::DockWidgetArea)) );
        }
        dock->setContentsMargins(F(4),F(4),F(4),F(4));
        widget->setAttribute(Qt::WA_Hover);
        if ( config.menu.round )
            FILTER_EVENTS(dock); // shape
        Bespin::Shadows::manage(dock);
        if ( config.bg.docks.invert && dock->features() & (QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable) )
        {
            QPalette pal = dock->palette();
            QColor c = pal.color(QPalette::Window);
            pal.setColor(QPalette::Window, pal.color(QPalette::WindowText));
            pal.setColor(QPalette::WindowText, c);
            dock->setPalette(pal);
            dock->setAutoFillBackground(true);
        }
    }
    /// Menubars and toolbar default to QPalette::Button - looks crap and leads to flicker...?!
    else if (QMenuBar *mbar = qobject_cast<QMenuBar *>(widget))
    {
        mbar->setBackgroundRole(config.UNO.__role[Bg]);
        mbar->setForegroundRole(config.UNO.__role[Fg]);
        if (config.UNO.used)
        {
            widget->setAutoFillBackground(true);
            // catch resizes for gradient recalculation
            FILTER_EVENTS(mbar);
        }
#ifdef Q_WS_X11
#ifdef BESPIN_MACMENU
        if ( appType != KDevelop ) //&& !(appType == QtDesigner && mbar->inherits("QDesignerMenuBar")) )
            MacMenu::manage(mbar);
#endif
#endif
    }
    else if (widget->inherits("KFadeWidgetEffect"))
    {   // interfers with our animation, is slower and cannot handle non plain backgrounds
        // (unfortunately i cannot avoid the widget grabbing)
        // maybe ask ereslibre to query a stylehint for this?
        widget->hide();
        widget->installEventFilter(&eventKiller);
    }
    /// hover some leftover widgets
    else if (widget->inherits("QAbstractSpinBox") || widget->inherits("QSplitterHandle") ||
        widget->inherits("QWebView") || // to update the scrollbars
        widget->inherits("QWorkspaceTitleBar")
#ifdef QT3_SUPPORT
        || widget->inherits("Q3DockWindowResizeHandle")
#endif
            )
    {
//         if (!config.drawSplitters)
            SplitterProxy::manage(widget);
        widget->setAttribute(Qt::WA_Hover);
        if (widget->inherits("QWebView"))
            FILTER_EVENTS(widget);
    }
    else if (Hacks::config.konsoleScanlines &&widget->inherits("Konsole::TerminalDisplay"))
        FILTER_EVENTS(widget)
    // this is a WORKAROUND for amarok filebrowser, see above on itemviews...
    else if (widget->inherits("KDirOperator"))
    {
        if (widget->parentWidget() && widget->parentWidget()->inherits("FileBrowser"))
        {
            QPalette pal = widget->palette();
            pal.setColor(QPalette::Active, QPalette::Text, pal.color(QPalette::Active, QPalette::WindowText));
            pal.setColor(QPalette::Inactive, QPalette::Text, pal.color(QPalette::Inactive, QPalette::WindowText));
            pal.setColor(QPalette::Disabled, QPalette::Text, pal.color(QPalette::Disabled, QPalette::WindowText));
            widget->setPalette(pal);
        }
    }
#if 0
// #ifdef Q_WS_X11
    if ( config.bg.opacity != 0xff && /*widget->window() &&*/
        (widget->inherits("MplayerWindow") ||
         widget->inherits("KSWidget") ||
         widget->inherits("QX11EmbedContainer") ||
         widget->inherits("QX11EmbedWidget") ||
         widget->inherits("Phonon::VideoWidget")) )
    {
        bool vis = widget->isVisible();
        widget->setWindowFlags(Qt::Window);
        widget->show();
        printf("%s %s %d\n", widget->className(), widget->parentWidget()->className(), widget->winId());
        widget->setAttribute(Qt::WA_DontCreateNativeAncestors, widget->testAttribute(Qt::WA_DontCreateNativeAncestors));
        widget->setAttribute(Qt::WA_NativeWindow);
        widget->setAttribute(Qt::WA_TranslucentBackground, false);
        widget->setAttribute(Qt::WA_PaintOnScreen, true);
        widget->setAttribute(Qt::WA_NoSystemBackground, false);
        if (QWidget *window = widget->window())
        {
            qDebug() << "BESPIN, reverting" << widget << window;
            window->setAttribute(Qt::WA_TranslucentBackground, false);
            window->setAttribute(Qt::WA_NoSystemBackground, false);
        }
        QApplication::setColorSpec(QApplication::NormalColor);
    }
#endif

    bool isTopContainer = qobject_cast<QToolBar *>(widget);

    // Arora needs a separator between the buttons and the lineedit - looks megadull w/ shaped buttons otherwise :-(
    if ( appType == Arora && isTopContainer && widget->objectName() == "NavigationToolBar")
    {
        QAction *before = 0;
        QToolBar *bar = static_cast<QToolBar *>(widget);
        foreach ( QObject *o, bar->children() )
        {
            before = 0;
            if ( o->inherits("QWidgetAction") && bar->widgetForAction( (before = static_cast<QAction*>(o)) )->inherits("QSplitter") )
                break;
        }
        if ( before )
            bar->insertSeparator( before );
    }

    if ( isTopContainer )
    {   // catches show/resize events and manipulates fg/bg role
        if ( config.UNO.toolbar )
        {
            updateUno(static_cast<QToolBar *>(widget));
            FILTER_EVENTS(widget);
        }
#if I_AM_THE_ROB
        else if ( config.btn.tool.connected )
            FILTER_EVENTS(widget);
#endif
    }

#ifdef QT3_SUPPORT
    isTopContainer = isTopContainer || widget->inherits("Q3ToolBar");
#endif
    if (isTopContainer || qobject_cast<QToolBar*>(widget->parent()))
    {
        widget->setBackgroundRole(QPalette::Window);
        widget->setForegroundRole(QPalette::WindowText);
        if (!isTopContainer && widget->inherits("QToolBarHandle"))
            widget->setAttribute(Qt::WA_Hover);
    }

    /// this is for QToolBox kids - they're autofilled by default - what looks crap
    if (widget->autoFillBackground() && widget->parentWidget() &&
        ( widget->parentWidget()->objectName() == "qt_scrollarea_viewport" ) &&
        widget->parentWidget()->parentWidget() && //grampa
        qobject_cast<QAbstractScrollArea*>(widget->parentWidget()->parentWidget()) &&
        widget->parentWidget()->parentWidget()->parentWidget() && // grangrampa
        widget->parentWidget()->parentWidget()->parentWidget()->inherits("QToolBox") )
    {
        widget->parentWidget()->setAutoFillBackground(false);
        widget->setAutoFillBackground(false);
    }

    if (config.bg.blur && (widget->autoFillBackground() || widget->testAttribute(Qt::WA_OpaquePaintEvent)))
        FILTER_EVENTS(widget);

    /// KHtml css colors can easily get messed up, either because i'm unsure about what colors
    /// are set or KHtml does wrong OR (mainly) by html "designers"
    if (IS_HTML_WIDGET)
    {   // the eventfilter watches palette changes and ensures contrasted foregrounds...
        FILTER_EVENTS(widget);
        QEvent ev(QEvent::PaletteChange);
        eventFilter(widget, &ev);
    }

    if ( appType == KMail )
    {   // This no only has not been fixed in 4 years now, but there's even a new widget with this flag set: head -> desk!
        if (widget->inherits("KMail::MessageListView::Widget"))
            widget->setAutoFillBackground(false);
        else if ( widget->parentWidget() && widget->inherits("RecipientLine") )
            widget->parentWidget()->setAutoFillBackground(false);
    }

}
#undef PAL

void
Style::unpolish( QApplication *app )
{
    app->removeEventFilter(this);
    SplitterProxy::cleanUp();
    VisualFrame::setStyle(0L);
    app->setPalette(QPalette());
    Hacks::releaseApp();
    Gradients::wipe();
    disconnect ( app, SIGNAL( focusChanged(QWidget*, QWidget*) ), this, SLOT( focusWidgetChanged(QWidget*, QWidget*)) );
}

void
Style::unpolish( QWidget *widget )
{
    if (!widget)
        return;

    if (widget->isWindow() && widget->testAttribute(Qt::WA_WState_Created) && widget->internalWinId())
    {
//         if (config.bg.opacity != 0xff)
//         {
//             window->setAttribute(Qt::WA_TranslucentBackground, false);
//             window->setAttribute(Qt::WA_NoSystemBackground, false);
//         }
#ifdef Q_WS_X11
        XProperty::remove(widget->winId(), XProperty::winData);
        XProperty::remove(widget->winId(), XProperty::bgPics);
#endif
        if (qobject_cast<QMenu *>(widget))
            widget->clearMask();
    }

    if (qobject_cast<QAbstractButton*>(widget) || qobject_cast<QToolBar*>(widget) ||
        qobject_cast<QMenuBar*>(widget) || qobject_cast<QMenu*>(widget) ||
        widget->inherits("QToolBox"))
    {
        widget->setBackgroundRole(QPalette::Button);
        widget->setForegroundRole(QPalette::ButtonText);
    }
    if (QFrame *frame = qobject_cast<QFrame *>(widget))
        VisualFrame::release(frame);
#ifdef Q_WS_X11
#ifdef BESPIN_MACMENU
        if ( appType != KDevelop ) //&& !(appType == QtDesigner && mbar->inherits("QDesignerMenuBar")) )
            MacMenu::manage(mbar);
    if (QMenuBar *mbar = qobject_cast<QMenuBar *>(widget))
        MacMenu::release(mbar);
#endif
#endif

    Animator::Hover::release(widget);
    Animator::Progress::release(widget);
    Animator::Tab::release(widget);
    Hacks::remove(widget);

    widget->removeEventFilter(this);
}
#undef CCOLOR
#undef FCOLOR
#undef FILTER_EVENTS
