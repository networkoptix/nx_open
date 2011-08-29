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

#include <QListView>
#include <QTableView>
#include <QTreeView>
#include <QTextEdit>

#if QT_VERSION >= 0x040500
#include <QStyleOptionFrameV3>
#endif

#include "visualframe.h"
#include "draw.h"

#include "debug.h"

#include <QtDebug>

static const QWidget *last_widget = 0;
static bool last_isSpecialFrame = false;

bool
Style::isSpecialFrame(const QWidget *widget)
{
    if (!widget)
        return false;
    if ( widget == last_widget )
        return last_isSpecialFrame;
    last_widget = widget;
    if (appType == Opera)
        return (last_isSpecialFrame = true);
    if IS_HTML_WIDGET
        return (last_isSpecialFrame = true);
    if (const QListView *view = qobject_cast<const QListView*>(widget))
    {
        last_isSpecialFrame = (view->viewMode() == QListView::IconMode || view->inherits("KCategorizedView"));
        return last_isSpecialFrame;
    }
    return (last_isSpecialFrame = bool(qobject_cast<const QTextEdit*>(widget)));
//     || (w->parentWidget() && w->parentWidget()->inherits("KateView")); // kate repaints the frame anyway
}

void
Style::drawFocusFrame(const QStyleOption *option, QPainter *painter, const QWidget *w) const
{
    if (option->state & State_Selected || option->state & State_MouseOver)
        return; // looks crap...
    if ( w && w->style() != this && w->inherits("QAbstractButton"))
        return; // from QtCssStyle...

    painter->save();
    painter->setBrush(Qt::NoBrush);
    painter->setPen(FCOLOR(Highlight));
    painter->drawLine(RECT.bottomLeft(), RECT.bottomRight());
    painter->restore();
}

void
Style::drawFrame(const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    OPT_SUNKEN OPT_ENABLED OPT_FOCUS
    bool raised = option->state & State_Raised;

#if QT_VERSION >= 0x040500
    // lines via CE_ShapedFrame instead the ugly eventfilter override
    if HAVE_OPTION(v3frame, FrameV3)
    {
        if (v3frame->frameShape == QFrame::NoFrame)
            return;
        const bool v = (v3frame->frameShape == QFrame::VLine);
        if (v || v3frame->frameShape == QFrame::HLine)
        {
            shadows.line[v][sunken?Sunken:raised?Raised:Relief].render(RECT, painter);
            return;
        }
        if ( v3frame->frameShape == QFrame::Box )
            sunken = raised = false;
    }
#endif
    // all other frame kinds look the same
    if (!widget || (appType == GTK))
    {   // fallback, we cannot paint shaped frame contents
        if (sunken)
            shadows.fallback.render(RECT,painter);
        else if (raised)
            shadows.fallback.render(RECT,painter);
            // TODO !
//          shadows.raised.render(RECT,painter);
        else
        {
            //horizontal
            shadows.line[false][Sunken].render(RECT, painter, Tile::Full, false);
            shadows.line[false][Sunken].render(RECT, painter, Tile::Full, true);
            //vertical
            shadows.line[true][Sunken].render(RECT, painter, Tile::Full, false);
            shadows.line[true][Sunken].render(RECT, painter, Tile::Full, true);
        }
        return;
    }

    const QColor *brush = 0;
    QRect rect = RECT;
    bool fastFrame = false;
    if (qobject_cast<const QFrame*>(widget))
    {   // frame, can be killed unless...
        fastFrame = isSpecialFrame(widget);
        if (fastFrame)
        {   // ...TextEdit, ...
            if (const QAbstractItemView *view = qobject_cast<const QAbstractItemView*>(widget))
            if (view->viewport())
                brush = &view->viewport()->palette().color(view->viewport()->backgroundRole());
            if (!brush)
                brush = &PAL.color(QPalette::Base);
        }
        else
        {   // usually painted on visual frame, but...
            if (config.menu.shadow && widget->inherits("QComboBoxPrivateContainer"))
            {   // a decent combobox dropdown frame...
                SAVE_PEN;
                painter->setPen(Colors::mid(FCOLOR(Base),FCOLOR(Text),4,1));
                painter->drawRect(RECT.adjusted(0,0,-1,-1));
                RESTORE_PEN;
            }
            return;
        }
    }

    if (sunken)
        rect.setBottom(rect.bottom() - F(2));
    else if (raised)
        rect.adjust(F(2), F(1), -F(2), -F(4));
    else
        rect.adjust(F(2), F(2), -F(2), -F(2));

    const Tile::Set *mask = &masks.rect[false], *shadow = 0L;
    if (sunken)
        shadow = &shadows.sunken[false][isEnabled];
    else if (raised)
        shadow = &shadows.group;

    if (brush)
    {
        QRegion rgn = painter->clipRegion();
        bool hadClip = painter->hasClipping();
        painter->setClipRegion(QRegion(RECT) - RECT.adjusted(F(4),F(4),-F(4),-F(4)));
        mask->render(rect, painter, *brush);
        if (hadClip)
            painter->setClipRegion(rgn);
        else
            painter->setClipping(false);
    }
    if (shadow)
        shadow->render(RECT, painter);
    else
    {   // plain frame
        //horizontal
        shadows.line[0][Sunken].render(RECT, painter, Tile::Full, false);
        shadows.line[0][Sunken].render(RECT, painter, Tile::Full, true);
        //vertical
        shadows.line[1][Sunken].render(RECT, painter, Tile::Full, false);
        shadows.line[1][Sunken].render(RECT, painter, Tile::Full, true);
    }
    if (hasFocus)
    {
        rect = RECT;
        if (!fastFrame)
        if (const VisualFramePart* vfp = qobject_cast<const VisualFramePart*>(widget))
        {   // Looks somehow dull if a views header get's surrounded by the focus, ...but it
            // still should inside the frame: don't dare!
            Tile::setShape(Tile::Ring);
            QWidget *vHeader = 0, *hHeader = 0;
            if (const QTreeView* tv = qobject_cast<const QTreeView*>(vfp->frame()))
                hHeader = (QWidget*)tv->header();
            else if (const QTableView* table = qobject_cast<const QTableView*>(vfp->frame()))
            {
                hHeader = (QWidget*)table->horizontalHeader();
                vHeader = (QWidget*)table->verticalHeader();
            }
            if (vHeader && vHeader->isVisible())
            {
                Tile::setShape(Tile::shape() & ~Tile::Left);
                rect.setLeft(rect.left() + vHeader->width());
            }
            if (hHeader && hHeader->isVisible())
            {
                Tile::setShape(Tile::shape() & ~Tile::Top);
                rect.setTop(rect.top() + hHeader->height());
            }
        }
        lights.glow[false].render(rect, painter, FCOLOR(Highlight));
        Tile::reset();
    }
}

void
Style::drawGroupBox(const QStyleOptionComplex *option, QPainter *painter, const QWidget *widget) const
{
    ASSURE_OPTION(groupBox, GroupBox);
    OPT_ENABLED

    // Frame
    if (groupBox->subControls & QStyle::SC_GroupBoxFrame)
    {
        QStyleOptionFrameV2 frame;
        frame.QStyleOption::operator=(*groupBox);
        frame.features = groupBox->features;
        frame.lineWidth = groupBox->lineWidth;
        frame.midLineWidth = groupBox->midLineWidth;
        frame.rect = subControlRect(CC_GroupBox, option, SC_GroupBoxFrame, widget);
        drawGroupBoxFrame(&frame, painter, widget);
    }

    // Title
    if ((groupBox->subControls & QStyle::SC_GroupBoxLabel) && !groupBox->text.isEmpty())
    {
        QColor textColor = groupBox->textColor;
        QPalette::ColorRole role = QPalette::WindowText;
        // NOTICE, WORKAROUND: groupBox->textColor is black by def. and should be invalid - but it's not
        // so assuming everything is optimized for a black on white world, we assume the
        // CUSTOM groupBox->textColor to be only valid if it's != Qt::black
        // THIS IS A HACK!
        if (textColor.isValid() && textColor != Qt::black)
        {
            if (!isEnabled)
                textColor.setAlpha(48);
            painter->setPen(textColor);
            role = QPalette::NoRole;
        }
        setTitleFont(painter, groupBox->text, RECT.width());
        QStyleOptionGroupBox copy = *groupBox;
        copy.fontMetrics = QFontMetrics(painter->font());
        QRect textRect = subControlRect(CC_GroupBox, &copy, SC_GroupBoxLabel, widget);
        drawItemText(painter, textRect, BESPIN_MNEMONIC, groupBox->palette, isEnabled, groupBox->text, role);
        if (groupBox->features & QStyleOptionFrameV2::Flat)
        {
            Tile::PosFlags pf = Tile::Center;
            if (option->direction == Qt::LeftToRight)
            {
                textRect.setLeft(RECT.left());
                textRect.setRight(textRect.right() + (RECT.right()-textRect.right())/2);
                pf |= Tile::Right;
            }
            else
            {
                textRect.setRight(RECT.right());
                textRect.setLeft(textRect.left() - (textRect.left() - RECT.left())/2);
                pf |= Tile::Left;
            }
            shadows.line[0][Sunken].render(textRect, painter, pf, true);
        
//             const int x = textRect.right();
//             textRect.setRight(RECT.right()); textRect.setLeft(x);
//             shadows.line[0][Sunken].render(textRect, painter, Tile::Center | Tile::Right, true);
        }
        else if (config.groupBoxMode)
        {
            const int x = textRect.width()/8;
            textRect.adjust(x,0,-x,0);
            shadows.line[0][Sunken].render(textRect, painter, Tile::Full, true);
        }
    }
       
    // Checkbox
    // TODO: doesn't hover - yet.
    if (groupBox->subControls & SC_GroupBoxCheckBox)
    {
        QStyleOptionButton box;
        box.QStyleOption::operator=(*groupBox);
        box.rect = subControlRect(CC_GroupBox, option, SC_GroupBoxCheckBox, widget);
//       box.state |= State_HasFocus; // focus to signal this to the user
        if (groupBox->activeSubControls & SC_GroupBoxCheckBox)
            box.state |= State_MouseOver;
        drawRadio(&box, painter, 0L);
    }
}

void
Style::drawGroupBoxFrame(const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    const QStyleOptionFrameV2 *groupBox = qstyleoption_cast<const QStyleOptionFrameV2 *>(option);

    if (groupBox && groupBox->features == QStyleOptionFrameV2::Flat)
    {
        const QRect r = (option->direction == Qt::LeftToRight) ?
                        RECT.adjusted(RECT.width()/2,0,0,0) : RECT.adjusted(0,0,-RECT.width()/2,0);
        shadows.line[0][Sunken].render(r, painter, Tile::Full, true);
//         Tile::setShape(Tile::Bottom);
//         shadows.relief[true][false].render(RECT, painter);
//         Tile::reset();
        return;
    }
    if (config.groupBoxMode)
    {
        QRect rect = RECT.adjusted(F(4), F(2), -F(4), 0);
        rect.setHeight(qMin(2*F(32), RECT.height()));
        Tile::setShape(Tile::Full & ~Tile::Bottom);
        if ( config.groupBoxMode != 3 )
            masks.rect[false].render(rect, painter, Gradients::light(rect.height()) );
        rect.setBottom(RECT.bottom()-F(32));
        Tile::setShape(Tile::Full);
        shadows.group.render(RECT, painter);
        Tile::reset();
    }
    else
    {
#if BESPIN_ARGB_WINDOWS
        if (config.bg.opacity != 0xff)
            masks.rect[false].render( RECT.adjusted(0,0,0,-F(2)), painter, QColor(0,0,0,48) );
        else
#endif
            if (config.bg.mode == Scanlines && !(widget && widget->window() && widget->window()->testAttribute(Qt::WA_MacBrushedMetal)))
            masks.rect[false].render( RECT.adjusted(0,0,0,-F(2)), painter,
                                      Gradients::structure(FCOLOR(Window).darker(108)),
                                      widget ? widget->mapTo(widget->window(), RECT.topLeft()) : QPoint() );
        else
            masks.rect[false].render( RECT.adjusted(0,0,0,-F(2)), painter, FCOLOR(Window).darker(105));
        shadows.sunken[false][true].render(RECT, painter);
    }
}
