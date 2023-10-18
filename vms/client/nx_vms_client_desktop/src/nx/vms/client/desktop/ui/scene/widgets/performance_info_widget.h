// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QTimer>

#include <qt_graphics_items/graphics_widget.h>

#include <nx/vms/client/desktop/debug_utils/components/performance_info.h>
#include <nx/vms/client/desktop/window_context_aware.h>
#include <ui/graphics/items/generic/proxy_label.h>

namespace nx::vms::client::desktop {

/** Displays performance information and provides tooltips for the items under the widget. */
class PerformanceInfoWidget: public QnProxyLabel, public WindowContextAware
{
    Q_OBJECT

public:
    PerformanceInfoWidget(
        QGraphicsItem* parent, Qt::WindowFlags windowFlags, WindowContext* context);

protected:
    bool sceneEvent(QEvent* event) override;

private:
    PerformanceInfo m_performanceInfo;
    QPointer<const GraphicsWidget> m_tooltipWidget;
    QTimer m_timer;

    // Returns the first item below this that has tool tip and accepts hover event.
    const GraphicsWidget* getTooltipItem(const QPointF& pos) const;
};

} // namespace nx::vms::client::desktop
