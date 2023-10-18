// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "performance_info_widget.h"

#include <QtWidgets/QGraphicsSceneHelpEvent>
#include <QtWidgets/QToolTip>

#include <ui/graphics/instruments/signaling_instrument.h>
#include <ui/workbench/workbench_display.h>

namespace nx::vms::client::desktop {

PerformanceInfoWidget::PerformanceInfoWidget(
    QGraphicsItem* parent,
    Qt::WindowFlags windowFlags,
    WindowContext* context)
    :
    QnProxyLabel{parent, windowFlags},
    WindowContextAware{context}
{
    setAcceptedMouseButtons(Qt::NoButton);
    setTextFormat(Qt::RichText);
    setAcceptHoverEvents(false);

    constexpr auto kUpdateInterval = std::chrono::milliseconds{100};
    m_timer.setInterval(kUpdateInterval);

    connect(display()->beforePaintInstrument(), QnSignalingInstrumentActivated, &m_performanceInfo,
        &PerformanceInfo::registerFrame);

    connect(&m_performanceInfo, &PerformanceInfo::textChanged, this,
        [this](const QString& richText)
        {
            setText(richText);
            resize(effectiveSizeHint(Qt::PreferredSize));
        });

    connect(&m_timer, &QTimer::timeout, this,
        [this]
        {
            if (m_tooltipWidget && m_tooltipWidget->isUnderMouse())
                return;

            m_tooltipWidget.clear();
            if (isUnderMouse() && QToolTip::isVisible())
                QToolTip::hideText();

            m_timer.stop();
        });
}

bool PerformanceInfoWidget::sceneEvent(QEvent* event)
{
    if (event->type() == QEvent::GraphicsSceneHelp)
    {
        // This widget must provide a tooltip from the items below (e.g. resource item title and buttons bar)
        auto helpEvent = static_cast<QGraphicsSceneHelpEvent*>(event);
        m_tooltipWidget = getTooltipItem(helpEvent->scenePos());

        if (m_tooltipWidget)
        {
            QToolTip::showText(helpEvent->screenPos(), m_tooltipWidget->toolTip());
            m_timer.start();
            return true;
        }
    }

    return QnProxyLabel::sceneEvent(event);
}

const GraphicsWidget* PerformanceInfoWidget::getTooltipItem(const QPointF& pos) const
{
    bool belowThis{false};
    for (const auto* item: scene()->items(pos))
    {
        if (item == this)
        {
            belowThis = true;
            continue;
        }

        if (!belowThis || !item->isVisible() || !item->acceptHoverEvents()
            || item->toolTip().isEmpty())
        {
            continue;
        }

        auto graphicsWidget = dynamic_cast<const GraphicsWidget*>(item);
        if (!graphicsWidget)
            continue;

        return graphicsWidget;
    }

    return {};
}

} // namespace nx::vms::client::desktop
