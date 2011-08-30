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

#include <QMainWindow>
#include <QToolBar>
#include <QToolButton>
#include "draw.h"
#include "animator/hover.h"
#include "blib/FX.h"

static int step = 0;
static bool connected = false;

bool
Style::hasMenuIndicator(const QStyleOptionToolButton *tb)
{
    // subcontrol requested?
    bool ret = (tb->subControls & SC_ToolButtonMenu) || (tb->features & QStyleOptionToolButton::Menu);

    // delayed menu?
    if (!ret)
        ret = (tb->features & (QStyleOptionToolButton::HasMenu | QStyleOptionToolButton::PopupDelay))
                           == (QStyleOptionToolButton::HasMenu | QStyleOptionToolButton::PopupDelay);
    return ret;
}

void
Style::drawToolBar(const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    // UNO bar
    if (config.UNO.toolbar && widget && widget->autoFillBackground())
    if ( const QToolBar *bar = qobject_cast<const QToolBar*>(widget) )
    if ( QMainWindow *mwin = qobject_cast<QMainWindow*>(bar->parentWidget()) )
    if ( mwin->toolBarArea(const_cast<QToolBar*>(bar)) == Qt::TopToolBarArea )
    {
        QVariant var = mwin->property("UnoHeight");
        int h = var.isValid() ? var.toInt() : 0;
        if (config.UNO.gradient)
        {
            if (h)
            {
                const QPixmap &fill = Gradients::pix(CCOLOR(UNO._, Bg), (h & 0xffffff), Qt::Vertical, config.UNO.gradient);
                painter->drawTiledPixmap(RECT, fill, QPoint(0,bar->geometry().y() + ((h>>24) & 0xff)));
            }
        }
        h = (h & 0xffffff) - ((h>>24) & 0xff);
        if (h == bar->geometry().bottom())
        {
            SAVE_PEN;
            painter->setPen(Colors::mid(CCOLOR(UNO._, Bg), config.UNO.sunken ? Qt::white : Qt::black, 6, 1));
            painter->drawLine(RECT.bottomLeft(), RECT.bottomRight());
            RESTORE_PEN;
        }
        if (config.UNO.sunken && !(config.UNO.title || bar->geometry().y()-1))
        {
            Tile::setShape(Tile::Top);
            shadows.sunken[false][false].render(RECT, painter);
            Tile::reset();
        }
        return;
    }
    
    // lighter scanline variant
    if (config.bg.mode == Scanlines  && config.bg.structure < 5)
        painter->drawTiledPixmap( RECT, Gradients::structure(FCOLOR(Window), true), RECT.topLeft() );
    // window
    else if (widget && widget->isWindow())
    {
        if (config.bg.mode != Scanlines)
            painter->fillRect(RECT, Gradients::pix(FCOLOR(Window), RECT.height(), Qt::Vertical));
        else
            painter->fillRect(RECT, Gradients::structure(FCOLOR(Window), false));
        drawWindowFrame(option, painter, widget);
    }
}

void
Style::drawToolButton(const QStyleOptionComplex *option, QPainter *painter, const QWidget *widget) const
{
    ASSURE_OPTION(toolbutton, ToolButton);
    OPT_SUNKEN OPT_ENABLED OPT_HOVER
    QStyleOption tool(*toolbutton);
    QWidget *daddy = widget ? widget->parentWidget() : 0L;
    const QPalette &pal = daddy ? daddy->palette() : PAL;

    // special handling for the tabbar scrollers ------------------------------
    if (toolbutton->features & QStyleOptionToolButton::Arrow && qobject_cast<QTabBar*>(daddy))
    {
        QColor bg = pal.color(config.tab.std_role[Bg]), fg = pal.color(config.tab.std_role[Fg]);
        Qt::Orientation o = Qt::Vertical;
        QRect r = RECT; int dy2 = 0;
        Tile::Position pos = Tile::Right;
        switch (toolbutton->arrowType)
        {
            case Qt::RightArrow:
                pos = Tile::Left; // fall through
            default:
            case Qt::LeftArrow:
                r.adjust(0, F(2), 0, -F(4));
                dy2 = F(2);
                break;
            case Qt::UpArrow:
                r.adjust(F(2), 0, -F(2), 0);
                pos = Tile::Bottom; o = Qt::Horizontal; break;
            case Qt::DownArrow:
                r.adjust(F(2), 0, -F(2), -F(2));
                dy2 = F(2);
                pos = Tile::Top; o = Qt::Horizontal; break;
        }
        Tile::setShape(Tile::Full & ~pos);
        masks.rect[true].render(r, painter, sunken ? Gradients::Sunken : Gradients::None, o, bg);
        Tile::setShape(Tile::Ring & ~pos);
        if (dy2)
            r.setBottom(r.bottom() + dy2);
        shadows.sunken[true][true].render(r, painter);
        Tile::reset();
        QPen oldPen = painter->pen();
        painter->setPen( !isEnabled ? Colors::mid(bg, fg, 3,2) : (hover ? FCOLOR(Highlight) : fg) );
        drawToolButtonLabel(option, painter, widget);
        painter->setPen(oldPen);
        return;
    } // --------------------------------------------------------------------

    QRect button = subControlRect(CC_ToolButton, toolbutton, SC_ToolButton, widget);
    State bflags = toolbutton->state;

    if ((bflags & State_AutoRaise) && !hover)
        bflags &= ~State_Raised;

    State mflags = bflags;

    if (toolbutton->activeSubControls & SC_ToolButton)
        bflags |= State_Sunken;

    hover = isEnabled && (bflags & (State_Sunken | State_On | State_Raised | State_HasFocus));

    step = Animator::Hover::step(widget);
    QToolBar *bar = 0;
    QMainWindow *mwin = 0;
    connected = config.btn.tool.connected && // we want them at all
                daddy && daddy->parentWidget() && (bar = qobject_cast<QToolBar*>(daddy)) && // daddy is a toolbar
                (mwin = qobject_cast<QMainWindow*>(daddy->parentWidget())) && // in a mainwindow
//                 mwin->toolBarArea(bar) != Qt::NoToolBarArea && // kills floaters...
                true;

    // frame around whole button
    if (connected || option->state & State_On)
    {
        QStyleOption tool(0);
        tool.palette = pal;
        tool.rect = RECT;
        tool.state = bflags;
        drawToolButtonShape(&tool, painter, widget);
    }

    if (!(bflags & State_Sunken) && hasMenuIndicator(toolbutton))
    {
        QRect menuarea = subControlRect(CC_ToolButton, toolbutton, SC_ToolButtonMenu, widget);
        QPen oldPen = painter->pen();
        painter->setPen(Colors::mid(pal.color(QPalette::Window), pal.color(QPalette::WindowText), 8-step, step+3));
        drawSolidArrow(Navi::S, menuarea, painter);
        painter->setPen(oldPen);
    }

   // label in the toolbutton area
   QStyleOptionToolButton label = *toolbutton;
   label.rect = button;
   label.palette = pal;
   drawToolButtonLabel(&label, painter, widget);
   step = 0;
   connected = false;
}

void
Style::drawToolButtonShape(const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    OPT_ENABLED

    QRect rect = RECT;
    if (connected)
    {
        QToolBar *tb = static_cast<QToolBar*>(widget->parentWidget()); // guaranteed by "connected", see above
        OPT_SUNKEN
        const bool round = (config.btn.tool.frame == 1);
        const bool sameRoles = config.btn.tool.std_role[Bg] == config.btn.tool.active_role[Bg];
        Gradients::Type gt = sunken ? Gradients::Sunken : GRAD(btn.tool);

        QPalette pal = PAL;
#undef PAL
#define PAL pal
        if (widget && widget->parentWidget())
            pal.setCurrentColorGroup(widget->parentWidget()->palette().currentColorGroup());
        else
            pal.setCurrentColorGroup(QPalette::Active);

        QColor c  = config.btn.tool.std_role[Bg] == QPalette::Window ? Colors::bg(pal, widget) : CCOLOR(btn.tool.std, Bg);
        QColor c2 = sameRoles ? Colors::mid(c, Qt::black, 3,1) : CCOLOR(btn.tool.active, Bg);
#undef PAL
#define PAL option->palette
        
        
        if (option->state & State_On)
        {
            QColor h = c; c = c2; c2 = h; // swap colors
            if ( sameRoles ) gt = Gradients::Sunken; // sunken dark active - mac-a-like
        }

        if (Colors::value(c) < 50)
            { int h,s,v,a; c.getHsv(&h, &s, &v, &a); c.setHsv(h, s, 50, a); }
        if (step)
            c = Colors::mid(c, c2, 18-step, step);

        // shape
        const int d = 1;
        int pf = Tile::Full;
        Qt::Orientation o = tb->orientation();
        QRect geo = widget->geometry();
        if (o == Qt::Horizontal)
        {
            if (qobject_cast<QToolButton*>(tb->childAt(geo.x()-d, geo.y())))
                pf &= ~Tile::Left;
            if (qobject_cast<QToolButton*>(tb->childAt(geo.right()+d, geo.y())))
                pf &= ~Tile::Right;
        }
        else
        {
            if (qobject_cast<QToolButton*>(tb->childAt(geo.x(), geo.y()-d)))
                pf &= ~Tile::Top;
            if (qobject_cast<QToolButton*>(tb->childAt(geo.x(), geo.bottom()+d)))
                pf &= ~Tile::Bottom;
        }

        // paint
        o = (o == Qt::Horizontal) ? Qt::Vertical : Qt::Horizontal;
        Tile::setShape(pf);
#if 0
        if ( true )
        {
            const int d = F(3);
            masks.rect[round].render( rect, painter, Gradients::Sunken, o,
                                      Colors::mid(tb->palette().color(tb->backgroundRole()), Qt::black, 4, 1) );
            rect.adjust(bool(pf & Tile::Left) * d, bool(pf & Tile::Top) * d,
                        bool(pf & Tile::Right) * -d, bool(pf & Tile::Bottom) * -d);
            masks.rect[round].render(rect, painter, gt, o, c);
        }
        else 
#endif
        if (config.btn.tool.frame)
        {
            if (pf & Tile::Bottom)
                rect.setBottom(rect.bottom()-F(2));
            bool relief = false;
            if (config.btn.tool.frame == 2)
                relief = !(sunken || option->state & State_On);
            masks.rect[round].render(rect, painter, gt, o, c);
            if (relief)
                shadows.relief[round][true].render(RECT, painter);
            else
                shadows.sunken[round][true].render(RECT, painter);
        }
        else
        {
            // shadow
            if (pf & Tile::Top)
                rect.adjust(0, F(1), 0, 0);
            shadows.raised[round][true][false].render(rect, painter);
            
            // plate
            rect.adjust((pf & Tile::Left) ? F(2) : 0, (pf & Tile::Top) ? F(1) : 0,
                        (pf & Tile::Right) ? -F(1) : 0, (pf & Tile::Bottom) ? -F(3) : 0);
                        
            masks.rect[round].render(rect, painter, gt, o, c);

            // outline
            if (Gradients::isReflective(GRAD(btn)))
                lights.glow[round].render(rect, painter, c.lighter(120));

        }
        if (config.btn.tool.separator)
        {
            QPen pen = painter->pen();
            painter->setPen(Colors::mid(c,QColor(0,0,0),8,1));
            if (!(pf & Tile::Bottom))
                painter->drawLine(rect.bottomLeft(), rect.bottomRight());
            else if (!(pf & Tile::Right))
                painter->drawLine(rect.topRight(), rect.bottomRight());
            painter->setPen(pen);
        }
        Tile::reset();
    }
    else if (isEnabled && (option->state & State_On))
    {
        if (widget && widget->testAttribute(Qt::WA_StyleSheet))
            masks.rect[true].render(rect, painter, Gradients::Sunken, Qt::Vertical, QColor(128,128,128,128));
        else
        {
            const QColor &c = Colors::bg(PAL, widget);
            masks.rect[true].render(rect, painter, Gradients::Sunken, Qt::Vertical, c);
        }
        shadows.sunken[true][true].render(rect, painter);
    }
}

static QPixmap scaledIcon, emptyIcon;
qint64 lastIconPix = 0;
static QPixmap &
icon(QPixmap &pix, int step)
{
    if (pix.cacheKey() != lastIconPix)
    {
        scaledIcon = pix.scaledToHeight ( pix.height() + F(4), Qt::SmoothTransformation );
        if (emptyIcon.size() != scaledIcon.size())
            emptyIcon = QPixmap(scaledIcon.size());
        lastIconPix = pix.cacheKey();
    }
    emptyIcon.fill(Qt::transparent);
    float quote = step/6.0;
    if (quote >= 1.0)
        return scaledIcon;

    FX::blend(pix, emptyIcon, 1.0, F(2), F(2));
    FX::blend(scaledIcon, emptyIcon, quote);
    return emptyIcon;
}

void
Style::drawToolButtonLabel(const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    ASSURE_OPTION(toolbutton, ToolButton);
    OPT_ENABLED OPT_SUNKEN

    // Arrow type always overrules and is always shown
    const bool hasArrow = toolbutton->features & QStyleOptionToolButton::Arrow;
    const bool justText = toolbutton->toolButtonStyle == Qt::ToolButtonTextOnly ||
                          (!hasArrow && toolbutton->icon.isNull() && !toolbutton->text.isEmpty() );

    QPalette::ColorRole role = QPalette::WindowText;
    QPalette::ColorRole bgRole = QPalette::Window;
    if (connected)
    {
        role = config.btn.tool.std_role[Fg];
        bgRole = config.btn.tool.std_role[Bg];
    }
    if (widget && bgRole == QPalette::Window)
    {
        const QWidget *dad = widget->parentWidget();

        const QWidget *w = dad ? dad : widget;
        bgRole = w->backgroundRole();
        role = w->foregroundRole();
        
        if (role == QPalette::ButtonText && dad && dad->inherits("QMenu"))
        {
            role = config.menu.std_role[Fg]; // this is a f**** KMenu Header
            step = 0;
        }
    }

    QColor text = PAL.color(role);
    if (connected && (option->state & State_On))
        text = CCOLOR(btn.tool.active, Fg);

    if (justText)
    {   // the most simple way
        if (!connected)
            text = Colors::mid(text, FCOLOR(Link), 6-step, step);
        painter->setPen(text);
        if (sunken)
            setBold(painter, toolbutton->text, RECT.width());
        drawItemText(painter, RECT, Qt::AlignCenter | BESPIN_MNEMONIC, PAL, isEnabled, toolbutton->text);
        return;
    }

    QPixmap pm;
    QSize pmSize = RECT.size() - QSize(F(4), F(4));
    pmSize = pmSize.boundedTo(toolbutton->iconSize);

    if (!toolbutton->icon.isNull())
    {
        const int style = config.btn.tool.disabledStyle;
//         const QIcon::State state = toolbutton->state & State_On ? QIcon::On : QIcon::Off;
        pm = toolbutton->icon.pixmap(RECT.size().boundedTo(pmSize), isEnabled || style ? QIcon::Normal : QIcon::Disabled, QIcon::Off);
#if 0   // this is -in a way- the way it should be done..., but KIconLoader gives a shit on this or anything else
        if (!isEnabled)
            pm = generatedIconPixmap ( QIcon::Disabled, pm, toolbutton );
#else
        if (!isEnabled && style)
        {
            QImage img(pm.width() + F(4), pm.height() + F(4), QImage::Format_ARGB32);
            img.fill(Qt::transparent);
            QPainter p(&img);
            if (style > 1) // blurring
            {
                p.setOpacity(0.5);
                p.drawImage(F(3),F(3), pm.toImage().scaled(pm.size() - QSize(F(2),F(2)), Qt::KeepAspectRatio, Qt::SmoothTransformation));
                p.end();
                FX::expblur(img, F(3));
            }
            else // desaturation (like def. Qt but with a little transparency)
            {
                p.setOpacity(0.7);
                p.drawImage(F(2), F(2), pm.toImage());
                p.end();
                FX::desaturate(img, COLOR(bgRole));
            }
            pm = QPixmap::fromImage(img);
        }
#endif
        else if (step && !(connected || sunken || pm.isNull()))
            pm = icon(pm, step);
        pmSize = pm.size();
    }

    if (!(toolbutton->text.isEmpty() || toolbutton->toolButtonStyle == Qt::ToolButtonIconOnly))
    {
        if (!connected && pm.isNull())
            text = Colors::mid(text, FCOLOR(Link), 6-step, step);
        painter->setPen(text);
            
//         QFont fnt = toolbutton->font;
//         fnt.setStretch(QFont::SemiCondensed);
        painter->setFont(toolbutton->font);

        QRect pr = RECT, tr = RECT;
        int alignment = BESPIN_MNEMONIC;

        if (toolbutton->toolButtonStyle == Qt::ToolButtonTextUnderIcon)
        {
            int fh = painter->fontMetrics().height();
            pr.adjust(0, 0, 0, -fh - F(2));
            tr.adjust(0, pr.bottom(), 0, -F(3));
            if (!hasArrow)
                drawItemPixmap(painter, pr, Qt::AlignCenter, pm);
            else
                drawSolidArrow(Navi::S, pr, painter);
            alignment |= Qt::AlignCenter;
        }
        else
        {
            pr.setWidth(toolbutton->iconSize.width() + F(4));

            if (!hasArrow)
                drawItemPixmap(painter, pr, Qt::AlignCenter, pm);
            else
                drawSolidArrow(Navi::S, pr, painter);

            tr.adjust(pr.width() + F(4), 0, 0, 0);
            alignment |= Qt::AlignLeft | Qt::AlignVCenter;
        }
        drawItemText(painter, tr, alignment, PAL, isEnabled, toolbutton->text);
        return;
    }

    if (hasArrow)
    {
        const int f5 = F(5);
        drawSolidArrow(Navi::Direction(toolbutton->arrowType), RECT.adjusted(f5,f5,-f5,-f5), painter);
    }
    else
        drawItemPixmap(painter, RECT, Qt::AlignCenter, pm);
}

void
Style::drawToolBarHandle(const QStyleOption *option, QPainter *painter, const QWidget*widget) const
{

    OPT_HOVER
//    if (!hover) return;

    painter->save();
    QRect rect = RECT; bool line = false; int dx(0), dy(0);
    if (RECT.width() > RECT.height())
    {
        line = (RECT.width() > 9*RECT.height()/2);
        if (line)
            { dx = 3*RECT.height()/2; dy = 0; }
        rect.setLeft(rect.left()+(rect.width()-rect.height())/2);
        rect.setWidth(rect.height());
    }
    else
    {
        line = (RECT.height() > 3*RECT.width());
        if (line)
            { dx = 0; dy = 3*RECT.width()/2; }
        rect.setTop(rect.top()+(rect.height()-rect.width())/2);
        rect.setHeight(rect.width());
    }
    QColor c = (config.UNO.toolbar && widget && widget->autoFillBackground()) ? CCOLOR(UNO._, Bg) : FCOLOR(Window);
    if (hover)
        c = Colors::mid(c, FCOLOR(Highlight), 3, 1);
    else
        c = Colors::mid(c, Qt::black, 15, 1);

    painter->setRenderHint(QPainter::Antialiasing);
    painter->setBrush(Gradients::pix(c, rect.height(), Qt::Vertical, Gradients::Sunken));
    painter->setPen(Qt::NoPen);
    painter->setBrushOrigin(rect.topLeft());
    painter->drawEllipse(rect);
    if (line)
    {
        const int f1 = F(1);
        rect.adjust(f1,f1,-f1,-f1);
        painter->setBrush(Gradients::pix(c, rect.height(), Qt::Vertical, Gradients::Sunken));
        rect.translate(-dx,-dy);
        painter->setBrushOrigin(rect.topLeft());
        painter->drawEllipse(rect);
        rect.translate( 2*dx, 2*dy);
        painter->setBrushOrigin(rect.topLeft());
        painter->drawEllipse(rect);
    }
    painter->restore();
}
