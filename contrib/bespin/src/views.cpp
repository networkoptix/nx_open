/* Bespin widget style for Qt4
   Copyright (C) 2007-2009 Thomas Luebking <thomas.luebking@web.de>

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
#include <QAbstractItemView>
#include <QPushButton>
#include <QTreeView>
#include "draw.h"

void
Style::drawHeader(const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    ASSURE_OPTION(header, Header);

    if (appType == GTK)
        const_cast<QStyleOption*>(option)->palette = qApp->palette();

    // init
    //    const QRegion clipRegion = painter->clipRegion();
    //    painter->setClipRect(RECT/*, Qt::IntersectClip*/);

    // base
    drawHeaderSection(header, painter, widget);

    // label
    drawHeaderLabel(header, painter, widget);

    // sort Indicator on sorting or (inverted) on hovered headers
    if (header->sortIndicator != QStyleOptionHeader::None)
    {
        QStyleOptionHeader subopt = *header;
        subopt.rect = subElementRect(SE_HeaderArrow, option, widget);
        drawHeaderArrow(&subopt, painter, widget);
    }

//    painter->setClipRegion(clipRegion);
}

void
Style::drawHeaderSection(const QStyleOption *option, QPainter *painter, const QWidget*) const
{
    OPT_SUNKEN OPT_HOVER
    const QStyleOptionHeader *header = qstyleoption_cast<const QStyleOptionHeader *>(option);
    const bool sorting = header && (header->sortIndicator != QStyleOptionHeader::None);

    Qt::Orientation o = Qt::Vertical; int s = RECT.height();
    if (header && header->orientation == Qt::Vertical)
    {
        o = Qt::Horizontal;
        s = RECT.width();
    }

    QColor c =  sorting ? COLOR(config.view.sortingHeader_role[Bg]) : COLOR(config.view.header_role[Bg]);

    if (Colors::value(c) < 50)
        { int h,s,v,a; c.getHsv(&h, &s, &v, &a); c.setHsv(h, s, 50, a); }

    if (appType == GTK)
        sunken = option->state & State_HasFocus;
    if (sunken)
    {
        const QPixmap &sunk = Gradients::pix(c, s, o, Gradients::Sunken);
        painter->drawTiledPixmap(RECT, sunk);
        return;
    }

    const Gradients::Type gt = sorting ? config.view.sortingHeaderGradient : config.view.headerGradient;

    if (hover)
        c = Colors::mid(c, sorting ? CCOLOR(view.sortingHeader, Fg) : CCOLOR(view.header, Fg),8,1);

    if (gt == Gradients::None)
        painter->fillRect(RECT, c);
    else
        painter->drawTiledPixmap(RECT, Gradients::pix(c, s, o, gt));

    if (o == Qt::Vertical)
    {
        if (!header || header->section < QStyleOptionHeader::End)
        {
            QRect r = RECT; r.setLeft(r.right() - F(1));
            painter->drawTiledPixmap(r, Gradients::pix(CCOLOR(view.header, Bg), s, o, Gradients::Sunken));
        }
        if (Colors::value(CCOLOR(view.header, Bg)) > 90) // not on dark elements - looks just stupid...
        {
            SAVE_PEN
            painter->setPen(Colors::mid(FCOLOR(Base), Qt::black, 6, 1));
            painter->drawLine(RECT.bottomLeft(), RECT.bottomRight());
            RESTORE_PEN
        }
    }
}

void
Style::drawHeaderLabel(const QStyleOption * option, QPainter * painter, const QWidget *widget) const
{
    OPT_ENABLED

    const QStyleOptionHeader* header = qstyleoption_cast<const QStyleOptionHeader*>(option);
    QRect rect = widget ? RECT.intersected(widget->rect()) : RECT;

    // iconos
    if ( !header->icon.isNull() )
    {
        QPixmap pixmap = header->icon.pixmap( 22,22, isEnabled ? QIcon::Normal : QIcon::Disabled );
        int pixw = pixmap.width();
        int pixh = pixmap.height();
        // "pixh - 1" because of tricky integer division
        rect.setY( rect.center().y() - (pixh - 1) / 2 );
        drawItemPixmap ( painter, rect, Qt::AlignCenter, pixmap );
        rect = RECT; rect.setLeft( rect.left() + pixw + 2 );
    }

    if (header->text.isEmpty())
        return;

    // textos ;)
    painter->save();

    // this works around a possible Qt bug?!?
    bool bold = (option->state & State_On);
    const QColor *bg, *fg;
    if (header->sortIndicator != QStyleOptionHeader::None)
    {
        bold = true;
        bg = &CCOLOR(view.sortingHeader, Bg);
        fg = &CCOLOR(view.sortingHeader, Fg);
    }
    else
    {
        bg = &CCOLOR(view.header, Bg);
        fg = &CCOLOR(view.header, Fg);
    }
    if (bold)
        setBold(painter, header->text, rect.width());

    if (isEnabled)
    {   // dark background, let's paint an emboss
        rect.moveTop(rect.top()-1);
        painter->setPen(bg->dark(120));
        drawItemText ( painter, rect, Qt::AlignCenter, PAL, isEnabled, header->text);
        rect.moveTop(rect.top()+1);
    }

    painter->setPen(*fg);
    drawItemText ( painter, rect, Qt::AlignCenter, PAL, isEnabled, header->text);
    painter->restore();
}

void
Style::drawHeaderArrow(const QStyleOption * option, QPainter * painter, const QWidget *) const
{
    Navi::Direction dir = Navi::S;
    if (const QStyleOptionHeader* hopt = qstyleoption_cast<const QStyleOptionHeader*>(option))
    {
        if (hopt->sortIndicator == QStyleOptionHeader::None)
            return;
        if (hopt->sortIndicator == QStyleOptionHeader::SortUp)
            dir = Navi::N;
    }
    painter->save();
    painter->setPen(Qt::NoPen);
    painter->setBrush(Colors::mid(CCOLOR(view.sortingHeader, Bg), CCOLOR(view.sortingHeader, Fg)));
    drawArrow(dir, RECT, painter);
    painter->restore();
}

static const int decoration_size = 9;

void
Style::drawBranch(const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{

    if ( !RECT.isValid() )
        return;

    SAVE_PEN;
    int mid_h = RECT.x() + RECT.width() / 2;
    int mid_v = RECT.y() + RECT.height() / 2;
    int bef_h = mid_h;
    int bef_v = mid_v;
    int aft_h = mid_h;
    int aft_v = mid_v;


    QPalette::ColorRole bg = QPalette::Text, fg = QPalette::Base;
    if (widget)
        { bg = widget->backgroundRole(); fg = widget->foregroundRole(); }

    bool firstCol = ( RECT.x() < 1 );
    if HAVE_OPTION(item, ViewItemV4)
        firstCol = item->viewItemPosition == QStyleOptionViewItemV4::Beginning ||
                   item->viewItemPosition == QStyleOptionViewItemV4::OnlyOne;

    if (option->state & State_Children)
    {
        SAVE_BRUSH
        int delta = decoration_size / 2 + 2;
        bef_h -= delta;
        bef_v -= delta;
        aft_h += delta;
        aft_v += delta;
        painter->setPen(Qt::NoPen);
        QRect rect = QRect(bef_h+2, bef_v+2, decoration_size, decoration_size);
        if (firstCol)
            rect.moveRight(RECT.right()-F(1));
        Navi::Direction dir;
        QColor c;
        if (option->state & State_Open)
        {
            c = (option->state & State_Selected) ? FCOLOR(HighlightedText) : Colors::mid( COLOR(bg), COLOR(fg));
            rect.translate(0,-decoration_size/6);
            dir = (option->direction == Qt::RightToLeft) ? Navi::SW : Navi::SE;
        }
        else
        {
            c = (option->state & State_Selected) ? FCOLOR(HighlightedText) : Colors::mid( COLOR(bg), COLOR(fg), 6, 1);
            dir = (option->direction == Qt::RightToLeft) ? Navi::W : Navi::E;
        }
        c.setAlpha(255);
        painter->setBrush(c);
        drawSolidArrow(dir, rect, painter);
        RESTORE_BRUSH
    }

    // no line on the first column!
    if (firstCol)
    {
        RESTORE_PEN;
        return;
    }

    painter->setPen(Colors::mid( COLOR(bg), COLOR(fg), 40, 1));

    if (option->state & (State_Item | State_Sibling))
        painter->drawLine(mid_h, RECT.y(), mid_h, bef_v);
    if (option->state & State_Sibling)
        painter->drawLine(mid_h, aft_v, mid_h, RECT.bottom());
    if (option->state & State_Item)
    {
        if (option->direction == Qt::RightToLeft)
            painter->drawLine(RECT.left(), mid_v, bef_h, mid_v);
        else
            painter->drawLine(aft_h, mid_v, RECT.right(), mid_v);
    }
    RESTORE_PEN;
}

void
Style::drawTree(const QStyleOptionComplex *option, QPainter *painter, const QWidget *widget) const
{
#ifdef QT3_SUPPORT
    ASSURE_OPTION(lv, Q3ListView);

    int i;
    if (lv->subControls & SC_Q3ListView)
        QCommonStyle::drawComplexControl(CC_Q3ListView, lv, painter, widget);
    if (!(lv->subControls & (SC_Q3ListViewBranch | SC_Q3ListViewExpand)))
        return;

    if (lv->items.isEmpty())
        return;
    QStyleOptionQ3ListViewItem item = lv->items.at(0);
    int y = lv->rect.y();
    int c;
//     int dotoffset = 0;
    QPolygon dotlines;

    QPalette::ColorRole bg = QPalette::Text, fg = QPalette::Base;
    if (widget)
        { bg = widget->backgroundRole(); fg = widget->foregroundRole(); }
    QColor  cLine = Colors::mid( COLOR(bg), COLOR(fg), 40, 1),
            cIndi = Colors::mid( COLOR(bg), COLOR(fg), 6, 1),
            cIndiOpen = Colors::mid( COLOR(bg), COLOR(fg) );

    if ((lv->activeSubControls & SC_All) && (lv->subControls & SC_Q3ListViewExpand))
    {
        c = 2;
        dotlines.resize(2);
        dotlines[0] = QPoint(lv->rect.right(), lv->rect.top());
        dotlines[1] = QPoint(lv->rect.right(), lv->rect.bottom());
    }
    else
    {
        int linetop = 0, linebot = 0;
        // each branch needs at most two lines, ie. four end points
//         dotoffset = (item.itemY + item.height - y) % 2;
        dotlines.resize(item.childCount * 4);
        c = 0;

        // skip the stuff above the exposed rectangle
        for (i = 1; i < lv->items.size(); ++i)
        {
            QStyleOptionQ3ListViewItem child = lv->items.at(i);
            if (child.height + y > 0)
                break;
            y += child.totalHeight;
        }
        int bx = lv->rect.width() / 2;

        painter->setPen(Qt::NoPen);
        while (i < lv->items.size() && y < lv->rect.height())
        {   // paint stuff in the magical area
            QStyleOptionQ3ListViewItem child = lv->items.at(i);
            if (child.features & QStyleOptionQ3ListViewItem::Visible)
            {
                int lh;
                if (!(item.features & QStyleOptionQ3ListViewItem::MultiLine))
                    lh = child.height;
                else
                    lh = painter->fontMetrics().height() + 2 * lv->itemMargin;
                    lh = qMax(lh, QApplication::globalStrut().height());
                if (lh % 2 > 0)
                    ++lh;
                linebot = y + lh / 2;
                if (child.features & QStyleOptionQ3ListViewItem::Expandable ||
                    (child.childCount > 0 && child.height > 0))
                {
                    if (child.state & State_Open)
                    {
                        if (child.state & State_Selected)
                            painter->setBrush(FCOLOR(HighlightedText));
                        else
                            painter->setBrush(cIndiOpen);
                        if (option->direction == Qt::RightToLeft)
                            drawSolidArrow(Navi::SW, QRect(bx - 4, linebot - 4, 9, 9), painter);
                        else
                            drawSolidArrow(Navi::SE, QRect(bx - 4, linebot - 4, 9, 9), painter);
                    }
                    else
                    {
                        if (child.state & State_Selected)
                            painter->setBrush(FCOLOR(HighlightedText));
                        else
                            painter->setBrush(cIndi);
                        if (option->direction == Qt::RightToLeft)
                            drawArrow(Navi::W, QRect(bx - 4, linebot - 4, 9, 9), painter);
                        else
                            drawArrow(Navi::E, QRect(bx - 4, linebot - 4, 9, 9), painter);
                    }
                    // dotlinery
                    painter->setPen(cLine);
                    dotlines[c++] = QPoint(bx, linetop);
                    dotlines[c++] = QPoint(bx, linebot - 6);
                    dotlines[c++] = QPoint(bx + 5, linebot);
                    dotlines[c++] = QPoint(lv->rect.width(), linebot);
                    linetop = linebot + 11;
                    painter->setPen(Qt::NoPen);
                }
                else
                {   // just dotlinery
                    dotlines[c++] = QPoint(bx+1, linebot -1);
                    dotlines[c++] = QPoint(lv->rect.width(), linebot -1);
                }
                y += child.totalHeight;
            }
            ++i;
        }
        // Expand line height to edge of rectangle if there's any
        // visible child below
        while (i < lv->items.size() && lv->items.at(i).height <= 0)
            ++i;
        if (i < lv->items.size())
            linebot = lv->rect.height();

        if (linetop < linebot)
        {
            dotlines[c++] = QPoint(bx, linetop);
            dotlines[c++] = QPoint(bx, linebot);
        }
    }
    painter->setPen(cLine);

    int line; // index into dotlines
    if (lv->subControls & SC_Q3ListViewBranch)
    {
        for (line = 0; line < c; line += 2)
        {
            // assumptions here: lines are horizontal or vertical.
            // lines always start with the numerically lowest
            // coordinate.

            // point ... relevant coordinate of current point
            // end ..... same coordinate of the end of the current line
            // other ... the other coordinate of the current point/line
            if (dotlines[line].y() == dotlines[line+1].y())
            {
                int end = dotlines[line + 1].x(), point = dotlines[line].x(), other = dotlines[line].y();
                int i;
                while (point < end)
                {
                    i = 128;
                    if (i + point > end)
                        i = end - point;
                    painter->drawLine(point, other, point+i, other);
                    point += i;
                }
            }
            else
            {
                int end = dotlines[line + 1].y(), point = dotlines[line].y(), other = dotlines[line].x();
                int i;
                while(point < end)
                {
                    i = 128;
                    if (i + point > end)
                        i = end - point;
                    painter->drawLine(other, point, other, point+i);
                    point += i;
                }
            }
        }
    }
#endif
}
//    case PE_Q3CheckListController: // Qt 3 compatible Controller part of a list view item.

void
Style::drawRubberBand(const QStyleOption *option, QPainter *painter, const QWidget*) const
{
    const QBrush oldBrush(painter->brush());
    const QPen oldPen(painter->pen());
    const QPainter::RenderHints oldHints(painter->renderHints());

    QColor c = FCOLOR(Highlight);
    painter->setPen(c);
    c.setAlpha(80);
    painter->setBrush(c);
//     painter->setBrush(QBrush(c, Qt::Dense6Pattern));
//     painter->setBrush(Qt::NoBrush);
// //     painter->setCompositionMode(QPainter::RasterOp_NotSourceXorDestination);
    painter->setRenderHint(QPainter::Antialiasing, false);
    painter->drawRect(RECT.adjusted(0,0,-1,-1));
    painter->setPen(oldPen);
    painter->setBrush(oldBrush);
    painter->setRenderHints(oldHints, true);
}

enum IVI_Flags { Crumb = 1, DolphinDetail = 2 };

static const QWidget *last_widget = 0;
static int last_flags = 0;

static void updateLastWidget( const QWidget *widget, QPainter */*p*/ )
{
    if (widget != last_widget)
    {
        last_widget = widget;
        last_flags = 0;
        if (qobject_cast<const QPushButton*>(widget))
        {
            if (widget->inherits("KUrlButton") && !widget->inherits("KFilePlacesSelector"))
                last_flags |= Crumb;
            else if (widget->inherits("BreadcrumbItemButton"))
                last_flags |= Crumb;
        }
    }
}

void
Style::drawItem(const QStyleOption *option, QPainter *painter, const QWidget *widget, bool isItem) const
{
    // kwin tabbox, painting plasma and animation - this looks really lousy :-(
    if ( appType == KWin &&
         widget && widget->parentWidget() &&
         widget->parentWidget()->inherits("KWin::TabBox::TabBoxView") )
        return;

    ASSURE_OPTION(item, ViewItemV4);

    updateLastWidget( widget, painter );

    if (widget && (last_flags & Crumb))
        return;

    OPT_HOVER

    const QAbstractItemView *view = qobject_cast<const QAbstractItemView *>(widget);
    hover = hover && (!view || view->selectionMode() != QAbstractItemView::NoSelection);
    bool selected = item->state & QStyle::State_Selected;
    const QWidget *viewport = 0;
    if (view)
        viewport = view->viewport();
    else if (!widget && painter->device())
    {   // search the widget from the painter =P
        if (painter->device()->devType() == QInternal::Widget)
            widget = static_cast<QWidget*>(painter->device());
        else
        {
            QPaintDevice *dev = QPainter::redirected(painter->device());
            if (dev && dev->devType() == QInternal::Widget)
                widget = static_cast<QWidget*>(dev);
        }
        if (widget && widget->objectName() == "qt_scrollarea_viewport")
            viewport = widget;
    }

    QPalette::ColorRole bg = QPalette::Base, fg = QPalette::Text;
    if (viewport)
    {
        if (viewport->autoFillBackground())
            { bg = viewport->backgroundRole(); fg = viewport->foregroundRole(); }
        else
            { bg = QPalette::Window; fg = QPalette::WindowText; }
    }
    else if (widget)
        { bg = widget->backgroundRole(); fg = widget->foregroundRole(); }

    bool strongSelect = false;
    if (bg == QPalette::Window)
    {
        strongSelect = true;
        if (config.bg.modal.invert && widget && widget->window()->isModal())
            { bg = QPalette::WindowText; fg = QPalette::Window; }
    }

   // this could just lead to cluttered listviews...?!^
//    QPalette::ColorGroup cg = item->state & QStyle::State_Enabled ? QPalette::Normal : QPalette::Disabled;
//    if (cg == QPalette::Normal && !(item->state & QStyle::State_Active))
//       cg = QPalette::Inactive;

    if (!isItem && (hover || selected))
    {   // dolphin constrains selection to the text and really REALLY hates a non winblows look :-(
        // TODO: write a better finder clone...
        if (view && !qstrcmp(view->metaObject()->className(), "DolphinDetailsView"))
            hover = selected = false;
    }
    if (hover || selected)
    {
        const QTreeView *tree = qobject_cast<const QTreeView*>(view);
        const bool single =  tree || (view && view->selectionMode() == QAbstractItemView::SingleSelection);
        bool round = !tree; // looks ultimatly CRAP!

        switch (item->viewItemPosition)
        {
            default:
            case QStyleOptionViewItemV4::Invalid:
                if (round) round = strongSelect;
                break;
            case QStyleOptionViewItemV4::OnlyOne:
                break;
            case QStyleOptionViewItemV4::Beginning:
                if (round) Tile::setShape(Tile::Full & ~Tile::Left);
                break;
            case QStyleOptionViewItemV4::Middle:
                round = false; break;
            case QStyleOptionViewItemV4::End:
                if (round) Tile::setShape(Tile::Full & ~Tile::Left);
                break;
        }
//         if  (item->viewItemPosition != QStyleOptionViewItemV4::OnlyOne)
//             round = false;

        Gradients::Type gt = Gradients::None;
        if (round)
        {
            if (!strongSelect)
                gt = hover ? Gradients::Button : Gradients::Sunken;
            else if (selected)
                gt = Gradients::Button;
        }
        else if (selected && single)
            gt =  Gradients::Button;

        if (gt == Gradients::None)
        {
            const int contrast = qMax(1, Colors::contrast(FCOLOR(Highlight), COLOR(fg)));
            const QColor high = selected ? FCOLOR(Highlight) : Colors::mid(COLOR(bg), FCOLOR(Highlight), 100/contrast, 4);
            if (round)
                masks.rect[true].render(RECT, painter, high);
            else
                painter->fillRect(RECT, high);
        }
        else
        {
            const QPixmap &fill = Gradients::pix(FCOLOR(Highlight), RECT.height(), Qt::Vertical, gt);
            if (round)
            {
                masks.rect[true].render(RECT, painter, fill);
//                 painter->drawTiledPixmap(RECT, fill);
                if (selected && strongSelect)
                    shadows.sunken[true][true].render(RECT, painter);
            }
            else
                painter->drawTiledPixmap(RECT, fill);
        }
        // try to convince the itemview to use the proper fg color, WORKAROUND (kcategorizedview, mainly)
        if (selected)
            painter->setPen(FCOLOR(HighlightedText));
        else
            painter->setPen(COLOR(fg));
        Tile::reset();
    }
    else
    {
        if (item->backgroundBrush.style() != Qt::NoBrush)
        {
            QPoint oldBO = painter->brushOrigin();
            painter->setBrushOrigin(RECT.topLeft());
            painter->fillRect(RECT, item->backgroundBrush);
            painter->setBrushOrigin(oldBO);
        }
        else if (item->features & QStyleOptionViewItemV2::Alternate)
        {
            if (bg == QPalette::Base)
                painter->fillRect(RECT, PAL.brush(QPalette::AlternateBase));
            else
                painter->fillRect(RECT, Colors::mid(COLOR(bg), COLOR(fg), 100, 8));
        }
        // reset the painter for normal items. our above workaround otherwise might kill things...
        painter->setPen(COLOR(fg));
    }
}
