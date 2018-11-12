
#include "camera_fullscreen_metric.h"

#include <ui/workbench/workbench_display.h>
#include <ui/graphics/items/resource/media_resource_widget.h>

namespace
{
    bool isMediaResourceWidget(const QnResourceWidget *resourceWidget)
    {
        return (dynamic_cast<const QnMediaResourceWidget *>(resourceWidget) != nullptr);
    }
}

CameraFullscreenMetric::CameraFullscreenMetric(QnWorkbenchDisplay *display)
    : base_type()
    , QnTimeDurationMetric()
    , m_currentFullscreenWidget()
{
    if (!display)
        return;

    const QPointer<CameraFullscreenMetric> guard(this);
    const auto widgetOptionsChangedHandler = [this, guard](const WidgetPointer &widget)
    {
        if (!guard)
            return;

        const bool isChangedWidgetFullscreen =
            (widget->options().testFlag(QnResourceWidget::FullScreenMode));
        const bool isCurrentFullscreenWidget =
            (m_currentFullscreenWidget == widget);

        if (isChangedWidgetFullscreen)
            m_currentFullscreenWidget = widget;
        else if (isCurrentFullscreenWidget)
            m_currentFullscreenWidget = nullptr;

        setCounterActive(m_currentFullscreenWidget != nullptr);
    };

    const auto addWidgetHandler =
        [this, guard, widgetOptionsChangedHandler](QnResourceWidget *widget)
    {
        if (!guard)
            return;

        const auto updateCounterImpl = [this, guard, widget, widgetOptionsChangedHandler]()
        {
            if (guard)
                widgetOptionsChangedHandler(widget);
        };

        connect(widget, &QnResourceWidget::optionsChanged
            , this, updateCounterImpl);
    };

    const auto removeWidgetHandler =
        [this, guard](QnResourceWidget *widget)
    {
        if (!guard || (widget != m_currentFullscreenWidget))
            return;

        m_currentFullscreenWidget = nullptr;
        setCounterActive(false);
    };

    connect(display, &QnWorkbenchDisplay::widgetAdded
        , this, addWidgetHandler);
    connect(display, &QnWorkbenchDisplay::widgetAboutToBeRemoved
        , this, removeWidgetHandler);
}

CameraFullscreenMetric::~CameraFullscreenMetric()
{}
