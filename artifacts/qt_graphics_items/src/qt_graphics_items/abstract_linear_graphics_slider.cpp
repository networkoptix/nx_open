// Modified by Network Optix, Inc. Original copyright notice follows.

/****************************************************************************
**
** Copyright (C) 1992-2005 Trolltech AS. All rights reserved.
**
** This file is part of the QtGui module of the Qt Toolkit.
**
** This file may be used under the terms of the GNU General Public
** License version 2.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of
** this file.  Please review the following information to ensure GNU
** General Public Licensing requirements will be met:
** http://www.trolltech.com/products/qt/opensource.html
**
** If you are unsure which license is appropriate for your use, please
** review the following information:
** http://www.trolltech.com/products/qt/licensing.html or contact the
** sales department at sales@trolltech.com.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
****************************************************************************/

#include "abstract_linear_graphics_slider.h"
#include "abstract_linear_graphics_slider_p.h"

#include <QtWidgets/QStyleOptionSlider>

#include <nx/vms/client/desktop/style/graphics_style.h>

using namespace nx::vms::client::desktop;

void AbstractLinearGraphicsSliderPrivate::init()
{
    mapperDirty = true;

    control = QStyle::CC_Slider;
    grooveSubControl = QStyle::SC_SliderGroove;
    handleSubControl = QStyle::SC_SliderHandle;
}

void AbstractLinearGraphicsSliderPrivate::initControls(QStyle::ComplexControl control,
    QStyle::SubControl grooveSubControl,
    QStyle::SubControl handleSubControl)
{
    this->control = control;
    this->grooveSubControl = grooveSubControl;
    this->handleSubControl = handleSubControl;
}

void AbstractLinearGraphicsSliderPrivate::invalidateMapper()
{
    if (mapperDirty)
        return;

    mapperDirty = true;
    q_func()->sliderChange(AbstractLinearGraphicsSlider::SliderMappingChange);
}

void AbstractLinearGraphicsSliderPrivate::ensureMapper() const
{
    if (!mapperDirty)
        return;

    Q_Q(const AbstractLinearGraphicsSlider);

    QStyleOptionSlider opt;
    q->initStyleOption(&opt);
    upsideDown = opt.upsideDown;

    QRect grooveRect = q->style()->subControlRect(control, &opt, grooveSubControl, nullptr);
    QRect handleRect = q->style()->subControlRect(control, &opt, handleSubControl, nullptr);

    if (q->orientation() == Qt::Horizontal)
    {
        pixelPosMin = grooveRect.x();
        pixelPosMax = grooveRect.right() - handleRect.width() + 1;
    }
    else
    {
        pixelPosMin = grooveRect.y();
        pixelPosMax = grooveRect.bottom() - handleRect.height() + 1;
    }

    mapperDirty = false;
}

AbstractLinearGraphicsSlider::AbstractLinearGraphicsSlider(QGraphicsItem* parent):
    base_type(*new AbstractLinearGraphicsSliderPrivate(), parent)
{
    d_func()->init();
}

AbstractLinearGraphicsSlider::AbstractLinearGraphicsSlider(
    AbstractLinearGraphicsSliderPrivate& dd, QGraphicsItem* parent):
    base_type(dd, parent)
{
    d_func()->init();
}

AbstractLinearGraphicsSlider::~AbstractLinearGraphicsSlider()
{
    return;
}

void AbstractLinearGraphicsSlider::updateGeometry()
{
    d_func()->invalidateMapper();

    base_type::updateGeometry();
}

void AbstractLinearGraphicsSlider::resizeEvent(QGraphicsSceneResizeEvent* event)
{
    d_func()->invalidateMapper();

    base_type::resizeEvent(event);
}

void AbstractLinearGraphicsSlider::sliderChange(SliderChange change)
{
    if (change != SliderValueChange && change != SliderMappingChange)
        d_func()->invalidateMapper();

    base_type::sliderChange(change);
}

bool AbstractLinearGraphicsSlider::event(QEvent* event)
{
    if (event->type() == QEvent::StyleChange)
        d_func()->invalidateMapper();

    return base_type::event(event);
}

QPointF AbstractLinearGraphicsSlider::positionFromValue(qint64 logicalValue, bool bound) const
{
    Q_D(const AbstractLinearGraphicsSlider);

    d->ensureMapper();

    qreal result = d->pixelPosMin
        + GraphicsStyle::sliderPositionFromValue(d->minimum,
            d->maximum,
            logicalValue,
            d->pixelPosMax - d->pixelPosMin,
            d->upsideDown,
            bound);

    if (d->orientation == Qt::Horizontal)
        return QPointF(result, 0.0);

    return QPointF(0.0, result);
}

qint64 AbstractLinearGraphicsSlider::valueFromPosition(const QPointF& position, bool bound) const
{
    Q_D(const AbstractLinearGraphicsSlider);

    d->ensureMapper();

    qreal pixelPos = d->orientation == Qt::Horizontal ? position.x() : position.y();
    return GraphicsStyle::sliderValueFromPosition(d->minimum,
        d->maximum,
        pixelPos - d->pixelPosMin,
        d->pixelPosMax - d->pixelPosMin,
        d->upsideDown,
        bound);
}
