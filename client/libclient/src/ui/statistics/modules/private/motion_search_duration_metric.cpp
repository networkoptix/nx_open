
#include "motion_search_duration_metric.h"

#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <ui/graphics/items/resource/media_resource_widget.h>

MotionSearchDurationMetric::MotionSearchDurationMetric(QnWorkbenchContext *context)
    : base_type()
    , QnTimeDurationMetric()
    , m_context(context)
    , m_widgets()
{
    if (!m_context)
        return;

    connect(m_context->display(), &QnWorkbenchDisplay::widgetAdded
        , this, &MotionSearchDurationMetric::addWidget);
    connect(m_context->display(), &QnWorkbenchDisplay::widgetAboutToBeRemoved
        , this, &MotionSearchDurationMetric::removeWidget);

    connect(m_context->workbench(), &QnWorkbench::currentLayoutChanged
        , this, &MotionSearchDurationMetric::updateCounterState);
}

MotionSearchDurationMetric::~MotionSearchDurationMetric()
{}

void MotionSearchDurationMetric::addWidget(QnResourceWidget *resourceWidget)
{
    const QnMediaResourceWidgetPtr mediaWidget =
        dynamic_cast<QnMediaResourceWidget *>(resourceWidget);
    if (!mediaWidget)
        return;

    const QPointer<MotionSearchDurationMetric> guard(this);
    const auto widgetOptionsChangedHandler =
        [this, guard, mediaWidget]()
    {
        if (!guard || !mediaWidget)
            return;
    };

    m_widgets.insert(mediaWidget);
    connect(mediaWidget, &QnResourceWidget::optionsChanged
        , this, &MotionSearchDurationMetric::updateCounterState);

    if (isFromCurrentLayout(mediaWidget) && !isCounterActive())
        updateCounterState();
}

void MotionSearchDurationMetric::removeWidget(QnResourceWidget *resourceWidget)
{
    const QnMediaResourceWidgetPtr mediaWidget =
        dynamic_cast<QnMediaResourceWidget *>(resourceWidget);
    if (!mediaWidget)
        return;

    disconnect(mediaWidget, nullptr, this, nullptr);
    m_widgets.erase(mediaWidget);

    if (isFromCurrentLayout(mediaWidget) && isCounterActive())
        updateCounterState();
}

void MotionSearchDurationMetric::updateCounterState()
{
    const bool activate = std::any_of(m_widgets.begin(), m_widgets.end()
        , [this](const QnMediaResourceWidgetPtr &mediaWidget)
    {
        return (mediaWidget && isFromCurrentLayout(mediaWidget)
            && (mediaWidget->options().testFlag(QnResourceWidget::DisplayMotion)));
    });

    setCounterActive(activate);
}

bool MotionSearchDurationMetric::isFromCurrentLayout(const QnMediaResourceWidgetPtr &widget)
{
    if (!m_context)
        return false;

    const auto item = widget->item();
    if (!item)
        return false;

    const auto widgetLayout = item->layout();
    const auto currentLayout = m_context->workbench()->currentLayout();
    return (currentLayout == widgetLayout);
}

