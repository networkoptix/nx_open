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

#include <QApplication>
#include <QDockWidget>
#include <QStyleOptionDockWidget>
#include "draw.h"
#include "hacks.h"
#include "blib/shadows.h"

#include <QtDebug>

static QDockWidget *carriedDock = 0;

void
Style::dockLocationChanged( Qt::DockWidgetArea /*area*/ )
{
    QDockWidget *dock = carriedDock ? carriedDock : qobject_cast<QDockWidget*>( sender() );
    if ( !dock )
        return;

    if ( dock->isFloating() || !Hacks::config.lockDocks )
    {
        if ( QWidget *title = dock->titleBarWidget() )
        {
            if ( title->objectName() ==  "bespin_docktitle_dummy" )
            {
                dock->setTitleBarWidget(0);
                title->deleteLater();
            }
            else
                title->show();
        }
    }
    else
    {
        if ( !dock->titleBarWidget() )
        {
            QWidget *title = new QWidget;
            title->setObjectName( "bespin_docktitle_dummy" );
            dock->setTitleBarWidget( title );
        }
        dock->titleBarWidget()->hide();
    }
}

static struct {
    QPainterPath path;
    QSize size;
    bool ltr;
} glas = {QPainterPath(), QSize(), true};

void
Style::drawDockBg(const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    if (config.bg.mode == Scanlines && config.bg.structure < 5 && config.bg.opacity == 0xff)
    {
        painter->save();
        painter->setPen(Qt::NoPen);
        painter->setBrush(Gradients::structure(FCOLOR(Window), true));
        painter->translate(RECT.topLeft());
        painter->restore();
        painter->drawRect(RECT);
    }
    if (widget && widget->isWindow())
        drawWindowFrame(option, painter, widget);
}

void
Style::drawDockTitle(const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    ASSURE_OPTION(dock, DockWidget);

    QColor bg = widget ? COLOR(widget->backgroundRole()) : FCOLOR(Window);
    bg.setAlpha(config.bg.opacity);
    const bool floating = widget && widget->isWindow();

    if ((dock->floatable || dock->movable) && config.bg.opacity == 0xff)
    {
        if (!floating)
        {
            if (widget)
            {
                if (config.bg.docks.shape && widget->parentWidget())
                {
                    QPixmap *buffer = new QPixmap(RECT.size());
                    QRect r = buffer->rect();
                    const int rnd = F(8);
                    QPainter bp(buffer);
                    bp.setPen(Qt::NoPen);
                    QPoint pt = widget->mapFrom(widget->parentWidget(), RECT.topLeft());
                    erase(option, &bp, widget->parentWidget(), &pt);
                    bp.setBrush(FCOLOR(Window));
                    bp.setRenderHint(QPainter::Antialiasing);
                    QPainterPath path;
                    path.moveTo(rnd, 0);
                    path.arcTo(QRect(0,0,rnd,rnd), 90, 90);
                    path.lineTo(0, buffer->height());
                    path.lineTo(buffer->width(), buffer->height());
                    path.lineTo(buffer->width(), rnd);
                    path.arcTo(QRect(buffer->width() - rnd,0,rnd,rnd), 0, 90);
                    path.closeSubpath();
                    bp.drawPath(path);
                    bp.end();
                    painter->drawPixmap(RECT.topLeft(), *buffer);
                    delete buffer;
                }
//                 else if (widget->underMouse())
//                     shadows.relief[false][false].render(widget->rect(), painter);
            }
            bool ltr = !widget || (widget->window() && widget->mapTo(widget->window(), QPoint(0,0)).x() < 3);
            if (glas.size != RECT.size() || ltr != glas.ltr)
            {
                glas.ltr = ltr;
                glas.size = RECT.size();
                glas.path = QPainterPath();
                if (ltr)
                {
                    glas.path.moveTo(RECT.topLeft());
                    glas.path.lineTo(RECT.topRight());
                    glas.path.quadTo(RECT.center()/2, RECT.bottomLeft());
                }
                else
                {
                    glas.path.moveTo(RECT.topRight());
                    glas.path.lineTo(RECT.topLeft());
                    glas.path.quadTo(RECT.center()/2, RECT.bottomRight());
                }
            }
        }
        painter->save();
        painter->setPen(Qt::NoPen);
        painter->setRenderHint(QPainter::Antialiasing);
        const int v = Colors::value(bg);
        const int alpha = bg.alpha()*v / (255*(7-v/80));
        painter->setBrush(QColor(255,255,255,alpha));
        floating ? painter->drawRect(RECT.adjusted(0,0,0,-RECT.height()/2)) : painter->drawPath(glas.path);
        painter->restore();
    }

    if (dock->title.isEmpty())
        return;

    OPT_ENABLED
    QRect rect = RECT;
    const int bo = 16 + F(6);
    rect.adjust(dock->closable ? bo : F(4), 0, dock->closable ? -bo : -F(4), 0);

    // text
    const int itemtextopts = Qt::AlignCenter | Qt::TextSingleLine | Qt::TextHideMnemonic;
    QPalette::ColorRole fg = widget ? widget->foregroundRole() : QPalette::WindowText;
    QFont fnt = painter->font();
    setBold(painter, dock->title, rect.width());
    QPen pen = painter->pen();
    if (floating && widget->isActiveWindow())
        painter->setPen(COLOR(fg));
    else
        painter->setPen(Colors::mid(bg, COLOR(fg), 2, 1+isEnabled));
    drawItemText(painter, rect, itemtextopts, PAL, isEnabled, dock->title/*, QPalette::NoRole, &rect*/);
    painter->setPen(pen);
    painter->setFont(fnt);
//     const int d = 3*rect.width()/16; rect.adjust(d,0,-d,0);
//     shadows.line[0][Sunken].render(rect, painter, Tile::Full, true);
}

void
Style::drawDockHandle(const QStyleOption *option, QPainter *painter, const QWidget *) const
{
    OPT_HOVER
#if 0
    const bool vert = RECT.height() > RECT.width();
    QRect rect = RECT;
    if (vert)
    {
        const int d = RECT.height() / 3;
        rect.adjust(0,d,0,-d);
    }
    else
    {
        const int d = RECT.width() / 3;
        rect.adjust(d,0,-d,0);
    }
    shadows.line[vert][Sunken].render(rect, painter, Tile::Full, false);
#else
    QPoint *points; int num;
    const int f12 = F(12), f6 = F(6);
    if (RECT.width() > RECT.height())
    {
        int x = RECT.left()+4*RECT.width()/9;
        int y = RECT.top()+(RECT.height()-f6)/2;
        num = RECT.width()/(9*f12);
        if ((4*RECT.width()/9) % f12)
            ++num;
        points = new QPoint[num];
        for (int i = 0; i < num; ++i)
            { points[i] = QPoint(x,y); x += f12; }
    }
    else
    {
        int x = RECT.left()+(RECT.width()-f6)/2;
        int y = RECT.top()+4*RECT.height()/9;
        num = RECT.height()/(9*f12);
        if ((4*RECT.height()/9) % f12)
            ++num;
        points = new QPoint[num];
        for (int i = 0; i < num; ++i)
            { points[i] = QPoint(x,y); y += f12; }
    }
    painter->save();
    painter->setPen(Qt::NoPen);
    const QPixmap *fill; int cnt = num/2, imp = hover ? 4 : 1;
    const QColor &bg = FCOLOR(Window);
    const QColor &fg = hover ? FCOLOR(Highlight) : FCOLOR(WindowText);
    if (num%2)
    {
        fill = &Gradients::pix(Colors::mid(bg, fg, 5, imp), f6, Qt::Vertical, Gradients::Sunken);
        fillWithMask(painter, points[cnt], *fill, masks.notch);
    }
    --num;
    for (int i = 0; i < cnt; ++i)
    {
        fill = &Gradients::pix(Colors::mid(bg, fg, 5+cnt-i, imp), f6, Qt::Vertical, Gradients::Sunken);
        fillWithMask(painter, points[i], *fill, masks.notch);
        fillWithMask(painter, points[num-i], *fill, masks.notch);
    }
    painter->restore();
    delete[] points;
#endif
}

void
Style::drawMDIControls(const QStyleOptionComplex *option, QPainter *painter, const QWidget *widget) const
{
    QStyleOptionButton btnOpt;
    btnOpt.QStyleOption::operator=(*option);
    OPT_SUNKEN

#define PAINT_MDI_BUTTON(_btn_)\
if (option->subControls & SC_Mdi##_btn_##Button)\
{\
    if (sunken && option->activeSubControls & SC_Mdi##_btn_##Button)\
    {\
        btnOpt.state |= State_Sunken;\
        btnOpt.state &= ~State_Raised;\
    }\
    else\
    {\
        btnOpt.state |= State_Raised;\
        btnOpt.state &= ~State_Sunken;\
    }\
    btnOpt.rect = subControlRect(CC_MdiControls, option, SC_Mdi##_btn_##Button, widget);\
    painter->drawPixmap(btnOpt.rect.topLeft(), standardPixmap(SP_TitleBar##_btn_##Button, &btnOpt, widget));\
}//

    PAINT_MDI_BUTTON(Close);
    PAINT_MDI_BUTTON(Normal);
    PAINT_MDI_BUTTON(Min);

#undef PAINT_MDI_BUTTON
}

void
Style::unlockDocks(bool b)
{
    const bool lock = Hacks::config.lockDocks;
    Hacks::config.lockDocks = b;
    foreach ( QWidget *w, qApp->allWidgets() )
    {
        if ( (carriedDock = qobject_cast<QDockWidget*>(w)) )
        if ( !carriedDock->isFloating() )
            dockLocationChanged( Qt::AllDockWidgetAreas );
    }
    carriedDock = 0;
    Hacks::config.lockDocks = lock;
}
