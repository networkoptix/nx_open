#include "analytics_search_synchronizer.h"

#include <chrono>
#include <limits>

#include <camera/cam_display.h>
#include <camera/resource_display.h>
#include <client/client_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_item_index.h>

#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/workbench/extensions/workbench_stream_synchronizer.h>
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

using namespace std::chrono;

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
            updateWorkbench();
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
            updateWorkbench();

            if (m_analyticsSearchWidget->selectedCameras() != AbstractSearchWidget::Cameras::current)
                setAreaSelectionActive(false);
        });

    connect(m_analyticsSearchWidget, &AnalyticsSearchWidget::textFilterChanged,
        this, &AnalyticsSearchSynchronizer::updateWorkbench);

    connect(m_analyticsSearchWidget, &AnalyticsSearchWidget::selectedObjectTypeChanged,
        this, &AnalyticsSearchSynchronizer::updateWorkbench);

    connect(m_analyticsSearchWidget, &AnalyticsSearchWidget::filterRectChanged, this,
        [this](const QRectF& value)
        {
            updateWorkbench();

            const auto mediaWidget = this->mediaWidget();
            if (value.isNull() && mediaWidget && m_analyticsSearchWidget->areaSelectionEnabled())
                mediaWidget->setAnalyticsFilterRect({});
        });

    connect(navigator(), &QnWorkbenchNavigator::currentResourceChanged,
        this, &AnalyticsSearchSynchronizer::updateWorkbench);

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

                mediaWidget->setAnalyticsFilter(active() ? m_filter : nx::analytics::db::Filter());
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
            const auto currentLayout = workbench()->currentLayout();
            if (index.layout() != currentLayout->resource())
                return;

            const auto item = currentLayout->item(index.uuid());
            if (!item)
                return;

            if (const auto widget = asMediaWidget(display()->widget(item)))
                updateMediaResourceWidgetAnalyticsMode(widget);

            // Cleanup analytics areas on all target widget zoom windows.
            for (const auto zoomItem: currentLayout->zoomItems(item))
            {
                if (const auto widget = asMediaWidget(display()->widget(zoomItem)))
                {
                    NX_ASSERT(widget->isZoomWindow());
                    updateMediaResourceWidgetAnalyticsMode(widget);
                }
            }
        });

    m_analyticsSearchWidget->setLiveTimestampGetter(
        [this](const QnVirtualCameraResourcePtr& camera) -> milliseconds
        {
            static constexpr auto kMaxTimestamp = std::numeric_limits<milliseconds>::max();

            auto widgets = display()->widgets(camera);
            if (widgets.empty())
                return kMaxTimestamp;

            // In sync mode analyze only the first widget.
            if (this->context()->instance<QnWorkbenchStreamSynchronizer>()->isRunning())
                widgets = decltype(widgets)({*widgets.cbegin()});

            for (const auto widget: widgets)
            {
                const auto mediaWidget = qobject_cast<QnMediaResourceWidget*>(widget);
                if (!NX_ASSERT(mediaWidget))
                    continue;

                const auto camDisplay = mediaWidget->display()->camDisplay();
                if (camDisplay && camDisplay->isRealTimeSource())
                {
                    const auto timestampUs = camDisplay->getDisplayedTime();
                    return timestampUs < 0 || timestampUs == DATETIME_NOW
                        ? kMaxTimestamp
                        : duration_cast<milliseconds>(microseconds(timestampUs));
                }
            }

            return kMaxTimestamp;
        });
    m_filter.withBestShotOnly = true;
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

void AnalyticsSearchSynchronizer::updateWorkbench()
{
    if (!active())
    {
        for (const auto widget: display()->widgets())
        {
            if (const auto mediaWidget = asMediaWidget(widget))
                mediaWidget->setAnalyticsFilter({});
        }

        setTimeContentDisplayed(Qn::AnalyticsContent, false);
        return;
    }

    m_filter = {};
    const auto cameraSetType = m_analyticsSearchWidget->selectedCameras();
    if (cameraSetType != AbstractSearchWidget::Cameras::all
        && cameraSetType != AbstractSearchWidget::Cameras::layout)
    {
        for (const auto& camera: m_analyticsSearchWidget->cameras())
            m_filter.deviceIds.push_back(camera->getId());
    }

    const auto filterRect = m_analyticsSearchWidget->filterRect();
    if (!filterRect.isNull())
        m_filter.boundingBox = filterRect;

    const auto selectedObjectType = m_analyticsSearchWidget->selectedObjectType();
    if (!selectedObjectType.isEmpty())
        m_filter.objectTypeId.push_back(selectedObjectType);

    m_filter.freeText = m_analyticsSearchWidget->textFilter();

    for (const auto widget: display()->widgets())
    {
        if (const auto mediaWidget = asMediaWidget(widget))
            mediaWidget->setAnalyticsFilter(m_filter);
    }

    const auto camera = navigator()->currentResource().dynamicCast<QnVirtualCameraResource>();
    const bool relevant = camera && m_analyticsSearchWidget->cameras().contains(camera);
    if (!relevant)
    {
        navigator()->setSelectedExtraContent(Qn::RecordingContent /*means none*/);
        return;
    }

    auto filter = m_filter;
    filter.deviceIds = {camera->getId()};
    NX_VERBOSE(this, "Load analytics chunks by filter %1", filter);
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
