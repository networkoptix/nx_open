#include "selectable_button.h"

#include <ui/style/helper.h>
#include <utils/common/scoped_painter_rollback.h>

#include <nx/client/core/utils/geometry.h>

using nx::vms::client::core::Geometry;

namespace nx::vms::client::desktop {

SelectableButton::SelectableButton(QWidget* parent): base_type(parent)
{
    setFocusPolicy(Qt::TabFocus);
    setCheckable(true);
    setAutoExclusive(true);
    updateContentsMargins();
}

int SelectableButton::markerFrameWidth() const
{
    return m_markerFrameWidth;
}

void SelectableButton::setMarkerFrameWidth(int width)
{
    if (width == m_markerFrameWidth)
        return;

    m_markerFrameWidth = width;
    updateContentsMargins();
    update();
}

int SelectableButton::markerMargin() const
{
    return m_markerMargin;
}

void SelectableButton::setMarkerMargin(int margin)
{
    if (margin == m_markerMargin)
        return;

    m_markerMargin = margin;
    updateContentsMargins();
    update();
}

qreal SelectableButton::roundingRadius() const
{
    return m_roundingRadius;
}

void SelectableButton::setRoundingRadius(int width)
{
    if (width == m_markerFrameWidth)
        return;

    m_markerFrameWidth = width;
    update();
}

void SelectableButton::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QStyleOptionToolButton option;
    initStyleOption(&option);

    if (!option.state.testFlag(QStyle::State_Enabled))
    {
        painter.setOpacity(style::Hints::kDisabledItemOpacity);
        option.state &= ~(QStyle::State_Sunken | QStyle::State_MouseOver);
    }

    if (option.state.testFlag(QStyle::State_On))
    {
        paintMarker(&painter, palette().highlight());
        option.state &= ~QStyle::State_MouseOver;
    }
    else if (option.state.testFlag(QStyle::State_Sunken))
    {
        paintMarker(&painter, palette().dark());
        option.state &= ~QStyle::State_MouseOver;
    }
    else if (option.state.testFlag(QStyle::State_MouseOver))
    {
        paintMarker(&painter, palette().midlight());
    }

    option.rect = contentsRect();
    customPaint(&painter, &option, this);

    if (option.state.testFlag(QStyle::State_HasFocus))
    {
        option.rect = Geometry::dilated(option.rect, 1);
        style()->drawPrimitive(QStyle::PE_FrameFocusRect, &option, &painter, this);
    }
}

void SelectableButton::paintMarker(QPainter* painter, const QBrush& brush)
{
    QRectF frameRect(rect());

    QnScopedPainterPenRollback penRollback(painter, QPen(brush, m_markerFrameWidth));
    QnScopedPainterBrushRollback brushRollback(painter, Qt::NoBrush);

    painter->drawRoundedRect(
        Geometry::eroded(frameRect, m_markerFrameWidth / 2.0),
        m_roundingRadius, m_roundingRadius);
}

void SelectableButton::updateContentsMargins()
{
    int margin = m_markerFrameWidth + m_markerMargin;
    setContentsMargins(margin, margin, margin, margin);
}

} // namespace nx::vms::client::desktop
