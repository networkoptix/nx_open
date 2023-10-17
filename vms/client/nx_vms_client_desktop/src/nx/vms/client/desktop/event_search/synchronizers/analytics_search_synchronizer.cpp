// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_search_synchronizer.h"

#include <chrono>
#include <limits>

#include <QtCore/QScopedValueRollback>
#include <QtGui/QAction>

#include <camera/cam_display.h>
#include <camera/resource_display.h>
#include <core/resource/camera_resource.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/desktop/event_search/utils/analytics_search_setup.h>
#include <nx/vms/client/desktop/event_search/utils/common_object_search_setup.h>
#include <nx/vms/client/desktop/event_search/utils/text_filter_setup.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/menu/actions.h>
#include <nx/vms/client/desktop/resource/layout_item_index.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/utils/video_cache.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <ui/graphics/instruments/instrument_manager.h>
#include <ui/graphics/instruments/rubber_band_instrument.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/workbench/extensions/workbench_stream_synchronizer.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_navigator.h>

namespace nx::vms::client::desktop {

namespace {

using namespace std::chrono;

QnMediaResourceWidget* asMediaWidget(QnResourceWidget* widget)
{
    return qobject_cast<QnMediaResourceWidget*>(widget);
}

} // namespace

AnalyticsSearchSynchronizer::AnalyticsSearchSynchronizer(
    WindowContext* context,
    CommonObjectSearchSetup* commonSetup,
    AnalyticsSearchSetup* analyticsSetup,
    QObject* parent)
    :
    AbstractSearchSynchronizer(context, parent),
    m_commonSetup(commonSetup),
    m_analyticsSetup(analyticsSetup)
{
    NX_CRITICAL(m_commonSetup && m_analyticsSetup);

    instances().push_back(this);

    setupInstanceSynchronization();

    connect(this, &AbstractSearchSynchronizer::activeChanged, this,
        [this]()
        {
            if (!active())
                m_analyticsSetup->setAreaSelectionActive(false);
        });

    connect(m_analyticsSetup, &AnalyticsSearchSetup::areaSelectionActiveChanged, this,
        [this]()
        {
            if (m_analyticsSetup->areaSelectionActive())
                setActive(true);
        });

    m_analyticsSetup->setLiveTimestampGetter(
        [this](const QnVirtualCameraResourcePtr& camera) -> milliseconds
        {
            static constexpr auto kMaxTimestamp = std::numeric_limits<milliseconds>::max();

            auto widgets = display()->widgets(camera);
            if (widgets.empty())
                return kMaxTimestamp;

            auto streamSynchronizer = windowContext()->streamSynchronizer();
            // In sync mode analyze only the first widget.
            if (streamSynchronizer->isRunning())
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

    if (!isMasterInstance())
        return;

    connect(this, &AbstractSearchSynchronizer::mediaWidgetAboutToBeChanged, this,
        [this]() { m_activeMediaWidgetConnections.reset(); });

    connect(this, &AbstractSearchSynchronizer::mediaWidgetChanged, this,
        [this](QnMediaResourceWidget* mediaWidget)
        {
            if (!mediaWidget)
                return;

            updateAreaSelection();
            m_activeMediaWidgetConnections << connect(
                mediaWidget,
                &QnMediaResourceWidget::analyticsFilterRectChanged,
                this,
                &AnalyticsSearchSynchronizer::handleWidgetAnalyticsFilterRectChanged);

            m_activeMediaWidgetConnections << connect(
                mediaWidget,
                &QnMediaResourceWidget::areaSelectionFinished,
                this,
                [this]() { m_analyticsSetup->setAreaSelectionActive(false); });
        });

    // Stop analytics area selection if scene rubber band camera selection has started.
    connect(display()->instrumentManager()->instrument<RubberBandInstrument>(),
        &RubberBandInstrument::rubberBandStarted,
        this,
        [this]() { m_analyticsSetup->setAreaSelectionActive(false); });

    connect(this, &AbstractSearchSynchronizer::activeChanged, this,
        [this]()
        {
            const QScopedValueRollback updateGuard(m_updating, true);
            updateAreaSelection();
            updateCachedDevices();
            updateWorkbench();
            updateAllMediaResourceWidgetsAnalyticsMode();
            updateAction();
        });

    connect(m_analyticsSetup, &AnalyticsSearchSetup::areaEnabledChanged,
        this, &AnalyticsSearchSynchronizer::updateAreaSelection);

    connect(m_analyticsSetup, &AnalyticsSearchSetup::areaSelectionActiveChanged, this,
        [this]() { setAreaSelectionActive(m_analyticsSetup->areaSelectionActive()); });

    connect(m_commonSetup, &CommonObjectSearchSetup::selectedCamerasChanged, this,
        [this]()
        {
            updateCachedDevices();
            updateWorkbench();
        });

    connect(m_analyticsSetup, &AnalyticsSearchSetup::combinedTextFilterChanged,
        this, &AnalyticsSearchSynchronizer::updateWorkbench);

    connect(m_analyticsSetup, &AnalyticsSearchSetup::relevantObjectTypesChanged,
        this, &AnalyticsSearchSynchronizer::updateWorkbench);

    connect(m_analyticsSetup, &AnalyticsSearchSetup::engineChanged,
        this, &AnalyticsSearchSynchronizer::updateWorkbench);

    connect(m_analyticsSetup, &AnalyticsSearchSetup::areaChanged, this,
        [this]()
        {
            updateWorkbench();

            const auto mediaWidget = this->mediaWidget();
            const auto areaSelected = m_analyticsSetup->isAreaSelected();

            if (mediaWidget && !areaSelected && m_analyticsSetup->areaEnabled())
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

    connect(action(menu::ObjectSearchModeAction), &QAction::toggled, this,
        [this](bool on)
        {
            if (on)
            {
                setActive(true);
                m_commonSetup->setCameraSelection(RightPanel::CameraSelection::current);

                if (!m_updating)
                    m_analyticsSetup->setAreaSelectionActive(true);
            }
            else
            {
                if (!m_updating)
                    setActive(false);
            }
        });

    connect(m_commonSetup, &CommonObjectSearchSetup::cameraSelectionChanged, this,
        [this]()
        {
            const QScopedValueRollback updateGuard(m_updating, true);
            updateAction();
        });
}

AnalyticsSearchSynchronizer::~AnalyticsSearchSynchronizer()
{
    const bool removed = instances().removeOne(this);
    NX_ASSERT(removed);
}

bool AnalyticsSearchSynchronizer::calculateMediaResourceWidgetAnalyticsEnabled(
    QnMediaResourceWidget* widget) const
{
    if (!widget->isAnalyticsSupported())
        return false;

    return active();
}

void AnalyticsSearchSynchronizer::updateAreaSelection()
{
    const auto mediaWidget = this->mediaWidget();
    if (!mediaWidget || !m_analyticsSetup || !active())
        return;

    const auto area = mediaWidget->analyticsFilterRect();

    if (area.isValid())
        m_commonSetup->setCameraSelection(RightPanel::CameraSelection::current);

    m_analyticsSetup->setArea(m_analyticsSetup->areaEnabled() ? area : QRectF());
}

void AnalyticsSearchSynchronizer::updateCachedDevices()
{
    std::map<SystemContext*, QnUuidSet> cachedDevicesByContext;
    if (active() && m_commonSetup)
    {
        for (const auto& camera: m_commonSetup->selectedCameras())
        {
            auto systemContext = SystemContext::fromResource(camera);
            cachedDevicesByContext[systemContext].insert(camera->getId());
        }
    }
    for (const auto& [systemContext, cachedDevices]: cachedDevicesByContext)
        systemContext->videoCache()->setCachedDevices(intptr_t(this), cachedDevices);
}

void AnalyticsSearchSynchronizer::updateWorkbench()
{
    if (!active() || !m_commonSetup)
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

    const auto cameraSetType = m_commonSetup->cameraSelection();

    if (cameraSetType != RightPanel::CameraSelection::all
        && cameraSetType != RightPanel::CameraSelection::layout)
    {
        for (const auto& camera: m_commonSetup->selectedCameras())
            m_filter.deviceIds.insert(camera->getId());
    }

    const auto filterRect = m_analyticsSetup->area();
    if (!filterRect.isNull())
        m_filter.boundingBox = filterRect;

    m_filter.analyticsEngineId = m_analyticsSetup->engine();
    m_filter.objectTypeId = m_analyticsSetup->relevantObjectTypes();
    m_filter.freeText = m_analyticsSetup->combinedTextFilter();

    for (const auto widget: display()->widgets())
    {
        if (const auto mediaWidget = asMediaWidget(widget))
            mediaWidget->setAnalyticsFilter(m_filter);
    }

    const auto camera = navigator()->currentResource().dynamicCast<QnVirtualCameraResource>();
    const bool relevant = camera && m_commonSetup->selectedCameras().contains(camera);
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

void AnalyticsSearchSynchronizer::updateAction()
{
    action(menu::ObjectSearchModeAction)->setChecked(active()
        && m_commonSetup->cameraSelection() == RightPanel::CameraSelection::current);
}

void AnalyticsSearchSynchronizer::updateMediaResourceWidgetAnalyticsMode(
    QnMediaResourceWidget* widget)
{
    widget->setAnalyticsObjectsVisibleForcefully(
        calculateMediaResourceWidgetAnalyticsEnabled(widget));

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
    if (m_analyticsSetup)
        m_analyticsSetup->setAreaSelectionActive(false);
}

void AnalyticsSearchSynchronizer::setAreaSelectionActive(bool value)
{
    if (m_areaSelectionActive == value)
        return;

    m_areaSelectionActive = value;
    updateAllMediaResourceWidgetsAnalyticsMode();
}

void AnalyticsSearchSynchronizer::ensureVisible(milliseconds timestamp, const QnUuid& trackId,
    const QnTimePeriod& proposedTimeWindow)
{
    for (auto instance: instances())
    {
        if (auto setup = instance->m_analyticsSetup)
            emit setup->ensureVisibleRequested(timestamp, trackId, proposedTimeWindow);
    }
}

void AnalyticsSearchSynchronizer::setupInstanceSynchronization()
{
    if (!NX_ASSERT(m_commonSetup && m_analyticsSetup))
        return;

    connect(this, &AnalyticsSearchSynchronizer::activeChanged, this,
        [this]()
        {
            for (auto instance: instancesToNotify())
                instance->setActive(active());
        });

    connect(m_commonSetup->textFilter(), &TextFilterSetup::textChanged, this,
        [this]()
        {
            for (auto instance: instancesToNotify())
                instance->m_commonSetup->textFilter()->setText(m_commonSetup->textFilter()->text());
        });

    connect(m_commonSetup, &CommonObjectSearchSetup::timeSelectionChanged, this,
        [this]()
        {
            for (auto instance: instancesToNotify())
                instance->m_commonSetup->setTimeSelection(m_commonSetup->timeSelection());
        });

    connect(m_commonSetup, &CommonObjectSearchSetup::cameraSelectionChanged, this,
        [this]()
        {
            for (auto instance: instancesToNotify())
                instance->m_commonSetup->setCameraSelection(m_commonSetup->cameraSelection());
        });

    connect(m_commonSetup, &CommonObjectSearchSetup::selectedCamerasChanged, this,
        [this]()
        {
            if (m_commonSetup->cameraSelection() != RightPanel::CameraSelection::custom)
                return;

            for (auto instance: instancesToNotify())
                instance->m_commonSetup->setCustomCameras(m_commonSetup->selectedCameras());
        });

    connect(m_analyticsSetup, &AnalyticsSearchSetup::engineChanged, this,
        [this]()
        {
            for (auto instance: instancesToNotify())
                instance->m_analyticsSetup->setEngine(m_analyticsSetup->engine());
        });

    connect(m_analyticsSetup, &AnalyticsSearchSetup::objectTypesChanged, this,
        [this]()
        {
            for (auto instance : instancesToNotify())
                instance->m_analyticsSetup->setObjectTypes(m_analyticsSetup->objectTypes());
        });

    connect(m_analyticsSetup, &AnalyticsSearchSetup::attributeFiltersChanged, this,
        [this]()
        {
            for (auto instance: instancesToNotify())
            {
                instance->m_analyticsSetup->setAttributeFilters(
                    m_analyticsSetup->attributeFilters());
            }
        });

    connect(m_analyticsSetup, &AnalyticsSearchSetup::areaChanged, this,
        [this]()
        {
            for (auto instance: instancesToNotify())
                instance->m_analyticsSetup->setArea(m_analyticsSetup->area());
        });

    connect(m_analyticsSetup, &AnalyticsSearchSetup::areaSelectionActiveChanged, this,
        [this]()
        {
            for (auto instance: instancesToNotify())
            {
                instance->m_analyticsSetup->setAreaSelectionActive(
                    m_analyticsSetup->areaSelectionActive());
            }
        });

    if (isMasterInstance() || !NX_ASSERT(!instances().empty()))
        return;

    const auto master = instances()[0];
    setActive(master->active());

    m_commonSetup->textFilter()->setText(master->m_commonSetup->textFilter()->text());
    m_commonSetup->setTimeSelection(master->m_commonSetup->timeSelection());
    m_commonSetup->setCameraSelection(master->m_commonSetup->cameraSelection());

    if (m_commonSetup->cameraSelection() == RightPanel::CameraSelection::custom)
        m_commonSetup->setCustomCameras(master->m_commonSetup->selectedCameras());

    m_analyticsSetup->setEngine(master->m_analyticsSetup->engine());
    m_analyticsSetup->setObjectTypes(master->m_analyticsSetup->objectTypes());
    m_analyticsSetup->setAttributeFilters(master->m_analyticsSetup->attributeFilters());
    m_analyticsSetup->setArea(master->m_analyticsSetup->area());
    m_analyticsSetup->setAreaSelectionActive(master->m_analyticsSetup->areaSelectionActive());
}

bool AnalyticsSearchSynchronizer::isMasterInstance() const
{
    return NX_ASSERT(!instances().empty()) && instances().front() == this;
}

QVector<AnalyticsSearchSynchronizer*> AnalyticsSearchSynchronizer::instancesToNotify()
{
    if (!NX_ASSERT(!instances().empty()))
        return {};

    return isMasterInstance()
        ? instances().mid(1)
        : instances().mid(0, 1);
}

QVector<AnalyticsSearchSynchronizer*>& AnalyticsSearchSynchronizer::instances()
{
    static QVector<AnalyticsSearchSynchronizer*> instances;
    return instances;
}

} // namespace nx::vms::client::desktop
