// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "motion_search_duration_metric.h"

#include <nx/vms/client/desktop/workbench/workbench.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_item.h>

using namespace nx::vms::client::desktop;

MotionSearchDurationMetric::MotionSearchDurationMetric(
    QnWorkbenchContext* context,
    QObject* parent)
    :
    base_type(parent),
    QnWorkbenchContextAware(context)
{
    connect(display(), &QnWorkbenchDisplay::widgetAdded,
        this, &MotionSearchDurationMetric::addWidget);
    connect(display(), &QnWorkbenchDisplay::widgetAboutToBeRemoved,
        this, &MotionSearchDurationMetric::removeWidget);

    connect(workbench(), &Workbench::currentLayoutChanged,
        this, &MotionSearchDurationMetric::updateCounterState);
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
    connect(mediaWidget, &QnResourceWidget::optionsChanged,
        this, &MotionSearchDurationMetric::updateCounterState);

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
    const bool activate = std::any_of(m_widgets.begin(), m_widgets.end(),
        [this](const QnMediaResourceWidgetPtr &mediaWidget)
        {
            return (mediaWidget && isFromCurrentLayout(mediaWidget)
                && (mediaWidget->options().testFlag(QnResourceWidget::DisplayMotion)));
        });

    setCounterActive(activate);
}

bool MotionSearchDurationMetric::isFromCurrentLayout(const QnMediaResourceWidgetPtr &widget)
{
    const auto item = widget->item();
    if (!item)
        return false;

    const auto widgetLayout = item->layout();
    const auto currentLayout = workbench()->currentLayout();
    return (currentLayout == widgetLayout);
}
