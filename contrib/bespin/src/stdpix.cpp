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

#include <QStyleOptionTitleBar>
#include <QApplication>
#include <QDockWidget>
#include <QImageReader>
#include <QPainter>
#include <QPen>

#include "blib/colors.h"
#include "blib/gradients.h"
#include "blib/shapes.h"

#include "bespin.h"

#include <QtDebug>

#define COLOR(_TYPE_) pal.color(QPalette::_TYPE_)

using namespace Bespin;

static void
setIconFont(QPainter &painter, const QRect &rect, float f = 0.75)
{
    QFont fnt = painter.font();
    fnt.setPixelSize ( (int)(f*rect.height()) );
    fnt.setBold(true); painter.setFont(fnt);
}

static inline uint qt_intensity(uint r, uint g, uint b)
{
    // 30% red, 59% green, 11% blue
    return (77 * r + 150 * g + 28 * b) / 255;
}

#if 0
static
QPainterPath arrow( const QRect &rect, bool right = false )
{
    int cy = rect.center().y();
    int x1 = rect.right();
    int s = -1;
    if (right)
        { s = 1; x1 = rect.left(); }

    QPainterPath shape;
    shape.moveTo( x1, rect.top() );
    shape.quadTo( x1+s*18*rect.width()/10, cy, x1, rect.bottom() );
    shape.quadTo( x1+s*rect.width()/5, cy, x1, rect.top() );
    return shape;
}
#endif
QPixmap
Style::standardPixmap(StandardPixmap standardPixmap, const QStyleOption *option, const QWidget *widget ) const
{
    bool sunken = false, isEnabled = false, hover = false;
    if (option)
    {
        sunken = option->state & State_Sunken;
        isEnabled = option->state & State_Enabled;
        hover = isEnabled && (option->state & State_MouseOver);
    }

    QRect rect; QPalette pal;
//    const QStyleOptionTitleBar *opt =
//       qstyleoption_cast<const QStyleOptionTitleBar *>(option);
    if (option)
    {
        if (option->rect.isNull()) // THIS SHOULD NOT!!!! happen... but unfortunately does on dockwidgets...
            rect = QRect(0,0,14,14);
        else
        {
            rect = option->rect; rect.moveTo(0,0);
            if (rect.width() > rect.height())
                rect.setWidth(rect.height());
            else
                rect.setHeight(rect.width());
        }
        pal = option->palette;
    }
    else
    {
        rect = QRect(0,0,14,14);
        pal = widget ? widget->palette() : qApp->palette();
    }

    QPalette::ColorRole bg = QPalette::Window, fg = QPalette::WindowText;
    if (widget)
        { bg = widget->backgroundRole(); fg = widget->foregroundRole(); }

    const QDockWidget *dock = qobject_cast<const QDockWidget*>(widget);
    const int sz = dock ? 14 : rect.height();
    QPixmap pm(sz, sz);
    pm.fill(Qt::transparent);
    QPainter painter(&pm);
    QPainterPath shape;
    Shapes::Style style = (Shapes::Style)config.winBtnStyle;

    switch (standardPixmap)
    {
#if 0
    case SP_ArrowBack:
    case SP_ArrowLeft:
        shape = arrow( pm.rect() ).subtracted(arrow( pm.rect().translated(pm.rect().width()/2, 0) ));
        goto paint;
    case SP_ArrowRight:
    case SP_ArrowForward:
        shape = arrow( pm.rect(), true ).subtracted(arrow( pm.rect().translated(-pm.rect().width()/2, 0), true ));
        goto paint;
    case SP_MediaPlay:
        shape = arrow( pm.rect(), true );
        goto paint;
    case SP_MediaPause:
        shape = Shapes::unAboveBelow(pm.rect());
        goto paint;
    case SP_BrowserReload:
    {
        QRectF rect = pm.rect();
        const float s = rect.width()/3.0;
        QPointF c = rect.center();
        QRectF box(rect.x(), c.y()-s/2, s, s);

        shape.arcMoveTo(rect, 90);
        shape.arcTo(rect, 90, 90);
        shape.arcTo(box, 180, 180);
        shape.arcTo(rect.adjusted(s,s,-s,-s), 180, -90);
        box.moveTo(c.x()-s/2, rect.y());
        shape.arcTo(box, -90, 180);
        shape.closeSubpath();

        shape.arcMoveTo(rect, -90);
        shape.arcTo(rect, -90, 90);
        box.moveBottomRight(QPointF(rect.right(), c.y()+s/2));
        shape.arcTo(box, 0, 180);
        shape.arcTo(rect.adjusted(s,s,-s,-s), 0, -90);
        box.moveBottomRight(QPointF(c.x()+s/2, rect.bottom()));
        shape.arcTo(box, 90, 180);
        shape.closeSubpath();
/*
        int d5 = rect.height()/5;
        rect.setWidth( 4*rect.width()/5 );
        rect.setHeight( rect.height()/4 );
        rect.moveTop( rect.y() + d5 );
        shape.addRoundRect( rect, 50, 50 );
        rect.moveBottom( pm.rect().bottom() - d5 );
        rect.moveRight( pm.rect().right() );
        shape.addRoundRect( rect, 50, 50 );
        */
        goto paint;
    }
#endif
//         SP_MediaSkipForward 63  Icon indicating that media should skip forward.
//         SP_MediaSkipBackward    64  Icon indicating that media should skip backward.
//         SP_MediaSeekForward 65  Icon indicating that media should seek forward.
//         SP_MediaSeekBackward    66  Icon indicating that media should seek backward.

    case SP_DockWidgetCloseButton:
    case SP_TitleBarCloseButton:
//     case SP_BrowserStop:
//     case SP_MediaStop:
        shape = Shapes::close(pm.rect(), style);
        goto paint;
    case SP_TitleBarMinButton:
        shape = Shapes::min(pm.rect(), style);
        goto paint;
    case SP_TitleBarMaxButton:
        shape = Shapes::max(pm.rect(), style);
        goto paint;
    case SP_TitleBarMenuButton:
        shape = Shapes::menu(pm.rect(), false, style);
        goto paint;
    case SP_TitleBarShadeButton:
    case SP_TitleBarUnshadeButton:
        shape = Shapes::shade(pm.rect(), style);
        goto paint;
    case SP_TitleBarNormalButton:
        if (dock)
            shape = Shapes::dockControl(pm.rect(), dock->isFloating(), style);
        else
            shape = Shapes::restore(pm.rect(), style);
        goto paint;
    case SP_TitleBarContextHelpButton:
    {
        shape = Shapes::help(pm.rect(), style);
#if 0
        goto paint;
    case SP_ArrowDown:
    case SP_ArrowUp:
    case SP_FileDialogToParent: //  30
    {
        const float d = sz/2.0;
        shape = arrow( pm.rect()).subtracted(arrow( pm.rect().translated(d, 0)));
        painter.translate(d, d);
        painter.rotate(standardPixmap == SP_ArrowDown ? -90 : 90);
        painter.translate(-d, -d);
        goto paint;
    }
    case SP_MediaVolume:
    case SP_MediaVolumeMuted:
    {
        {
            QRectF r = pm.rect(); float d = r.width()/8.0;
            r.adjust(d,d,-d,-d);
            float x = r.width()/2.0, y = r.height()/2.0;
            shape.moveTo(0, r.y() + y);
            shape.quadTo(0,r.y() + y/2.0, x, r.y());
            shape.quadTo(r.width() - x/2.0, r.y()+y, x, r.bottom());
            shape.quadTo(0,r.bottom()-y/2.0, 0, r.y()+y);
            if (standardPixmap == SP_MediaVolume)
            {
                r = pm.rect();
                int st = -75, sw = 150;
                shape.moveTo(x, y);
                shape.arcTo(r, st, sw);
                r.adjust(d,d,-d,-d);
                shape.moveTo(x, y); shape.arcTo(r, st, sw);
                d = r.width()/8.0; r.adjust(d,d,-d,-d);
                shape.moveTo(x, y); shape.arcTo(r, st, sw);
                d = r.width()/8.0; r.adjust(d,d,-d,-d);
                shape.moveTo(x, y); shape.arcTo(r, st, sw);
                shape.closeSubpath();
            }
            else
            {
                shape.moveTo(r.topLeft());
                shape.lineTo(r.bottomRight());
                shape.moveTo(r.topRight());
                shape.lineTo(r.bottomLeft());
                shape.closeSubpath();
            }
        }
#endif
paint:
        const QColor c = Colors::mid(pal.color(fg), pal.color(bg), (sz > 16) ? 16 : 2, sunken ? 2 : (hover ? 4 : 2) );
        painter.setRenderHint ( QPainter::Antialiasing );
        if (sz > 16)
            painter.setPen(pal.color(bg));
        else
            painter.setPen(Qt::NoPen);
#if 0
        if (sz > 16)
            painter.setBrush( Gradients::brush( c, sz, Qt::Vertical, config.btn.gradient ) );
        else
#endif
            painter.setBrush(c);
        painter.drawPath(shape);
        break;
    }
    case SP_MessageBoxInformation:
    { //  9  The "information" icon
        const int bs = rect.height()/14;
        rect.adjust(bs,bs,-bs,-bs);
        setIconFont(painter, rect);
        painter.setRenderHint ( QPainter::Antialiasing );
        painter.setPen(QPen(Qt::white, bs));
        painter.setBrush(QColor(0,102,255));
        painter.drawEllipse(rect);
        painter.setPen(Qt::white);
        painter.drawText(rect, Qt::AlignHCenter | Qt::AlignBottom, "i");
        break;
    }
    case SP_MessageBoxWarning:
    { //  10  The "warning" icon
        int bs = rect.width()/14;
        rect.adjust(bs,bs,-bs,-bs);
        int hm = rect.x()+rect.width()/2;
        const QPoint points[3] = {
            QPoint(hm, rect.top()),
            QPoint(rect.left(), rect.bottom()),
            QPoint(rect.right(), rect.bottom())
        };
        setIconFont(painter, rect);
        painter.setRenderHint ( QPainter::Antialiasing);
        painter.setPen(QPen(QColor(227,173,0), bs, Qt::SolidLine, Qt::SquareCap, Qt::RoundJoin));
        painter.setBrush(QColor(255,235,85));
        painter.drawPolygon(points, 3);
        painter.setPen(Qt::black);
        painter.drawText(rect, Qt::AlignHCenter | Qt::AlignBottom, "!");
        break;
    }
    case SP_MessageBoxCritical:
    { //  11  The "critical" icon
        const int bs = rect.height()/14;
        rect.adjust(bs,bs,-bs,-bs);
        setIconFont(painter, rect);
        painter.setRenderHint ( QPainter::Antialiasing );
        painter.setPen(QPen(Qt::white/*Color(226,8,0)*/, bs));
        painter.setBrush(QColor(156,15,15));
        painter.drawEllipse(rect);
        painter.setPen(Qt::white);
        painter.drawText(rect, Qt::AlignCenter, "X");
        break;
    }
    case SP_MessageBoxQuestion:
    { //  12  The "question" icon
        setIconFont(painter, rect, 1);
        QColor c = COLOR(WindowText); c.setAlpha(128);
        painter.setPen(c);
        painter.drawText(rect, Qt::AlignCenter, "?");
        break;
    }
//    case SP_DesktopIcon: //  13   
//    case SP_TrashIcon: //  14   
//    case SP_ComputerIcon: //  15   
//    case SP_DriveFDIcon: //  16   
//    case SP_DriveHDIcon: //  17   
//    case SP_DriveCDIcon: //  18   
//    case SP_DriveDVDIcon: //  19   
//    case SP_DriveNetIcon: //  20   
//     case SP_DirOpenIcon: //  21
//     case SP_DirClosedIcon: //  22
//     case SP_DirLinkIcon: //  23
//         break;
#if 0
    case SP_FileDialogNewFolder: //  31
    {
        const float t = rect.width()/8.0;
        const float half = rect.width()/2.0;
        const float third = rect.width()/3.0;
        painter.setPen(QPen(Colors::mid(pal.color(bg), pal.color(fg)), t/2.0));
        painter.setBrush(Qt::NoBrush);
        painter.setRenderHint ( QPainter::Antialiasing );
        QRectF mother(t/2.0,t/2.0,half,half);
        painter.drawArc(mother, 10*16,80*16);
        painter.drawArc(mother, 270*16,80*16);
        mother.adjust(t,t,-t,-t);
        painter.drawEllipse(mother);

        QRectF child(sz-third,sz-third,third,third);
        painter.drawArc(child, 75*16,110*16);
        child.adjust(t,t,-t,-t);
        painter.drawEllipse(child);

        QRectF baby(sz-third,t/2.0,third,third);
//         painter.drawArc(baby, 110*16,110*16);
//         baby.adjust(t,t,-t,-t);
        QPointF c = baby.center();
        painter.drawLine(baby.x(), c.y(), baby.right(), c.y());
        painter.drawLine(c.x(), baby.y(), c.x(), baby.bottom());

        painter.drawLine(mother.center(), child.center());
        break;
    }
//    case SP_FileIcon: //  24   
//    case SP_FileLinkIcon: //  25

//    case SP_FileDialogStart: //  28   
//    case SP_FileDialogEnd: //  29

    case SP_FileDialogDetailedView: //  32
    {
        const float t = rect.width()/8.0;
        painter.setPen(QPen(Colors::mid(pal.color(bg), pal.color(fg)), t));
        painter.setBrush(Qt::NoBrush);
        painter.setRenderHint ( QPainter::Antialiasing );
        float y = t;
        while (y <= rect.height()-t)
        {
            painter.drawPoint(t, y); painter.drawLine(3*t, y, rect.right(), y);
            y += 2*t;
        }
        break;
    }

//    case SP_FileDialogInfoView: //  33   
//    case SP_FileDialogContentsView: //  34   
    case SP_FileDialogListView: //  35
    {
        QRectF r(0,0,sz/3.0,sz/3.0);
        painter.setPen(Qt::NoPen);
        painter.setBrush(Colors::mid(pal.color(bg), pal.color(fg)));
        painter.setRenderHint(QPainter::Antialiasing);
        painter.drawEllipse(r);
        r.moveRight(pm.rect().right()); painter.drawEllipse(r);
        r.moveBottom(pm.rect().bottom()); painter.drawEllipse(r);
        r.moveLeft(pm.rect().left()); painter.drawEllipse(r);
        break;
    }
#endif
//     case SP_FileDialogBack: //  36
//         break;

    case SP_ToolBarHorizontalExtensionButton: //  26  Extension button for horizontal toolbars
    case SP_ToolBarVerticalExtensionButton: //  27  Extension button for vertical toolbars
        painter.setPen(Qt::NoPen);
        painter.setBrush(Colors::mid(COLOR(Window), COLOR(WindowText)));
        drawSolidArrow(standardPixmap == SP_ToolBarHorizontalExtensionButton ? Navi::E : Navi::S, rect, &painter);
        break;
    default:
        return QCommonStyle::standardPixmap ( standardPixmap, option, widget );
    }
    painter.end();
#if 0
    QPixmapCache::insert(key, pm);
#endif
    return pm;
}

#undef COLOR
