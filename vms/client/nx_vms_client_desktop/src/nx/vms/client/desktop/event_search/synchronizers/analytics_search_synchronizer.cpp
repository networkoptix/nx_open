#include "analytics_search_synchronizer.h"

#include <core/resource/camera_resource.h>
#include <core/resource/layout_item_index.h>

#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_navigator.h>

#include <nx/utils/log/assert.h>
#include <nx/vms/client/desktop/event_search/widgets/analytics_search_widget.h>
#include <nx/vms/client/desktop/analytics/analytics_objects_visualization_manager.h>

namespace nx::vms::client::desktop {

namespace {

QnMediaResourceWidget* asMediaWidget(QnResourceWidget* widget)
{
    return qobject_cast<QnMediaResourceWidget*>(widget);
}

} // namespace

AnalyticsSearchSynchronizer::AnalyticsSearchSynchronizer(
    QnWorkbenchContext* context,
    AnalyticsSearchWidget* analyticsSearchWidget,
    QObject* parent)
    :
    AbstractSearchSynchronizer(context, parent),
    m_analyticsSearchWidget(analyticsSearchWidget),
    m_objectsVisualizationManager(context->instance<AnalyticsObjectsVisualizationManager>())
{
    NX_CRITICAL(m_analyticsSearchWidget);
    NX_CRITICAL(m_objectsVisualizationManager);

    connect(this, &AbstractSearchSynchronizer::mediaWidgetAboutToBeChanged, this,
        [this](QnMediaResourceWidget* mediaWidget)
        {
            if (!mediaWidget)
                return;

            QObject::disconnect(m_activeMediaWidgetConnection);
            mediaWidget->unsetAreaSelectionType(QnMediaResourceWidget::AreaType::analytics);
        });

    connect(this, &AbstractSearchSynchronizer::mediaWidgetChanged, this,
        [this](QnMediaResourceWidget* mediaWidget)
        {
            if (!mediaWidget)
                return;

            updateAreaSelection();
            m_activeMediaWidgetConnection = connect(
                mediaWidget,
                &QnMediaResourceWidget::analyticsFilterRectChanged,
                this,
                &AnalyticsSearchSynchronizer::handleWidgetAnalyticsFilterRectChanged);
        });

    connect(this, &AbstractSearchSynchronizer::activeChanged, this,
        [this]()
        {
            updateAreaSelection();
            updateTimelineDisplay();
            updateAllMediaResourceWidgetsAnalyticsMode();
        });

    connect(m_analyticsSearchWidget, &AnalyticsSearchWidget::areaSelectionEnabledChanged,
        this, &AnalyticsSearchSynchronizer::updateAreaSelection);

    connect(m_analyticsSearchWidget, &AnalyticsSearchWidget::areaSelectionRequested, this,
        [this]()
        {
            const auto mediaWidget = this->mediaWidget();
            if (mediaWidget && active())
            {
                mediaWidget->setAreaSelectionEnabled(
                    QnMediaResourceWidget::AreaType::analytics, true);
            }
        });

    connect(m_analyticsSearchWidget, &AnalyticsSearchWidget::cameraSetChanged,
        this, &AnalyticsSearchSynchronizer::updateTimelineDisplay);

    connect(m_analyticsSearchWidget, &AnalyticsSearchWidget::textFilterChanged,
        this, &AnalyticsSearchSynchronizer::updateTimelineDisplay);

    connect(m_analyticsSearchWidget, &AnalyticsSearchWidget::filterRectChanged, this,
        [this](const QRectF& value)
        {
            updateTimelineDisplay();

            const auto mediaWidget = this->mediaWidget();
            if (value.isNull() && mediaWidget && m_analyticsSearchWidget->areaSelectionEnabled())
                mediaWidget->setAnalyticsFilterRect({});
        });

    connect(navigator(), &QnWorkbenchNavigator::currentResourceChanged,
        this, &AnalyticsSearchSynchronizer::updateTimelineDisplay);

    connect(display(), &QnWorkbenchDisplay::widgetAdded, this,
        [this](QnResourceWidget* widget)
        {
            if (const auto mediaWidget = asMediaWidget(widget))
            {
                const auto updateWidgetMode =
                    [this, mediaWidget] { updateMediaResourceWidgetAnalyticsMode(mediaWidget); };

                connect(mediaWidget, &QnMediaResourceWidget::displayChanged, this,
                    updateWidgetMode);
                connect(mediaWidget, &QnMediaResourceWidget::analyticsSupportChanged, this,
                    updateWidgetMode);
                updateMediaResourceWidgetAnalyticsMode(mediaWidget);
            }
        });

    connect(display(), &QnWorkbenchDisplay::widgetAboutToBeRemoved, this,
        [this](QnResourceWidget* widget)
        {
            widget->disconnect(this);
        });

    connect(m_objectsVisualizationManager, &AnalyticsObjectsVisualizationManager::modeChanged,
        this,
        [this](const QnLayoutItemIndex& index, AnalyticsObjectsVisualizationMode value)
        {
            if (index.layout() != workbench()->currentLayout()->resource())
                return;

            const auto item = workbench()->currentLayout()->item(index.uuid());
            if (!item)
                return;

            if (const auto widget = asMediaWidget(display()->widget(item)))
                updateMediaResourceWidgetAnalyticsMode(widget);
        });
}

bool AnalyticsSearchSynchronizer::calculateMediaResourceWidgetAnalyticsEnabled(
    QnMediaResourceWidget* widget) const
{
    if (!widget->isAnalyticsSupported())
        return false;

    if (this->active())
        return true;

    const auto currentLayout = workbench()->currentLayout()->resource();
    if (!currentLayout)
        return false;

    const auto item = widget->item();
    NX_ASSERT(item);
    if (!item)
        return false;

    const QnLayoutItemIndex index(currentLayout, item->uuid());
    return m_objectsVisualizationManager->mode(index) == AnalyticsObjectsVisualizationMode::always;
}

void AnalyticsSearchSynchronizer::updateAreaSelection()
{
    const auto mediaWidget = this->mediaWidget();
    if (!mediaWidget || !m_analyticsSearchWidget)
        return;

    if (!active())
    {
        mediaWidget->unsetAreaSelectionType(QnMediaResourceWidget::AreaType::analytics);
        return;
    }

    m_analyticsSearchWidget->setFilterRect(m_analyticsSearchWidget->areaSelectionEnabled()
        ? mediaWidget->analyticsFilterRect()
        : QRectF());

    mediaWidget->setAreaSelectionType(m_analyticsSearchWidget->areaSelectionEnabled()
        ? QnMediaResourceWidget::AreaType::analytics
        : QnMediaResourceWidget::AreaType::none);
}

void AnalyticsSearchSynchronizer::updateTimelineDisplay()
{
    if (!active())
    {
        setTimeContentDisplayed(Qn::AnalyticsContent, false);
        return;
    }

    const auto camera = navigator()->currentResource().dynamicCast<QnVirtualCameraResource>();
    const bool relevant = camera && m_analyticsSearchWidget->cameras().contains(camera);
    if (!relevant)
    {
        navigator()->setSelectedExtraContent(Qn::RecordingContent /*means none*/);
        return;
    }

    analytics::storage::Filter filter;
    filter.deviceIds = {camera->getId()};
    filter.boundingBox = m_analyticsSearchWidget->filterRect();
    filter.freeText = m_analyticsSearchWidget->textFilter();
    navigator()->setAnalyticsFilter(filter);
    navigator()->setSelectedExtraContent(Qn::AnalyticsContent);
}

void AnalyticsSearchSynchronizer::updateMediaResourceWidgetAnalyticsMode(
    QnMediaResourceWidget* widget)
{
    widget->setAnalyticsEnabled(calculateMediaResourceWidgetAnalyticsEnabled(widget));
}

void AnalyticsSearchSynchronizer::updateAllMediaResourceWidgetsAnalyticsMode()
{
    for (const auto widget: display()->widgets())
    {
        if (const auto mediaWidget = asMediaWidget(widget))
            updateMediaResourceWidgetAnalyticsMode(mediaWidget);
    }
}

void AnalyticsSearchSynchronizer::handleWidgetAnalyticsFilterRectChanged()
{
    updateAreaSelection();

    // Stop selection after selecting once.
    mediaWidget()->setAreaSelectionEnabled(QnMediaResourceWidget::AreaType::analytics, false);
}

} // namespace nx::vms::client::desktop
