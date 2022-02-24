// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "round_panel_label.h"

#include <QtGui/QPainter>

namespace nx::vms::client::desktop {

RoundPanelLabel::RoundPanelLabel(QWidget* parent): base_type(parent)
{
    setAlignment(Qt::AlignCenter);
    setAutomaticContentsMargins();
}

QColor RoundPanelLabel::backgroundColor() const
{
    return m_backgroundColor;
}

QColor RoundPanelLabel::effectiveBackgroundColor() const
{
    return m_backgroundColor.isValid()
        ? m_backgroundColor
        : palette().color(QPalette::Window);
}

void RoundPanelLabel::setBackgroundColor(const QColor& value)
{
    if (m_backgroundColor == value)
        return;

    m_backgroundColor = value;
    update();
}

void RoundPanelLabel::unsetBackgroundColor()
{
    setBackgroundColor(QColor());
}

qreal RoundPanelLabel::roundingRaduis() const
{
    return m_roundingRadius;
}

void RoundPanelLabel::setRoundingRadius(qreal value, bool autoContentsMargins)
{
    if (qFuzzyIsNull(m_roundingRadius - value))
        return;

    m_roundingRadius = value;
    if (autoContentsMargins)
        setAutomaticContentsMargins();

    update();
}

void RoundPanelLabel::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    const auto radius = qMin(width(), height()) * 0.5;
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(effectiveBackgroundColor());
    painter.setPen(palette().windowText().color());
    painter.drawRoundedRect(rect(), radius, radius);
    painter.end(); //< For QGraphicsProxyWidget compatibility.
    base_type::paintEvent(event);
}

void RoundPanelLabel::setAutomaticContentsMargins()
{
    const auto margin = m_roundingRadius / 2;
    setContentsMargins(margin, 0, margin, 0);
}

} // namespace nx::vms::client::desktop
