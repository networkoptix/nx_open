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

#include <QToolButton>
#include "draw.h"
#include "animator/hoverindex.h"

inline static bool
verticalTabs(QTabBar::Shape shape)
{
    return  shape == QTabBar::RoundedEast ||
            shape == QTabBar::TriangularEast ||
            shape == QTabBar::RoundedWest ||
            shape == QTabBar::TriangularWest;
}

void
Style::drawTabWidget(const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    if (appType == GTK)
    {
        shadows.sunken[true][true].render(RECT, painter);
        return;
    }
   
    ASSURE_OPTION(twf, TabWidgetFrame);
    QLine line[2];
    QStyleOptionTabBarBase tbb;
    if (widget)
        tbb.initFrom(widget);
    else
        tbb.QStyleOption::operator=(*twf);
    tbb.shape = twf->shape; tbb.rect = twf->rect;

#define SET_BASE_HEIGHT(_o_) \
baseHeight = twf->tabBarSize._o_(); \
if (!baseHeight) return; /*  no base -> no tabbing -> no bottom border either. period.*/\
if (baseHeight < 0) \
    baseHeight = pixelMetric( PM_TabBarBaseHeight, option, widget )
         
    int baseHeight;
    switch (twf->shape)
    {
    case QTabBar::RoundedNorth: case QTabBar::TriangularNorth:
        SET_BASE_HEIGHT(height);
        tbb.rect.setHeight(baseHeight);
        line[0] = line[1] = QLine(RECT.bottomLeft(), RECT.bottomRight());
        line[0].translate(0,-1);
        break;
    case QTabBar::RoundedSouth: case QTabBar::TriangularSouth:
        SET_BASE_HEIGHT(height);
        tbb.rect.setTop(tbb.rect.bottom()-baseHeight);
        line[0] = line[1] = QLine(RECT.topLeft(), RECT.topRight());
        line[1].translate(0,1);
        break;
    case QTabBar::RoundedEast: case QTabBar::TriangularEast:
        SET_BASE_HEIGHT(width);
        tbb.rect.setLeft(tbb.rect.right()-baseHeight);
        line[0] = line[1] = QLine(RECT.topLeft(), RECT.bottomLeft());
        line[1].translate(1,0);
        break;
    case QTabBar::RoundedWest: case QTabBar::TriangularWest:
        SET_BASE_HEIGHT(width);
        tbb.rect.setWidth(baseHeight);
        line[0] = line[1] = QLine(RECT.topRight(), RECT.bottomRight());
        line[0].translate(-1,0);
        break;
    }
#undef SET_BASE_HEIGHT
   
    // the "frame"
    painter->save();
    painter->setPen(FCOLOR(Window).dark(120));
    painter->drawLine(line[0]);
    painter->setPen(FCOLOR(Window).light(114));
    painter->drawLine(line[1]);
    painter->restore();
    // the bar
    drawTabBar(&tbb, painter, widget);
}

void
Style::drawTabBar(const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    ASSURE_OPTION(tbb, TabBarBase);

    QWidget *win = 0;

    // this has completely changed with recent KDE, KTabWidget doesn't call the style at all?!
    if (widget)
    {
        if (widget->parentWidget() && qobject_cast<QTabWidget*>(widget->parentWidget()))
        {
#if 1 // QT_VERSION < 0x040500
            // in general we don't want a tabbar on a tabwidget
            // that's nonsense, looks crap... and still used by some KDE apps
            // the konqueror guys however want the konqueror tabbar to look
            // somewhat like Bespin =)
            // so permit the proxystyle solution
            if (widget->parentWidget()->style() == this)
#endif
                return; // otherwise it's a proxystyle like on konqueror / kdevelop...

        }
        else if (qobject_cast<const QTabBar*>(widget) || (appType == KDevelop && widget->inherits("QLabel")))
            return; // usually we alter the paintevent by eventfiltering
        win = widget->window();
    }
    else
    {
        if (painter->device()->devType() == QInternal::Widget)
            widget = static_cast<QWidget*>(painter->device());
        else
        {
            QPaintDevice *dev = QPainter::redirected(painter->device());
            if (dev && dev->devType() == QInternal::Widget)
                widget = static_cast<QWidget*>(dev);
        }
        if ( widget )
            win = widget->window();
    }

    QRect rect = RECT.adjusted(0, 0, 0, -F(2));
    int size = RECT.height(); Qt::Orientation o = Qt::Vertical;

    QRect winRect;
    if (win)
    {
        winRect = win->rect();
        winRect.moveTopLeft(widget->mapFrom(win, winRect.topLeft()));
    }
//     else
//         winRect = tbb->tabBarRect; // we set this from the eventfilter QEvent::Paint

    Tile::PosFlags pf = Tile::Full;
    if (verticalTabs(tbb->shape))
    {
        if (RECT.bottom() >= winRect.bottom())
            pf &= ~Tile::Bottom; // we do NEVER shape away the top - assuming deco here...!
        if (RECT.left() <= winRect.left())
            pf &= ~Tile::Left;
        if (RECT.right() >= winRect.right())
            pf &= ~Tile::Right;
        o = Qt::Horizontal; size = RECT.width();
    }
    else
    {
        if (RECT.width() >= winRect.width())
            pf &= ~(Tile::Left | Tile::Right);
        else
        {
            if (RECT.left() <= winRect.left()) pf &= ~Tile::Left;
            if (RECT.right() >= winRect.right()) pf &= ~Tile::Right;
        }
    }
    Tile::setShape(pf);

    masks.rect[true].render(rect, painter, GRAD(tab), o, CCOLOR(tab.std, Bg), size);
    rect.setBottom(rect.bottom()+F(2));
    shadows.sunken[true][true].render(rect, painter);
    Tile::reset();
}

static int animStep = -1;
static bool customColor = false;

void 
Style::calcAnimStep(const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{   // animation stuff
    OPT_ENABLED OPT_HOVER OPT_SUNKEN
    sunken = sunken || (option->state & State_Selected);
    animStep = 0;
    if ( isEnabled && !sunken )
    {
        Animator::IndexInfo *info = 0;
        int index = -1, hoveredIndex = -1;
        if (widget)
        if (const QTabBar* tbar = qobject_cast<const QTabBar*>(widget))
        {
            // NOTICE: the index increment is IMPORTANT to make sure it's not "0"
            index = tbar->tabAt(RECT.topLeft()) + 1; // is the action for this item!

            // sometimes... MANY times devs just set the tabTextColor to QPalette::WindowText,
            // because it's defined that it has to be this. Qt provides all these color roles just
            // to waste space and time... ...
            const QColor &fgColor = tbar->tabTextColor(index - 1);
            const QColor &stdFgColor = tbar->palette().color(config.tab.std_role[Fg]);
            if (fgColor.isValid() && fgColor != stdFgColor)
            {
                if (fgColor == tbar->palette().color(QPalette::WindowText))
                    const_cast<QTabBar*>(tbar)->setTabTextColor(index - 1, stdFgColor); // fixed
                else // nope, this is really a custom color that will likley contrast just enough with QPalette::Window...
                {
                    customColor = true;
                    if (Colors::haveContrast(tbar->palette().color(config.tab.std_role[Bg]), fgColor))
                        painter->setPen(fgColor);
                }
            }
#if QT_VERSION >= 0x040500
            if (!tbar->documentMode() || tbar->drawBase())
#endif
            {
                if (hover)
                    hoveredIndex = index;
                else if (widget->underMouse())
                    hoveredIndex = tbar->tabAt(tbar->mapFromGlobal(QCursor::pos())) + 1;
                info = const_cast<Animator::IndexInfo*>(Animator::HoverIndex::info(widget, hoveredIndex));
            }
            
        }
        if (info)
            animStep = info->step(index);
        if (hover && !animStep)
            animStep = 6;
    }
}

void
Style::drawTab(const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    ASSURE_OPTION(tab, Tab);

#if 0 // solid bg now, but kep in case ;-)
    // do we have to exclude the scrollers?
    bool needRestore = false;
    if (widget && qobject_cast<const QTabBar*>(widget))
    {
        QRegion clipRgn = painter->clipRegion();
        if (clipRgn.isEmpty())
            clipRgn = RECT;
            
        QList<QToolButton*> buttons = widget->findChildren<QToolButton*>();
        foreach (QToolButton* button, buttons)
        {
            if (button->isVisible())
                clipRgn -= QRect(button->pos(), button->size());
        }
        if (!clipRgn.isEmpty())
        {
            painter->save();
            needRestore = true;
            painter->setClipRegion(clipRgn);
        }
    }
#endif

    // paint shape and label
    QStyleOptionTab copy = *tab;
    // NOTICE: workaround for e.g. konsole,
    // which sets the tabs bg, but not the fg color to the palette, but just
    // presets the painter and hopes for the best... tststs
    // TODO: bug Konsole/Konqueror authors
    if (widget)
        copy.palette = widget->palette();

    copy.rect.adjust(0, F(1), 0, -F(2));

    if (appType == GTK)
    {
        QRect r = copy.rect;
        r.adjust(0, F(1), 0, -F(1));
        switch (tab->position)
        {
        default:
        case QStyleOptionTab::OnlyOneTab:
            r.adjust(F(2), 0, -F(2), 0); break;
        case QStyleOptionTab::Beginning:
            r.setLeft(r.left()+F(2));
            Tile::setShape(Tile::Full &~ Tile::Right); break;
        case QStyleOptionTab::End:
            r.setRight(r.right()-F(2));
            Tile::setShape(Tile::Full &~ Tile::Left); break;
        case QStyleOptionTab::Middle:
            Tile::setShape(Tile::Full &~ (Tile::Left | Tile::Right)); break;
        }
        shadows.raised[true][true][false].render(RECT, painter);
        masks.rect[true].render(r, painter, GRAD(tab), Qt::Vertical, CCOLOR(tab.std, Bg), r.height());
//         shadows.sunken[true][true].render(RECT, painter);
        Tile::reset();
    }

    if (tab->position != QStyleOptionTab::OnlyOneTab || appType == GTK)
    {
        calcAnimStep( option, painter, widget );
        drawTabShape(&copy, painter, widget);
    }
#if QT_VERSION >= 0x040500
    if (config.tab.std_role[Bg] == config.tab.active_role[Bg])
        copy.rect = RECT;
    else if HAVE_OPTION(tabV3, TabV3)
    {
#if QT_VERSION >= 0x040600	
        int d[4] = {0,1,0,0};
#else
        int d[4] = {0,0,0,0};
#endif
        if (verticalTabs(tab->shape))
        {
            if ( tabV3->leftButtonSize.isValid() ) d[1] = tabV3->leftButtonSize.height() + F(2);
            if ( tabV3->rightButtonSize.isValid() ) d[3] = -tabV3->rightButtonSize.height() - F(2);
        }
        else
        {
            if ( tabV3->leftButtonSize.isValid() ) d[0] = tabV3->leftButtonSize.width() + F(2);
            if ( tabV3->rightButtonSize.isValid() ) d[2] = -tabV3->rightButtonSize.width() - F(2);
        }
        copy.rect.adjust( d[0], d[1], d[2], d[3] );
    }
#endif
    drawTabLabel(&copy, painter, widget);
    customColor = false;
#if 0
    if (needRestore)
        painter->restore();
#endif
}

void
Style::drawTabShape(const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    ASSURE_OPTION(tab, Tab);
    OPT_SUNKEN

    QRect rect = RECT;

    if (appType == GTK)
    {
        rect.adjust(0, F(3), 0, 0 );
        sunken = option->state & State_Selected;
    }
    else if (tab->position == QStyleOptionTab::OnlyOneTab)
        sunken = false;
    else
        sunken = sunken || (option->state & State_Selected);
    
    // konsole now paints itself and of course it would be wrong to 
    // just ask the style to paint a tabbar *grrrrrr...*
    bool isOnlyShape = false;
    if ( (isOnlyShape = (animStep < 0)) )
        calcAnimStep( option, painter, widget );

    // maybe we're done here?!
    if ( !(animStep || sunken) )
        return;
    
    int size = rect.height() + F(3);
    Qt::Orientation o = Qt::Vertical;
    const bool vertical = verticalTabs(tab->shape);
    if (vertical)
    {
        rect.adjust(F(3), F(1), -F(4), -F(1));
        o = Qt::Horizontal;
        size = RECT.width();
    }
    else    // Konsole now thinks it has to write it's own broken tab painting :-(
        rect.adjust( F(1), F(2), -F(1), -( F(3) + isOnlyShape*F(2) ) );
    
    QColor c;
    bool noBase = false;
    const bool sameRoles = config.tab.active_role[Bg] == config.tab.std_role[Bg];
    if (sunken)
    {
        if (const QTabBar *bar = qobject_cast<const QTabBar*>(widget))
        {
            noBase = true;
            if (bar->drawBase() || qobject_cast<const QTabWidget*>(bar->parentWidget()))
                noBase = false;
        }

        c = sameRoles ? Colors::mid(CCOLOR(tab.std, Bg), CCOLOR(tab.std, Fg), 60, 6) : CCOLOR(tab.active, Bg);
        if (option->state & State_HasFocus)
            c = Colors::mid(c, FCOLOR(Highlight), 2, 1);
/*
        else if (config.tab.activeTabSunken && Colors::value(CCOLOR(tab.std, Bg)) < 164)
            vertical ?  rect.adjust(0, F(1), 0, -F(2)) : rect.adjust(F(2), -F(1), -F(2), 0);
        else
            vertical ? rect.adjust(-F(1), F(2), F(1), -F(2)) : rect.adjust(F(2), -F(1), -F(2), F(1));*/
    }
    else if (sameRoles)
        c = Colors::mid(CCOLOR(tab.std, Bg), CCOLOR(tab.std, Fg), 66-animStep, animStep);
    else
        c = Colors::mid(CCOLOR(tab.std, Bg), CCOLOR(tab.active, Bg), 10-animStep, animStep);

    Gradients::Type gt = (config.tab.activeTabSunken && sunken) ? Gradients::Sunken : GRAD(tab);
    if (sameRoles)
    {
        rect = RECT.adjusted( F(1), F(2), -F(1), isOnlyShape*-F(2));
        Tile::setShape( (o == Qt::Vertical) ? (Tile::Left|Tile::Right) : (Tile::Top|Tile::Bottom) );
        if (sunken) 
        {
            if ( GRAD(tab) == Gradients::Sunken )
                gt = Gradients::Simple;
            rect.adjust(0,-F(2),0,0);
        }
        painter->drawTiledPixmap(rect, Gradients::pix(c, size, o, gt));
        rect.adjust(0,0,0,-F(2));
    }
    else
        masks.rect[true].render(rect, painter, gt, o, c, size, rect.topLeft());

    // shadow
    if ((config.tab.activeTabSunken || noBase) && sunken)
    {
        rect.setBottom(rect.bottom() + F(2));
        shadows.sunken[true][true].render(rect, painter);
    }
    
    Tile::reset();
}

void
Style::drawTabLabel(const QStyleOption *option, QPainter *painter, const QWidget*) const
{
    ASSURE_OPTION(tab, Tab);
    OPT_SUNKEN OPT_ENABLED OPT_HOVER
    if (tab->position == QStyleOptionTab::OnlyOneTab)
        { sunken = false; hover = false; }
    else
        sunken = sunken || (option->state & State_Selected);
    if (sunken) hover = false;

    painter->save();
    QRect tr = RECT;

    bool vertical = verticalTabs(tab->shape);
    bool east = false;


    if (vertical)
    {
        int newX, newY, newRot;
        if (east)
            { newX = tr.width(); newY = tr.y(); newRot = 90; }
        else
            { newX = 0; newY = tr.y() + tr.height(); newRot = -90; }
        tr.setRect(0, 0, tr.height(), tr.width());
        QMatrix m; m.translate(newX, newY); m.rotate(newRot);
        painter->setMatrix(m, true);
    }

    int alignment = Qt::AlignCenter | BESPIN_MNEMONIC;
    if ( !tab->icon.isNull() )
    {
        QSize iconSize;
        if (const QStyleOptionTabV2 *tabV2 = qstyleoption_cast<const QStyleOptionTabV2*>(tab))
            iconSize = tabV2->iconSize;
        if ( !iconSize.isValid() )
        {
            int iconExtent = pixelMetric(PM_SmallIconSize);
            iconSize = QSize(iconExtent, iconExtent);
        }
        QPixmap tabIcon = tab->icon.pixmap(iconSize, (isEnabled) ? QIcon::Normal : QIcon::Disabled);

        if (animStep > 2 || sunken)
            painter->setClipRect(tr.adjusted(F(4), F(4), -F(5), -F(5)));
        painter->drawPixmap(tr.left() + F(9), tr.center().y() - tabIcon.height() / 2, tabIcon);
        tr.setLeft(tr.left() + iconSize.width() + F(12));
        alignment = Qt::AlignLeft | Qt::AlignVCenter | BESPIN_MNEMONIC;
    }
#if QT_VERSION >= 0x040500
    if HAVE_OPTION(tabV3, TabV3)
    {
        if (vertical)
        {
            tr.setLeft(tr.left() + tabV3->leftButtonSize.height());
            tr.setRight(tr.right() - tabV3->rightButtonSize.height());
        }
        else
        {
            tr.setLeft(tr.left() + tabV3->leftButtonSize.width());
            tr.setRight(tr.right() - tabV3->rightButtonSize.width());
        }
    }
#endif

    // color adjustment
    QColor cF = CCOLOR(tab.std, Fg), cB = CCOLOR(tab.std, Bg);
//     qDebug() << cF << cB;
    if (sunken)
    {
        cF = CCOLOR(tab.active, Fg);
        cB = CCOLOR(tab.active, Bg);
//         if (config.tab.active_role[Bg] == config.tab.std_role[Bg])
            setBold(painter, tab->text, tr.width());
    }
    else if (animStep)
    {
        cB = Colors::mid(cB, CCOLOR(tab.active, Bg), 8-animStep, animStep);
        if (Colors::contrast(CCOLOR(tab.active, Fg), cB) > Colors::contrast(cF, cB))
            cF = CCOLOR(tab.active, Fg);
    }
    else if (customColor)
    {
        if (Colors::haveContrast(CCOLOR(tab.std, Bg), painter->pen().color()))
            cF = painter->pen().color();
        QFont fnt = painter->font();
        fnt.setUnderline(true);
        painter->setFont(fnt);
    }
//     else
//     {
//         cB = Colors::mid(CCOLOR(tab.std, Bg), FCOLOR(Window), 2, 1);
//         cF = Colors::mid(cB, CCOLOR(tab.std, Fg), 1,4);
//     }

//     sunken = sunken && config.tab.activeTabSunken;
//     if (!sunken || Colors::value(CCOLOR(tab.active, Bg)) < 164)
        tr.translate(0,-F(1));
    if (isEnabled && Colors::value(cB) < Colors::value(cF)) // this is not the same...
    {   // dark background, let's paint an emboss
        painter->setPen(cB.dark(120));
        tr.moveTop(tr.top()-1);
        drawItemText(painter, tr, alignment, PAL, isEnabled, tab->text);
        tr.moveTop(tr.top()+1);
    }
    painter->setPen(cF);
    drawItemText(painter, tr, alignment, PAL, isEnabled, tab->text);

    painter->restore();
    animStep = -1;
}

#if QT_VERSION >= 0x040500
void
Style::drawTabCloser(const QStyleOption *option, QPainter *painter, const QWidget*) const
{
    OPT_SUNKEN OPT_HOVER
    if (sunken) hover = false;
    
    QRect rect = RECT;
    sunken ? rect.adjust(F(5),F(4),-F(4),-F(5)) :
    hover ?  rect.adjust(F(3),F(2),-F(2),-F(3)) :
             rect.adjust(F(4),F(3),-F(3),-F(4));

    painter->setRenderHint(QPainter::Antialiasing);

    QColor c = Colors::mid( CCOLOR(tab.std, Bg), CCOLOR(tab.std, Fg), 3, 1+hover );
    painter->setPen( QPen( c, F(3) ) );
    painter->drawEllipse( rect );

//     c = hover ? CCOLOR(tab.active, Fg) : Colors::mid( CCOLOR(tab.active, Bg), CCOLOR(tab.active, Fg) );
//     painter->setPen( QPen( c, F(2) ) );
//     rect.adjust(F(2),F(2),-F(2),-F(2));
//     painter->drawEllipse( rect );
}
#endif

void
Style::drawToolboxTab(const QStyleOption *option, QPainter *painter, const QWidget * widget) const
{
    ASSURE_OPTION(tbt, ToolBox);

    // color fix...
    if (widget && widget->parentWidget())
        const_cast<QStyleOption*>(option)->palette = widget->parentWidget()->palette();

    drawToolboxTabShape(tbt, painter, widget);
    QStyleOptionToolBox copy = *tbt;
    copy.rect.setBottom(copy.rect.bottom()-F(2));
    drawToolboxTabLabel(&copy, painter, widget);
}

void
Style::drawToolboxTabShape(const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    if (option->state & State_Selected)
        return; // plain selected items
    OPT_HOVER OPT_SUNKEN

    QRect r = RECT; Tile::PosFlags pf = Tile::Full;
    if (const QStyleOptionToolBoxV2* tbt = qstyleoption_cast<const QStyleOptionToolBoxV2*>(option))
    {
        switch (tbt->position)
        {
        case QStyleOptionToolBoxV2::Beginning:
            pf &= ~Tile::Bottom; break;
        case QStyleOptionToolBoxV2::Middle:
            pf &= ~(Tile::Top | Tile::Bottom); break;
        case QStyleOptionToolBoxV2::End:
            pf &= ~Tile::Top; // fallthrough
        default: // single
            r.setBottom(r.bottom()-F(2));
        }

        // means we touch the displayed box bottom
        switch (tbt->selectedPosition)
        {
        case QStyleOptionToolBoxV2::PreviousIsSelected:
            pf |= Tile::Top; break;
        // means we touch the displayed box top
        case QStyleOptionToolBoxV2::NextIsSelected:
            pf |= Tile::Bottom; break;
        default: break;
        }

        // handle window relative position
        if (widget)
        if (QWidget *win = widget->window())
        {
            QRect winRect = win->rect();
            winRect.moveTopLeft(widget->mapFrom(win, winRect.topLeft()));
            if (RECT.width() >= winRect.width())
                pf &= ~(Tile::Left | Tile::Right);
            else
            {
                if (RECT.left() <= winRect.left()) pf &= ~Tile::Left;
                if (RECT.right() >= winRect.right()) pf &= ~Tile::Right;
            }
        }
    }

    QColor c = CCOLOR(tab.std, Bg);
    Gradients::Type gt = GRAD(tab);
    
    if (gt == Gradients::Sunken) // looks freaky
        gt = Gradients::Button;
    if (sunken)
        { c = FCOLOR(Window); gt = Gradients::Sunken; }
    else if (hover)
    {
        c = Colors::mid(CCOLOR(tab.std, Bg), CCOLOR(tab.active, Bg), 4, 1);
        if ( config.tab.std_role[Bg] == config.tab.active_role[Bg] )
            gt = (gt == Gradients::Button) ? Gradients::Glass : Gradients::Button;
    }

    Tile::setShape(pf);
    masks.rect[true].render(r, painter, gt, Qt::Vertical, c);
    Tile::setShape(pf & ~Tile::Center);
    shadows.sunken[true][true].render(RECT, painter);
    Tile::reset();
}

void
Style::drawToolboxTabLabel(const QStyleOption *option, QPainter *painter, const QWidget *) const
{
    ASSURE_OPTION(tbt, ToolBox);
    OPT_ENABLED OPT_SUNKEN
    const bool selected = option->state & (State_Selected);

    QColor cB = FCOLOR(Window), cF = FCOLOR(WindowText);
    painter->save();
    if (selected)
        setTitleFont(painter);
    else if (!sunken)
        { cB = CCOLOR(tab.std, Bg); cF = CCOLOR(tab.std, Fg); }

    // on dark background, let's paint an emboss
    const uint tf = Qt::AlignHCenter | (selected ? Qt::AlignBottom : Qt::AlignVCenter) | BESPIN_MNEMONIC;
    if (isEnabled && Colors::value(cB) < Colors::value(cF))
    {
        QRect tr = RECT;
        painter->setPen(cB.dark(120));
        tr.moveTop(tr.top()-1);
        drawItemText(painter, tr, tf, PAL, isEnabled, tbt->text);
        tr.moveTop(tr.top()+1);
    }
    painter->setPen(cF);
    if (selected)
    {
        QRect rect;
        drawItemText(painter, RECT, tf, PAL, isEnabled, tbt->text, QPalette::NoRole, &rect);
        Tile::PosFlags pf = Tile::Center;
        if (option->direction == Qt::LeftToRight)
            { rect.setRight(RECT.right()); pf |= Tile::Left; }
        else
            { rect.setLeft(RECT.left()); pf |= Tile::Right; }
        shadows.line[0][Sunken].render(rect, painter, pf, true);
//         painter->drawLine(rect.x(), RECT.bottom(), RECT.right(), RECT.bottom());
    }
    else
        drawItemText(painter, RECT, tf, PAL, isEnabled, tbt->text);
    painter->restore();
}
