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
#include <QComboBox>
#include "draw.h"
#include "hacks.h"
#include "animator/hover.h"

void
Style::drawLineEditFrame(const QStyleOption *option, QPainter *painter, const QWidget *) const
{
    // WARNING this is NOT used to draw lineedits - just the frame, see below!!
    OPT_ENABLED OPT_FOCUS

    QRect r = RECT;
    if (appType != GTK)
    {
        r.setBottom(r.bottom() - F(2));
        shadows.sunken[false][isEnabled].render(r, painter);
    }
    else
        shadows.fallback.render(RECT,painter);

    if (hasFocus)
        lights.glow[false].render(RECT, painter, FCOLOR(Highlight));
}

static QPixmap *bgBuffer(const QPalette &pal, const QRect &r)
{
    QPixmap *buffer = new QPixmap(r.size());
    QPainter p(buffer);
    p.setBrush(pal.brush(QPalette::Base));
    p.setPen(Qt::NoPen);
    p.drawRect(buffer->rect());
    p.end();
    return buffer;
}
#include <QtDebug>
void
Style::drawLineEdit(const QStyleOption *option, QPainter *painter, const QWidget *widget, bool round) const
{
    // spinboxes and combos allready have a lineedit as global frame
    // TODO: exclude Q3Combo??
    QWidget *daddy = widget ? widget->parentWidget() : 0L;
    if (qstyleoption_cast<const QStyleOptionFrame*>(option) && static_cast<const QStyleOptionFrame *>(option)->lineWidth < 1)
    {
        if (daddy && ( qobject_cast<QComboBox*>(daddy) || daddy->inherits("QAbstractSpinBox")))
            return;
        painter->fillRect(RECT, FCOLOR(Base));
        return;
    }
    if (Hacks::config.invertDolphinUrlBar && daddy && daddy->inherits("KUrlNavigator"))
        return;

    OPT_ENABLED OPT_FOCUS

//     round = true;
    isEnabled = isEnabled && !(option->state & State_ReadOnly);
    QRect r = RECT;
    if (isEnabled)
    {
        const Tile::Set &mask = masks.rect[round && appType != GTK];
        r.setBottom(r.bottom() - F(2));
        if (PAL.brush(QPalette::Base).style() > 1 ) // pixmap, gradient, whatever
            { QPixmap *buffer = bgBuffer(PAL, r); mask.render(r, painter, *buffer); delete buffer; }
        else if (r.height() > 2*option->fontMetrics.height()) // no lineEdit... like some input frames in QWebKit
        {
            const QColor &c = FCOLOR(Base);
            mask.render(r, painter, (hasFocus && Colors::value(c) < 100) ? c.lighter(112) : c);
        }
        else
            mask.render(r, painter, Gradients::Sunken, Qt::Vertical, FCOLOR(Base));
        if (hasFocus)
            lights.glow[round].render(RECT, painter, FCOLOR(Highlight));
    }
    if (appType == GTK)
        shadows.fallback.render(RECT,painter);
    else
        shadows.sunken[round][isEnabled].render(RECT, painter);
}

static void
drawSBArrow(QStyle::SubControl sc, QPainter *painter, QStyleOptionSpinBox *option,
            const QWidget *widget, const QStyle *style)
{
    if (option->subControls & sc)
    {
        const int f2 = F(2);

        option->subControls = sc;
        RECT = style->subControlRect(QStyle::CC_SpinBox, option, sc, widget);

        Navi::Direction dir = Navi::N;
        QAbstractSpinBox::StepEnabledFlag sef = QAbstractSpinBox::StepUpEnabled;
        if (sc == QStyle::SC_SpinBoxUp)
            RECT.setTop(RECT.bottom() - 2*RECT.height()/3);
        else
        {
            dir = Navi::S; sef = QAbstractSpinBox::StepDownEnabled;
            RECT.setBottom(RECT.top() + 2*RECT.height()/3);
        }

        bool isEnabled = option->stepEnabled & sef;
        bool hover = isEnabled && (option->activeSubControls == (int)sc);
        bool sunken = hover && (option->state & QStyle::State_Sunken);
        

        if (!sunken)
        {
            painter->setBrush(FCOLOR(Base).dark(108));
            RECT.translate(0, f2);
            Style::drawArrow(dir, RECT, painter);
            RECT.translate(0, -f2);
        }

        QColor c;
        if (hover)
            c = FCOLOR(Highlight);
        else if (isEnabled)
            c = Colors::mid(FCOLOR(Base), FCOLOR(Text));
        else
            c = Colors::mid(FCOLOR(Base), PAL.color(QPalette::Disabled, QPalette::Text));

        painter->setBrush(c);
        Style::drawArrow(dir, RECT, painter);
    }
}

void
Style::drawSpinBox(const QStyleOptionComplex * option, QPainter * painter,
                         const QWidget * widget) const
{
    ASSURE_OPTION(sb, SpinBox);
    OPT_ENABLED

    QStyleOptionSpinBox copy = *sb;

   // this doesn't work (for the moment, i assume...)
    //    isEnabled = isEnabled && !(option->state & State_ReadOnly);
    if (isEnabled)
    if (const QAbstractSpinBox *box = qobject_cast<const QAbstractSpinBox*>(widget))
    {
        isEnabled = isEnabled && !box->isReadOnly();
        if (!isEnabled)
            copy.state &= ~State_Enabled;
    }

    if (sb->frame && (sb->subControls & SC_SpinBoxFrame))
        drawLineEdit(&copy, painter, widget);

    if (!isEnabled)
        return; // why bother the user with elements he can't use... ;)

    painter->setPen(Qt::NoPen);
    drawSBArrow(SC_SpinBoxUp, painter, &copy, widget, this);
    copy.rect = RECT;
    copy.subControls = sb->subControls;
    drawSBArrow(SC_SpinBoxDown, painter, &copy, widget, this);
}

static int animStep = -1;
static bool round_ = true;

void
Style::drawComboBox(const QStyleOptionComplex *option, QPainter *painter, const QWidget *widget) const
{
    ASSURE_OPTION(cmb, ComboBox);
    B_STATES
    if ( widget && widget->inherits("WebView") )
    {
        if (!(config.btn.backLightHover || cmb->editable))
        {   // paints hardcoded black text bypassing the style?! grrr...
            QStyleOptionComboBox *_cmb = const_cast<QStyleOptionComboBox*>(cmb);
            _cmb->palette.setColor(config.btn.std_role[Bg], QColor(230,230,230,255));
            _cmb->palette.setColor(config.btn.active_role[Bg], QColor(255,255,255,255));
        }
        widget = 0;
    }

    const int f1 = F(1), f2 = F(2), f3 = F(3);
    QRect ar, r = RECT.adjusted(f1, f1, -f1, -f2);
    const QComboBox *combo = widget ? qobject_cast<const QComboBox*>(widget) : 0;
    QColor c = CONF_COLOR(btn.std, Bg);

    const bool listShown = combo && combo->view() && ((QWidget*)(combo->view()))->isVisible();
    if (listShown) // this messes up hover
        hover = hover || QRect(widget->mapToGlobal(RECT.topLeft()), RECT.size()).contains(QCursor::pos());

    if (isEnabled && (cmb->subControls & SC_ComboBoxArrow) && (!combo || combo->count() > 0))
    {   // do we have an arrow?
        ar = subControlRect(CC_ComboBox, cmb, SC_ComboBoxArrow, widget);
        ar.setBottom(ar.bottom()-f2);
    }

    // the frame
    const Tile::Set &mask = masks.rect[round_];
    if ((cmb->subControls & SC_ComboBoxFrame) && cmb->frame)
    {
        if (cmb->editable)
            drawLineEdit(option, painter, widget, false);
        else
        {
            if (!ar.isNull())
            {
                // ground
                animStep = (appType == GTK || !widget) ? 6*hover : Animator::Hover::step(widget);
                if (listShown)
                    animStep = 6;

                const bool translucent = Gradients::isTranslucent(GRAD(chooser));
                c = btnBg(PAL, isEnabled, hasFocus, animStep, config.btn.fullHover, translucent);
                if (hasFocus)
                {
                    lights.glow[round_].render(RECT, painter, FCOLOR(Highlight));
                    if ( !config.btn.backLightHover )
                    {
                        const int contrast =  (config.btn.fullHover && animStep) ?
                        Colors::contrast(btnBg(PAL, isEnabled, hasFocus, 0, true, translucent), FCOLOR(Highlight)):
                        Colors::contrast(c, FCOLOR(Highlight));
                        if (contrast > 10)
                            c = Colors::mid(c, FCOLOR(Highlight), contrast/4, 1);
                    }
                }

                mask.render(r, painter, GRAD(chooser), ori[1], c);

                if (animStep)
                {
                    if (!config.btn.fullHover)
                    {   // maybe hover indicator?
                        r.adjust(f3, f3, -f3, -f3);
                        c = Colors::mid(c, CONF_COLOR(btn.active, Bg), 6-animStep, animStep);
                        mask.render(r, painter, GRAD(chooser), ori[1], c, RECT.height()-f2, QPoint(0,f3));
                        r = RECT.adjusted(f1, f1, -f1, -f2); // RESET 'r' !!!
                    }
                    else if (config.btn.backLightHover)
                    {
                        QColor c2 = CCOLOR(btn.active, Bg);
                        c2.setAlpha(c2.alpha()*animStep/8);
                        lights.glow[round_].render(RECT, painter, c2);
                    }
                }
            }
            r.setBottom(RECT.bottom());
            shadows.sunken[round_][isEnabled].render(r, painter);
        }
    }

    // the arrow
    if (!ar.isNull())
    {
        if (!(ar.width()%2) )
            ar.setWidth(ar.width()-1);
        const int dy = ar.height()/4;
        QRect rect = ar.adjusted(0, dy, 0, -dy);

        Navi::Direction dir = Navi::S;
        bool upDown = false;
        if (listShown)
            dir = (config.leftHanded) ? Navi::E : Navi::W;
        else if (combo)
        {
            if (combo->currentIndex() == 0)
                dir = Navi::S;
            else if (combo->currentIndex() == combo->count()-1)
                dir = Navi::N;
            else
                upDown = true;
        }

        painter->save();
        painter->setPen(Qt::NoPen);
        if (cmb->editable)
        {
            if (upDown || dir == Navi::N)
                dir = Navi::S;
            upDown = false; // shall never look like spinbox!
            hover = hover && (cmb->activeSubControls == SC_ComboBoxArrow);
            if (!sunken)
            {
                painter->setBrush(FCOLOR(Base).dark(105));
                rect.translate(0, f2);
                drawArrow(dir, rect, painter);
                rect.translate(0, -f2);
            }
            if (hover || listShown)
                painter->setBrush(FCOLOR(Highlight));
            else
                painter->setBrush( Colors::mid(FCOLOR(Base), FCOLOR(Text)) );
        }
        else if (config.btn.backLightHover)
            painter->setBrush(Colors::mid(c, CONF_COLOR(btn.std, Fg), 6-animStep, 3+animStep));
        else
        {
            c = Colors::mid(c, CONF_COLOR(btn.active, Bg));
            c = Colors::mid(c, CONF_COLOR(btn.active, Bg), 6-animStep, animStep);
//          ar.adjust(f2, f3, -f2, -f3);
            mask.render(ar, painter, GRAD(chooser), ori[1], c, RECT.height()-f2, QPoint(0, ar.y() - RECT.y()) );
            painter->setBrush(Colors::mid(c, CONF_COLOR(btn.active, Fg), 1,2));
        }
        if (upDown)
        {
            rect.setBottom(rect.y() + rect.height()/2);
            rect.translate(0, -1);
            drawArrow(Navi::N, rect, painter);
            rect.translate(0, rect.height());
            drawArrow(Navi::S, rect, painter);
        }
        else
        {
            if (dir == Navi::N) // loooks unbalanced otherwise
                rect.translate(0, -f1);
            drawArrow(dir, rect, painter);
        }
        painter->restore();
    }
}


void
Style::drawComboBoxLabel(const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    ASSURE_OPTION(cb, ComboBox);
    OPT_ENABLED

    QRect editRect = subControlRect(CC_ComboBox, cb, SC_ComboBoxEditField, widget);
    painter->save();
    painter->setClipRect(editRect);

    if (!(cb->currentIcon.isNull() || cb->iconSize.isNull()))
    {   // icon ===============================================
        QIcon::Mode mode = isEnabled ? QIcon::Normal : QIcon::Disabled;
        QPixmap pixmap = cb->currentIcon.pixmap(cb->iconSize, mode);
        QRect iconRect(editRect);
        iconRect.setWidth(cb->iconSize.width() + 4);
        iconRect = alignedRect( cb->direction, Qt::AlignLeft | Qt::AlignVCenter, iconRect.size(), editRect);
//       if (cb->editable)
//          painter->fillRect(iconRect, opt->palette.brush(QPalette::Base));
        drawItemPixmap(painter, iconRect, Qt::AlignCenter, pixmap);

        if (cb->direction == Qt::LeftToRight)
            editRect.setLeft(editRect.left() + cb->iconSize.width() + 4);
        else
            editRect.setRight(editRect.right() - (cb->iconSize.width() + 4));
    }
    
    if (!cb->currentText.isEmpty() && !cb->editable)
    {   // text ==================================================
        if (cb->frame)
        {
            OPT_FOCUS
            if (animStep < 0)
            {
                OPT_HOVER
                animStep = hover ? 6 : 0;
            }
            else
            {
                if (const QComboBox* combo = qobject_cast<const QComboBox*>(widget))
                if (combo->view() && ((QWidget*)(combo->view()))->isVisible())
                    animStep = 6;
            }
            editRect.adjust(F(3),0, -F(3), 0);
            painter->setPen(btnFg(PAL, isEnabled, hasFocus, animStep));
        }
        int tf = Qt::AlignCenter;
        if ( !((cb->subControls & SC_ComboBoxFrame) && cb->frame) )
            tf = Qt::AlignVCenter | (cb->direction == Qt::LeftToRight ? Qt::AlignLeft : Qt::AlignRight);
        drawItemText(painter, editRect, tf, PAL, isEnabled, cb->currentText);
    }
    painter->restore();
    animStep = -1;
}
