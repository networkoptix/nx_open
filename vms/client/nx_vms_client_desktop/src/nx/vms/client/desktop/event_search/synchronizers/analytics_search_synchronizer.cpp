#include "analytics_search_synchronizer.h"

#include <client/client_module.h>
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
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/analytics/analytics_objects_visualization_manager.h>
#include <nx/vms/client/desktop/event_search/widgets/analytics_search_widget.h>
#include <nx/vms/client/desktop/utils/video_cache.h>

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
        [this]() { QObject::disconnect(m_activeMediaWidgetConnection); });

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
            updateCachedDevices();
            updateTimelineDisplay();
            updateAllMediaResourceWidgetsAnalyticsMode();
        });

    connect(m_analyticsSearchWidget, &AnalyticsSearchWidget::areaSelectionEnabledChanged,
        this, &AnalyticsSearchSynchronizer::updateAreaSelection);

    connect(m_analyticsSearchWidget, &AnalyticsSearchWidget::areaSelectionRequested, this,
        [this]() { setAreaSelectionActive(true); });

    connect(m_analyticsSearchWidget, &AnalyticsSearchWidget::cameraSetChanged, this,
        [this]()
        {
            updateCachedDevices();
            updateTimelineDisplay();
        });

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
    if (!mediaWidget || !m_analyticsSearchWidget || !active())
        return;

    m_analyticsSearchWidget->setFilterRect(m_analyticsSearchWidget->areaSelectionEnabled()
        ? mediaWidget->analyticsFilterRect()
        : QRectF());
}

void AnalyticsSearchSynchronizer::updateCachedDevices()
{
    if (!ini().cacheLiveVideoForRightPanelPreviews)
        return;

    if (auto cache = qnClientModule->videoCache())
    {
        QnUuidSet cachedDevices;
        if (active())
        {
            for (const auto& camera: m_analyticsSearchWidget->cameras())
                cachedDevices.insert(camera->getId());
        }

        cache->setCachedDevices(intptr_t(this), cachedDevices);
    }
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

    if (active())
    {
        widget->setAreaSelectionType(QnMediaResourceWidget::AreaType::analytics);
        widget->setAreaSelectionEnabled(m_areaSelectionActive);
    }
    else
    {
        widget->setAreaSelectionEnabled(QnMediaResourceWidget::AreaType::analytics, false);
        widget->unsetAreaSelectionType(QnMediaResourceWidget::AreaType::analytics);
    }
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
    setAreaSelectionActive(false);
}

void AnalyticsSearchSynchronizer::setAreaSelectionActive(bool value)
{
    if (m_areaSelectionActive == value)
        return;

    m_areaSelectionActive = value;
    updateAllMediaResourceWidgetsAnalyticsMode();
}

} // namespace nx::vms::client::desktop
