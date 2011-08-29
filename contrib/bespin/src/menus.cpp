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

#include <QAction>
#include <QMainWindow>
#include <QMenuBar>
#include "draw.h"
#include "animator/hoverindex.h"

static const bool round_ = true;

void
Style::drawMenuBarBg(const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    QMainWindow *mwin = 0;
    if (widget)
        mwin = qobject_cast<QMainWindow*>(widget->parentWidget());
    if (!mwin)
        return;

    const bool needScanlines = config.bg.mode == Scanlines && config.bg.structure < 5;
    if (!(config.UNO.used || needScanlines))
        return;

    QRect rect = RECT;
    QColor c = CCOLOR(UNO._, Bg);

    Tile::PosFlags pf = 0;
    if (config.UNO.sunken && !config.UNO.title)
        pf |= Tile::Top;
    
//     if (sunken)
//         rect.setBottom(rect.bottom()-F(2));

    if (config.UNO.used)
    {
        QVariant var = mwin->property("UnoHeight");
        int h = var.isValid() ? var.toInt() : 0;
        if (config.UNO.gradient)
        {
            if (h)
            {
                const QPixmap &fill = Gradients::pix(CCOLOR(UNO._, Bg), (h & 0xffffff), Qt::Vertical, config.UNO.gradient);
                painter->drawTiledPixmap(RECT, fill, QPoint(0,widget->geometry().y() + ((h>>24) & 0xff)));
            }
        }
        else if (config.bg.mode == Scanlines)
            painter->fillRect(rect, Gradients::structure(c, needScanlines));

        h = (h & 0xffffff) - ((h>>24) & 0xff);

        if (config.UNO.sunken && h == widget->geometry().bottom()+1) // i.e. no toolbar
            pf |= Tile::Bottom;
    }
    else // "else if (needScanlines)"
        painter->fillRect(rect, Gradients::structure(c, true));

    if (pf)
    {
        Tile::setShape(pf);
        shadows.sunken[false][false].render(RECT, painter);
        Tile::reset();
    }
}

void
Style::drawMenuBarItem(const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    ASSURE_OPTION(mbi, MenuItem);
    // NOTICE a)bused in XBar, b)ut shouldn't happen anyway
//     if (mbi->menuItemType == QStyleOptionMenuItem::Separator && appType != Plasma)
//         return;

#if 1 // was necessary once, not anymore?!
    if (appType != GTK && mbi->menuRect.height() > mbi->rect.height())
    {
        QStyleOptionMenuItem copy = *mbi;
        copy.rect.setTop(mbi->menuRect.top());
        copy.rect.setHeight(mbi->menuRect.height());
        drawMenuBarBg( &copy, painter, widget );
    }
    else
#endif
        drawMenuBarBg(option, painter, widget);

    OPT_SUNKEN OPT_ENABLED

    QPalette::ColorRole bg = config.menu.active_role[Bg];
    QPalette::ColorRole fg = config.menu.active_role[Fg];
    if (bg == config.UNO.__role[Bg])
    {   // swap!
        bg = fg;
        fg = config.UNO.__role[Bg];
    }

    bool hover = (option->state & State_Selected);
    Animator::IndexInfo *info = 0;
    int step = 0;
    QFont pFont = painter->font();

    if (isEnabled && sunken)
        step = 6;
    else
    {   // check for hover animation ==========================
        if (const QMenuBar* mbar = qobject_cast<const QMenuBar*>(widget))
        {
            QAction *action = mbar->actionAt(RECT.topLeft()); // is the action for this item!
            if (action && action->font().bold())
                setBold(painter, mbi->text);
            QAction *activeAction = mbar->activeAction();
            info = const_cast<Animator::IndexInfo*>(Animator::HoverIndex::info(widget, (long int)activeAction));
            if (info && (!(activeAction && activeAction->menu()) || activeAction->menu()->isHidden()))
                step = info->step((long int)action);
        }
        else if (appType == Plasma && widget)
        {
            // i abuse this property as xbar menus are no menus and the active state is s thing of it's own...
            int action = (mbi->menuItemType & 0xffff);
            int activeAction = ((mbi->menuItemType >> 16) & 0xffff);
            info = const_cast<Animator::IndexInfo*>(Animator::HoverIndex::info(widget, activeAction));
            if (info)
                step = info->step(action);
        }
        // ================================================
    }
    
    QRect r = RECT.adjusted(0, F(2), 0, -F(4));
    if (isEnabled && (step || hover))
    {
        if (!step)
            step = 6;
        QColor c;
        if (appType == Plasma)
        {   // NOTICE: opaque Colors::mid() are too flickerious with several plasma bgs...
            bg = QPalette::WindowText;
            fg = QPalette::Window;
        }
        c = COLOR(bg);
        c.setAlpha(step*255/8);
        // NOTICE: scale code - currently unused
//       int dy = 0;
//       if (!sunken) {
//          step = 6-step;
//          int dx = step*r.width()/18;
//          dy = step*r.height()/18;
//          r.adjust(dx, dy, -dx, -dy);
//          step = 6-step;
//       }
        const Gradients::Type gt = sunken ? Gradients::Sunken : config.menu.itemGradient;
        masks.rect[round_].render(r, painter, gt, Qt::Vertical, c, r.height()/*, QPoint(0,-dy)*/);
        if (config.menu.activeItemSunken && sunken)
        {
            r.setBottom(r.bottom()+F(1));
            shadows.sunken[round_][true].render(r, painter);
            r.adjust(0,F(1),0,-F(1)); // -> text repositioning
        }
        else if (step == 6 && config.menu.itemSunken)
            shadows.sunken[round_][false].render(r, painter);
    }
    QPalette::ColorRole fg2 = config.UNO.__role[Fg];
    QPixmap pix = mbi->icon.pixmap(pixelMetric(PM_SmallIconSize), isEnabled ? QIcon::Normal : QIcon::Disabled);
    const uint alignment = Qt::AlignCenter | BESPIN_MNEMONIC | Qt::TextDontClip | Qt::TextSingleLine;
    if (!pix.isNull())
        drawItemPixmap(painter,r, alignment, pix);
    else
    {
#if 0 // emboss
        QPalette::ColorRole ffg = (hover || step > 3) ? fg : fg2;
        if (isEnabled && appType == BEshell)
        {
            const QPen save = painter->pen();
            if (Colors::value(COLOR(ffg)) > 112)
            {
                painter->setPen(QColor(0,0,0,128));
                painter->drawText( r.translated(0,-1), alignment, mbi->text );
            }
            else
            {
                painter->setPen(QColor(255,255,255,200));
                painter->drawText( r.translated(0,1), alignment, mbi->text );
            }
            painter->setPen(save);
        }
#endif
        drawItemText(painter, r, alignment, mbi->palette, isEnabled, mbi->text, (hover || step > 3) ? fg : fg2);
    }
    painter->setFont(pFont);
}

static const int windowsItemFrame   = 1; // menu item frame width
static const int windowsItemHMargin = 3; // menu item hor text margin
static const int windowsItemVMargin = 1; // menu item ver text margin
static const int windowsRightBorder = 12; // right border on windows

void
Style::drawMenuItem(const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    ASSURE_OPTION(menuItem, MenuItem);
    ROLES(menu.std);
    OPT_SUNKEN OPT_ENABLED

    if (appType == GTK)
        sunken = false;

    if (menuItem->menuItemType == QStyleOptionMenuItem::Separator)
    {   // separator ===============================
        int dx = RECT.width()/10,
            dy = (RECT.height()-shadows.line[0][Sunken].thickness())/2;
        painter->save();
        const QRegion rgn = QRegion(RECT).subtract( painter->
                                                    boundingRect( RECT, Qt::AlignCenter, menuItem->text).
                                                    adjusted(-F(4),0,F(4),0));
        painter->setClipRegion(rgn, Qt::IntersectClip);
        shadows.line[0][Sunken].render(RECT.adjusted(dx,dy,-dx,-dy), painter);
        painter->restore();
        if (!menuItem->text.isEmpty())
        {
            setBold(painter, menuItem->text, RECT.width());
            drawItemText(painter, RECT, Qt::AlignCenter, PAL, isEnabled, menuItem->text, ROLE[Fg]);
            painter->setFont(menuItem->font);
        }
        return;
    }
       
    QRect r = RECT.adjusted(0,0,-1,-1);
    bool selected = isEnabled && menuItem->state & State_Selected;

    QColor bg = PAL.color(QPalette::Active, ROLE[Bg]);
    QColor fg = isEnabled ? COLOR(ROLE[Fg]) : Colors::mid(bg, PAL.color(QPalette::Active, ROLE[Fg]), 2,1);

    SAVE_PEN;
    SAVE_BRUSH;
    const bool checkable = (menuItem->checkType != QStyleOptionMenuItem::NotCheckable);
    const bool checked = checkable && menuItem->checked;
    const bool subMenu = (menuItem->menuItemType == QStyleOptionMenuItem::SubMenu);

    // selected bg
    if (selected)
    {
        if (config.menu.itemGradient != Gradients::None || config.UNO.__role[Bg] == ROLE[Bg] ||
            Colors::contrast(COLOR(ROLE[Bg]), CCOLOR(menu.active, Bg)) > 8) // enough to indicate hover
        {
            bg = Colors::mid(COLOR(ROLE[Bg]), CCOLOR(menu.active, Bg), 1, 6);
            fg = CCOLOR(menu.active, Fg);
        }
        else
        {
            bg = Colors::mid(COLOR(ROLE[Bg]), COLOR(ROLE[Fg]), 1, 2);
            fg = COLOR(ROLE[Bg]);
        }
        if (!config.menu.roundSelect)
            Tile::setShape(Tile::Top|Tile::Bottom|Tile::Center);
        masks.rect[round_].render( r, painter, sunken ? Gradients::Sunken : config.menu.itemGradient, Qt::Vertical, bg);
        if (sunken && config.menu.itemSunken)
            shadows.sunken[round_][false].render(r, painter);
        Tile::reset();
    }
       
    // Text and icon, ripped from windows style
    const QStyleOptionMenuItem *menuitem = menuItem;
    int iconCol = config.menu.showIcons*menuitem->maxIconWidth;

    if (isEnabled && config.menu.showIcons && !menuItem->icon.isNull())
    {
        QRect vCheckRect = visualRect(option->direction, r, QRect(r.x(), r.y(), iconCol, r.height()));
        const QPixmap &pixmap = menuItem->icon.pixmap(pixelMetric(PM_SmallIconSize), QIcon::Normal, checked ? QIcon::On : QIcon::Off);

        QRect pmr(QPoint(0, 0), pixmap.size());
        pmr.moveCenter(vCheckRect.center());

        painter->drawPixmap(pmr.topLeft(), pixmap);
    }
       
    int x, y, w, h;
    r.getRect(&x, &y, &w, &h);
    const int tab = menuitem->tabWidth;
    const int cDim = (2*(r.height()+2)/3);
    const int xm = windowsItemFrame + iconCol + windowsItemHMargin;
    int xpos = r.x() + xm;

    if (checkable)
    {   // Checkmark =============================
//         xpos = r.x() + F(4); //r.right() - F(4) - cDim;
        QStyleOptionMenuItem tmpOpt = *menuItem;
        tmpOpt.rect = QRect(xpos, r.y() + (r.height() - cDim)/2, cDim, cDim);
        tmpOpt.rect = visualRect(menuItem->direction, menuItem->rect, tmpOpt.rect);
        tmpOpt.state &= ~State_Selected; // cause of color, not about checkmark!
        if (checked)
        {
            tmpOpt.state |= State_On;
            tmpOpt.state &= ~State_Off;
        }
        else
        {
            tmpOpt.state |= State_Off;
            tmpOpt.state &= ~State_On;
        }
        painter->setPen(Colors::mid(bg, fg));
        // i don't know why, but opera sends has inverted or button colors for non selected items...
        // the roles are however correct.
        painter->setBrush(appType == Opera ? painter->pen().color() : fg);
        if (menuItem->checkType & QStyleOptionMenuItem::Exclusive)
        {
            const int d = cDim/7;
            tmpOpt.rect.adjust(d,d,-d,-d);
            drawExclusiveCheck(&tmpOpt, painter, widget); // Radio button
        }
        else // Check box
            drawMenuCheck(&tmpOpt, painter, widget);
        xpos += cDim + F(4);
    }
    
    painter->setPen ( fg );
    painter->setBrush ( Qt::NoBrush );
    
    QRect textRect(xpos, y + windowsItemVMargin,
                   w - (xm + checkable*(cDim+F(4)) + subMenu*windowsRightBorder + tab + windowsItemFrame + windowsItemHMargin),
                   h - 2 * windowsItemVMargin);
    QRect vTextRect = visualRect(option->direction, r, textRect);
    
    QString s = menuitem->text;
    if (!s.isEmpty())
    {   // draw text
        int t = s.indexOf('\t');
        const int text_flags = Qt::AlignVCenter | BESPIN_MNEMONIC | Qt::TextDontClip | Qt::TextSingleLine;
        if (t >= 0)
        {
            QRect vShortcutRect = visualRect(option->direction, r, QRect(textRect.topRight(),
                                             QPoint(textRect.right()+tab, textRect.bottom())));
            painter->setPen(Colors::mid(bg, fg));
            drawItemText(painter, vShortcutRect, text_flags | Qt::AlignRight, PAL, isEnabled, s.mid(t + 1));
//          painter->drawText(vShortcutRect, text_flags | Qt::AlignRight, s.mid(t + 1));
            painter->setPen(fg);
            s = s.left(t);
        }
        if (menuitem->menuItemType == QStyleOptionMenuItem::DefaultItem)
        {
            QFont fnt = painter->font();
            setBold(painter, s, vTextRect.width());
            drawItemText(painter, vTextRect, text_flags | Qt::AlignLeft, PAL, isEnabled, s);
            painter->setFont(fnt);
        }
        else
            drawItemText(painter, vTextRect, text_flags | Qt::AlignLeft, PAL, isEnabled, s);
//       painter->drawText(vTextRect, text_flags | Qt::AlignLeft, s.left(t));
    }
         
    if (subMenu)
    {   // draw sub menu arrow ================================

        Navi::Direction dir = (option->direction == Qt::RightToLeft) ? Navi::W : Navi::E;
        int dim = 5*r.height()/12;
        xpos = r.right() - F(4) - dim;
        QStyleOptionMenuItem tmpOpt = *menuItem;
        tmpOpt.rect = visualRect(option->direction, r, QRect(xpos, r.y() + (r.height() - dim)/2, dim, dim));
        painter->setBrush(Colors::mid(bg, fg, 1, 2));
        painter->setPen(QPen(painter->brush(), painter->pen().widthF()));
        drawArrow(dir, tmpOpt.rect, painter);
    }
    RESTORE_PEN;
    RESTORE_BRUSH;
}

void
Style::drawMenuScroller(const QStyleOption *option, QPainter *painter, const QWidget *) const
{
    OPT_SUNKEN

    QPoint offset;
    Navi::Direction dir = Navi::N;
    QPalette::ColorRole bg = config.menu.std_role[0];
    const QPixmap &gradient = Gradients::pix(PAL.color(QPalette::Active, bg), 2*RECT.height(), Qt::Vertical,
                                             sunken ? Gradients::Sunken : Gradients::Button);
    if (option->state & State_DownArrow)
        { offset = QPoint(0, RECT.height()); dir = Navi::S; }
    painter->drawTiledPixmap(RECT, gradient, offset);
    drawArrow(dir, RECT, painter);
}

//    case CE_MenuTearoff: // A menu item representing the tear off section of a QMenu

