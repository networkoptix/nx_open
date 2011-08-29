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

#include <QAbstractButton>
#include <QStyleOptionComboBox>
#include <QStyleOptionMenuItem>
#include <QTabBar>
#include "bespin.h"
#include "makros.h"

#define I_AM_THE_ROB 0

#if I_AM_THE_ROB
    #include <QMainWindow>
    #include <QtDebug>
    #include <QToolBar>
    #include <QToolButton>
#endif

using namespace Bespin;

static const int windowsArrowHMargin = 6; // arrow horizontal margin

#if I_AM_THE_ROB
static int shape( const QWidget *w )
{
    const QToolButton *bt = qobject_cast<const QToolButton*>(w);
    if (!bt)
        return 0;

    QToolBar *bar = 0;
    if ( !( Style::config.btn.tool.connected && bt && (bar = qobject_cast<QToolBar*>(bt->parentWidget())) ) )
        return 0;
    QMainWindow *mwin = qobject_cast<QMainWindow*>(bar->parentWidget());
    if (!mwin)
        return 0;

    int pf = Tile::Full;
    const int d = 1;
    Qt::Orientation o = bar->orientation();
    QRect geo = bt->geometry();
    if (o == Qt::Horizontal)
    {
        if (qobject_cast<QToolButton*>(bar->childAt(geo.x()-d, geo.y())))
            pf &= ~Tile::Left;
        if (qobject_cast<QToolButton*>(bar->childAt(geo.right()+d, geo.y())))
            pf &= ~Tile::Right;
    }
    else
    {
        if (qobject_cast<QToolButton*>(bar->childAt(geo.x(), geo.y()-d)))
            pf &= ~Tile::Top;
        if (qobject_cast<QToolButton*>(bar->childAt(geo.x(), geo.bottom()+d)))
            pf &= ~Tile::Bottom;
    }

    return pf;
}
#endif

QSize
Style::sizeFromContents(ContentsType ct, const QStyleOption *option, const QSize &contentsSize, const QWidget *widget) const
{
    switch ( ct )
    {
    case CT_CheckBox: // A check box, like QCheckBox
    case CT_RadioButton: // A radio button, like QRadioButton
        if HAVE_OPTION(btn, Button)
        {
            int w, h;
            w = h = qMax( Dpi::target.Indicator, Dpi::target.ExclusiveIndicator );
            h = qMax(h, contentsSize.height() + 4);

            int margin = 0;
            // we add 4 pixels for label margins
            if (btn->icon.isNull() || !btn->text.isEmpty())
                margin = F(7);
            return QSize(contentsSize.width() + w + margin, h);
        }
    case CT_ComboBox: // A combo box, like QComboBox
        if HAVE_OPTION(cb, ComboBox)
        {
            if (cb->editable)
                return contentsSize + QSize(F(12) + (cb->fontMetrics.ascent() + F(2))*1.1, qMax(F(4) - config.fontExtent,0));

            int hgt = contentsSize.height();
            int d = F(8);
            if ( cb->frame )
            {
                hgt += ((config.btn.fullHover) ? F(4) : F(6)) - config.fontExtent;
                d = F(16);
            }
            hgt += hgt%2;
//             if ( !cb->currentIcon.isNull()) // leads to inequal heights + pot. height changes on item change
//                 hgt += F(2);
            return QSize(contentsSize.width() + d + ( int)(hgt/1.1), qMax(config.btn.minHeight, hgt));
        }
//    case CT_DialogButtons: //
//       return QSize((contentsSize.width()+16 < 80) ? 80 : contentsSize.width()+16, contentsSize.height()+10);
#ifdef QT3_SUPPORT
//    case CT_Q3DockWindow: //
#endif
    case CT_HeaderSection: // A header section, like QHeader
        if HAVE_OPTION(hdr, Header)
        {
            QSize sz;
            int margin = F(2);
            int iconSize = hdr->icon.isNull() ? 0 : pixelMetric(QStyle::PM_SmallIconSize, hdr, widget);
            QSize txt = hdr->fontMetrics.size(0, hdr->text);
            sz.setHeight(qMax(iconSize, txt.height()) + F(5) - config.fontExtent);
            sz.setWidth((iconSize?margin+iconSize:0) + (hdr->text.isNull()?0:margin+txt.width()) + margin);
            return sz;
        }
    case CT_LineEdit: // A line edit, like QLineEdit
        return contentsSize + QSize(F(4), qMax(F(4) - config.fontExtent,0));
    case CT_MenuBarItem:
    {   // A menu bar item, like the buttons in a QMenuBar
        const int h = contentsSize.height() + F(8) - config.fontExtent;
        return QSize(qMax(contentsSize.width()+F(12), h*8/5), h);
    }
   case CT_MenuItem: // A menu item, like QMenuItem
        if HAVE_OPTION(menuItem, MenuItem)
        {
            if (menuItem->menuItemType == QStyleOptionMenuItem::Separator && menuItem->text.isEmpty())
                return QSize(10, F(6));

            bool checkable = menuItem->menuHasCheckableItems;
            int maxpmw = config.menu.showIcons*menuItem->maxIconWidth;
            int w = contentsSize.width();
            int h = qMax(contentsSize.height(), menuItem->fontMetrics.lineSpacing()) + F(4) - config.fontExtent;

            if (config.menu.showIcons && !menuItem->icon.isNull())
                h = qMax(h, menuItem->icon.pixmap(pixelMetric(PM_SmallIconSize), QIcon::Normal).height() + F(4) - config.fontExtent);
            if (menuItem->text.contains('\t'))
                w += F(12);
            if (maxpmw > 0)
                w += maxpmw + F(6);
            if (checkable)
                w += 2*(h - F(4))/3 + F(7);
            w += (checkable + (maxpmw > 0))*F(2);
            w += F(12);
            if (menuItem->menuItemType == QStyleOptionMenuItem::SubMenu)
                w += 2 * windowsArrowHMargin;
            if (menuItem->menuItemType == QStyleOptionMenuItem::DefaultItem)
            {   // adjust the font and add the difference in size.
                // it would be better if the font could be adjusted in the
                // getStyleOptions qmenu func!!
                QFontMetrics fm(menuItem->font);
                QFont fontBold = menuItem->font;
                fontBold.setBold(true);
                QFontMetrics fmBold(fontBold);
                w += fmBold.width(menuItem->text) - fm.width(menuItem->text);
            }
            if (menuItem->menuItemType == QStyleOptionMenuItem::Separator)
                w += 32; // add some space for the separator lines
            return QSize(w, h);
        }
        break;
    case CT_PushButton: // A push button, like QPushButton
        if HAVE_OPTION(btn, Button)
        {
            if (btn->text.isEmpty())
//             3px for shadow & outline + 1px padding -> 4px per side
                return ( QSize( contentsSize.width() + F(8), contentsSize.height() + F(8) ) );
            else
            {
                int w = contentsSize.width() + F(20);
                if (btn->features & QStyleOptionButton::HasMenu)
                    w += contentsSize.height()+F(16);
                else
                    if (widget)
                    if (const QAbstractButton* abn = qobject_cast<const QAbstractButton*>(widget))
                    if (abn->isCheckable())
                        w += contentsSize.height()+F(16);

                int h = (config.btn.layer ? F(6) : F(7)) - config.fontExtent;
                if (!config.btn.fullHover)
                    h += F(4);
                h += contentsSize.height();
                if (!(h%2))
                    ++h;

                if (!btn->icon.isNull())
                    { w += F(10); h += F(2); }

                if (config.btn.round)
                    { w += F(8); /*h -= F(2);*/ }

                if (w < F(80))
                    w = F(80);

                return QSize(w, qMax(config.btn.minHeight, h));
            }
        }
//    case CT_SizeGrip: // A size grip, like QSizeGrip

    case CT_Menu: // A menu, like QMenu
#ifdef QT3_SUPPORT
    case CT_Q3Header: // A Qt 3 header section, like Q3Header
#endif
    case CT_MenuBar: // A menu bar, like QMenuBar
    case CT_ProgressBar: // A progress bar, like QProgressBar
    case CT_Slider: // A slider, like QSlider
    case CT_ScrollBar: // A scroll bar, like QScrollBar
        return contentsSize;
    case CT_SpinBox: // A spin box, like QSpinBox
        return contentsSize - QSize(0, F(2));
//    case CT_Splitter: // A splitter, like QSplitter
    case CT_TabBarTab: // A tab on a tab bar, like QTabBar

        if HAVE_OPTION(tab, Tab)
        {
            const int add = F(9);
            int other = F(2) - config.fontExtent;
#if QT_VERSION >= 0x040500
            if ( appType == Dolphin && widget )
            if ( /*const QTabBar *bar =*/ qobject_cast<const QTabBar*>(widget) )
                other = qMax( 0, 16+F(8)-contentsSize.height() ); // compensate the close buttons
#endif
            switch (tab->shape)
            {
            case QTabBar::RoundedNorth: case QTabBar::TriangularNorth:
            case QTabBar::RoundedSouth: case QTabBar::TriangularSouth:
                return contentsSize + QSize(add, other);
            case QTabBar::RoundedEast: case QTabBar::TriangularEast:
            case QTabBar::RoundedWest: case QTabBar::TriangularWest:
                return contentsSize + QSize(0, add);
            }
        }
        return contentsSize + QSize(F(9), F(9));
    case CT_TabWidget: // A tab widget, like QTabWidget
        return contentsSize; // + QSize(F(8),F(6)); WARNING! this can causes recursive updates! (Qt 4.7 bug?)
    case CT_ToolButton:
    {   // A tool button, like QToolButton
        const QStyleOptionToolButton *toolbutton = qstyleoption_cast<const QStyleOptionToolButton *>(option);
        // get ~goldem mean ratio
        int w, h;
        if (toolbutton && toolbutton->toolButtonStyle == Qt::ToolButtonTextUnderIcon)
            h = contentsSize.height() + F(8);
        else
            h = contentsSize.height() + F(6);
        if (config.btn.tool.connected && !config.btn.tool.frame)
            h += F(4);

#if I_AM_THE_ROB
        w = qMax(contentsSize.width()+F(4), (shape(widget)==Tile::Full)*F(32)+h*4/3-F(4)); // 4/3 - 16/9
#else
        w = qMax(contentsSize.width()+F(4), h*4/3-F(4)); // 4/3 - 16/9
#endif
    //      w = contentsSize.width()+F(8);
        if (toolbutton && hasMenuIndicator(toolbutton))
            w += pixelMetric(PM_MenuButtonIndicator, option, widget)/* + F(4)*/;
        return QSize(w, h);
    }
    default: ;
    } // switch
    return QCommonStyle::sizeFromContents( ct, option, contentsSize, widget );
}
