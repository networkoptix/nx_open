// Modified by Network Optix, Inc. Original copyright notice follows.

/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWidgets module of the Qt Toolkit.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
**
****************************************************************************/

#include "style_base_private.h"

#include <QtGui/QPainter>
#include <QtWidgets/QStyle>

#include <nx/vms/client/desktop/style/helper.h>

namespace {

QWindow* qt_getWindow(const QWidget* widget)
{
    return widget ? widget->window()->windowHandle() : 0;
}

}

StyleBasePrivate::StyleBasePrivate(): QCommonStylePrivate()
{
}

StyleBasePrivate::~StyleBasePrivate()
{
}

int StyleBasePrivate::sliderPositionFromValue(
    int min, int max, int logicalValue, int span, bool upsideDown)
{
    // Prevent slider from shaking because of precision issue.
    if ((!upsideDown && logicalValue == max) || (upsideDown && logicalValue == min))
        return span;

    return QCommonStyle::sliderPositionFromValue(min, max, logicalValue, span, upsideDown);
}

void StyleBasePrivate::drawArrowIndicator(const QStyle* style,
    const QStyleOptionToolButton* toolbutton,
    const QRect& rect,
    QPainter* painter,
    const QWidget* widget)
{
    QStyle::PrimitiveElement pe;
    switch (toolbutton->arrowType)
    {
        case Qt::LeftArrow:
            pe = QStyle::PE_IndicatorArrowLeft;
            break;
        case Qt::RightArrow:
            pe = QStyle::PE_IndicatorArrowRight;
            break;
        case Qt::UpArrow:
            pe = QStyle::PE_IndicatorArrowUp;
            break;
        case Qt::DownArrow:
            pe = QStyle::PE_IndicatorArrowDown;
            break;
        default:
            return;
    }
    QStyleOption arrowOpt = *toolbutton;
    arrowOpt.rect = rect;
    style->drawPrimitive(pe, &arrowOpt, painter, widget);
}

void StyleBasePrivate::drawToolButton(
        QPainter* painter,
        const QStyleOptionToolButton* option,
        const QWidget* widget) const
{
    Q_Q(const QCommonStyle);

    QRect rect = option->rect;
    int shiftX = 0;
    int shiftY = 0;
    if (option->state & (QStyle::State_Sunken | QStyle::State_On))
    {
        shiftX = q->pixelMetric(QStyle::PM_ButtonShiftHorizontal, option, widget);
        shiftY = q->pixelMetric(QStyle::PM_ButtonShiftVertical, option, widget);
    }
    // Arrow type always overrules and is always shown
    bool hasArrow = option->features & QStyleOptionToolButton::Arrow;
    if (((!hasArrow && option->icon.isNull()) && !option->text.isEmpty())
        || option->toolButtonStyle == Qt::ToolButtonTextOnly)
    {
        int alignment = Qt::AlignCenter | Qt::TextShowMnemonic;
        if (!q->styleHint(QStyle::SH_UnderlineShortcut, option, widget))
            alignment |= Qt::TextHideMnemonic;
        rect.translate(shiftX, shiftY);
        painter->setFont(option->font);
        q->drawItemText(painter,
            rect,
            alignment,
            option->palette,
            option->state & QStyle::State_Enabled,
            option->text,
            QPalette::ButtonText);
    }
    else
    {
        QPixmap pm;
        QSize pmSize = option->iconSize;
        if (!option->icon.isNull())
        {
            QIcon::State state = option->state & QStyle::State_On ? QIcon::On : QIcon::Off;
            QIcon::Mode mode;
            if (!option->state.testFlag(QStyle::State_Enabled))
            {
                mode = QIcon::Disabled;
            }
            else if (option->state.testFlag(QStyle::State_MouseOver)
                && option->state.testFlag(QStyle::State_AutoRaise))
            {
                mode = QIcon::Active;
            }
            else
            {
                mode = QIcon::Normal;
            }

            /*
             * The only difference is here. Qt code uses rect.size() instead of iconSize here, so
             * invalid icon rect was returned on hidpi screens (e.g. 24x24 instead on 20x20).
             */
            pm = option->icon.pixmap(qt_getWindow(widget), option->iconSize, mode, state);
            pmSize = pm.size() / pm.devicePixelRatio();
        }

        if (option->toolButtonStyle != Qt::ToolButtonIconOnly)
        {
            painter->setFont(option->font);
            QRect pr = rect, tr = rect;
            int alignment = Qt::TextShowMnemonic;
            if (!q->styleHint(QStyle::SH_UnderlineShortcut, option, widget))
                alignment |= Qt::TextHideMnemonic;

            if (option->toolButtonStyle == Qt::ToolButtonTextUnderIcon)
            {
                pr.setHeight(pmSize.height() + 6);
                tr.adjust(0, pr.height() - 1, 0, -1);
                pr.translate(shiftX, shiftY);
                if (!hasArrow)
                {
                    q->drawItemPixmap(painter, pr, Qt::AlignCenter, pm);
                }
                else
                {
                    drawArrowIndicator(q, option, pr, painter, widget);
                }
                alignment |= Qt::AlignCenter;
            }
            else
            {
                shiftX += widget->contentsMargins().left();
                pr.setWidth(pmSize.width() + 8);
                tr.adjust(pr.width() + nx::style::Metrics::kInterSpace, 0, 0, 0);
                pr.translate(shiftX, shiftY);
                if (!hasArrow)
                {
                    q->drawItemPixmap(painter,
                        QStyle::visualRect(option->direction, rect, pr),
                        Qt::AlignCenter,
                        pm);
                }
                else
                {
                    drawArrowIndicator(q, option, pr, painter, widget);
                }
                alignment |= Qt::AlignLeft | Qt::AlignVCenter;
            }
            tr.translate(shiftX, shiftY);
            q->drawItemText(painter,
                QStyle::visualRect(option->direction, rect, tr),
                alignment,
                option->palette,
                option->state & QStyle::State_Enabled,
                option->text,
                QPalette::ButtonText);
        }
        else
        {
            rect.translate(shiftX, shiftY);
            for (const auto& child: widget->children())
            {
                const auto& childWidget = qobject_cast<QWidget*>(child);
                if (childWidget && childWidget->isVisible())
                {
                    rect.setWidth(
                        rect.width() -
                        childWidget->width() +
                        childWidget->contentsMargins().right());
                }
            }
            if (hasArrow)
                drawArrowIndicator(q, option, rect, painter, widget);
            else
                q->drawItemPixmap(painter, rect, Qt::AlignCenter, pm);
        }
    }
}

void StyleBasePrivate::drawSliderTickmarks(
    QPainter* painter,
    const QStyleOptionComplex* option,
    const QWidget* widget) const
{
    Q_Q(const QCommonStyle);

    if (auto slider = qstyleoption_cast<const QStyleOptionSlider*>(option))
    {
        const bool horizontal = slider->orientation == Qt::Horizontal;

        if (slider->subControls.testFlag(QStyle::SC_SliderTickmarks))
        {
            bool ticksAbove = slider->tickPosition & QSlider::TicksAbove;
            bool ticksBelow = slider->tickPosition & QSlider::TicksBelow;

            painter->setPen(slider->palette.color(QPalette::Midlight));
            int tickSize = q->proxy()->pixelMetric(QStyle::PM_SliderTickmarkOffset, option, widget);
            int available = q->proxy()->pixelMetric(QStyle::PM_SliderSpaceAvailable, slider, widget);
            int interval = slider->tickInterval;

            if (interval <= 0)
            {
                interval = slider->singleStep;

                if (QStyle::sliderPositionFromValue(
                        slider->minimum, slider->maximum, interval, available)
                        - QStyle::sliderPositionFromValue(
                            slider->minimum, slider->maximum, 0, available)
                    < 3)
                {
                    interval = slider->pageStep;
                }
            }
            if (interval <= 0)
                interval = 1;

            int v = slider->minimum;
            int len = q->proxy()->pixelMetric(QStyle::PM_SliderLength, slider, widget);

            while (v <= slider->maximum + 1)
            {
                if (v == slider->maximum + 1 && interval == 1)
                    break;

                const int v_ = qMin(v, slider->maximum);
                int pos = sliderPositionFromValue(slider->minimum,
                              slider->maximum,
                              v_,
                              (horizontal ? slider->rect.width() : slider->rect.height()) - len,
                              slider->upsideDown)
                    + len / 2;
                const int extra = 2;

                if (horizontal)
                {
                    if (ticksAbove)
                        painter->drawLine(
                            pos, slider->rect.top() + extra, pos, slider->rect.top() + tickSize);

                    if (ticksBelow)
                        painter->drawLine(pos,
                            slider->rect.bottom() - extra,
                            pos,
                            slider->rect.bottom() - tickSize);
                }
                else
                {
                    if (ticksAbove)
                        painter->drawLine(
                            slider->rect.left() + extra, pos, slider->rect.left() + tickSize, pos);

                    if (ticksBelow)
                        painter->drawLine(slider->rect.right() - extra,
                            pos,
                            slider->rect.right() - tickSize,
                            pos);
                }

                // in the case where maximum is max int
                int nextInterval = v + interval;
                if (nextInterval < v)
                    break;

                v = nextInterval;
            }
        }
    }
}
