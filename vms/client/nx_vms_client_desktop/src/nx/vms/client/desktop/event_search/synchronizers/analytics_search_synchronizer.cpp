// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_search_synchronizer.h"

#include <chrono>
#include <limits>

#include <QtCore/QScopedValueRollback>
#include <QtGui/QAction>

#include <camera/cam_display.h>
#include <camera/resource_display.h>
#include <core/resource/camera_resource.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/core/analytics/analytics_filter_model.h>
#include <nx/vms/client/core/analytics/taxonomy/object_type.h>
#include <nx/vms/client/core/event_search/utils/analytics_search_setup.h>
#include <nx/vms/client/core/event_search/utils/text_filter_setup.h>
#include <nx/vms/client/core/resource/user.h>
#include <nx/vms/client/core/utils/video_cache.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/event_search/utils/common_object_search_setup.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/menu/actions.h>
#include <nx/vms/client/desktop/resource/layout_item_index.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/client/desktop/settings/user_specific_settings.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/system_logon/logic/connect_actions_handler.h>
#include <nx/vms/client/desktop/system_logon/logic/fresh_session_token_helper.h>
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
    core::AnalyticsSearchSetup* analyticsSetup,
    QObject* parent)
    :
    base_type(context, commonSetup, parent),
    m_analyticsSetup(analyticsSetup)
{
    NX_CRITICAL(m_analyticsSetup);

    instances().push_back(this);

    setupInstanceSynchronization();

    connect(this, &AbstractSearchSynchronizer::activeChanged, this,
        [this]()
        {
            if (!active())
                m_analyticsSetup->setAreaSelectionActive(false);
        });

    connect(m_analyticsSetup, &core::AnalyticsSearchSetup::areaSelectionActiveChanged, this,
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

    connect(m_analyticsSetup, &core::AnalyticsSearchSetup::areaEnabledChanged,
        this, &AnalyticsSearchSynchronizer::updateAreaSelection);

    connect(m_analyticsSetup, &core::AnalyticsSearchSetup::areaSelectionActiveChanged, this,
        [this]() { setAreaSelectionActive(m_analyticsSetup->areaSelectionActive()); });

    connect(commonSetup, &CommonObjectSearchSetup::selectedCamerasChanged, this,
        [this]()
        {
            updateCachedDevices();
            updateWorkbench();
        });

    connect(m_analyticsSetup, &core::AnalyticsSearchSetup::combinedTextFilterChanged,
        this, &AnalyticsSearchSynchronizer::updateWorkbench);

    connect(m_analyticsSetup, &core::AnalyticsSearchSetup::engineChanged,
        this, &AnalyticsSearchSynchronizer::updateWorkbench);

    connect(m_analyticsSetup, &core::AnalyticsSearchSetup::areaChanged, this,
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
                this->commonSetup()->setCameraSelection(
                    core::EventSearch::CameraSelection::current);

                if (!m_updating)
                    m_analyticsSetup->setAreaSelectionActive(true);
            }
            else
            {
                if (!m_updating)
                    setActive(false);
            }
        });

    connect(commonSetup, &CommonObjectSearchSetup::cameraSelectionChanged, this,
        &AnalyticsSearchSynchronizer::updateAction);

    connect(system()->userSettings(), &UserSpecificSettings::loaded, this,
        [this]()
        {
            updateFiltersFromSettings();
            setupSettingsStorage();
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
        commonSetup()->setCameraSelection(core::EventSearch::CameraSelection::current);

    m_analyticsSetup->setArea(m_analyticsSetup->areaEnabled() ? area : QRectF());
}

void AnalyticsSearchSynchronizer::updateCachedDevices()
{
    std::map<SystemContext*, UuidSet> cachedDevicesByContext;
    if (active() && commonSetup())
    {
        for (const auto& camera: commonSetup()->selectedCameras())
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
    if (!active() || !commonSetup())
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

    const auto cameraSetType = commonSetup()->cameraSelection();

    if (cameraSetType != core::EventSearch::CameraSelection::all
        && cameraSetType != core::EventSearch::CameraSelection::layout)
    {
        for (const auto& camera: commonSetup()->selectedCameras())
            m_filter.deviceIds.insert(camera->getId());
    }

    const auto filterRect = m_analyticsSetup->area();
    if (!filterRect.isNull())
        m_filter.boundingBox = filterRect;

    m_filter.analyticsEngineId = m_analyticsSetup->engine();
    m_filter.freeText = m_analyticsSetup->combinedTextFilter();

    const auto objectTypes = m_analyticsSetup->objectTypes();
    m_filter.objectTypeId = {objectTypes.cbegin(), objectTypes.cend()};

    for (const auto widget: display()->widgets())
    {
        if (const auto mediaWidget = asMediaWidget(widget))
            mediaWidget->setAnalyticsFilter(m_filter);
    }

    const auto camera = navigator()->currentResource().dynamicCast<QnVirtualCameraResource>();
    const bool relevant = camera && commonSetup()->selectedCameras().contains(camera);
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
        && commonSetup()->cameraSelection() == core::EventSearch::CameraSelection::current);
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

void AnalyticsSearchSynchronizer::updateObjectTypesFromSettings()
{
    const QStringList& objectTypeIds = system()->userSettings()->objectSearchObjectTypeIds();

    if (m_updating || m_analyticsSetup->objectTypes() == objectTypeIds)
        return;

    const std::unique_ptr<core::analytics::taxonomy::AnalyticsFilterModel> filterModel{
        system()->taxonomyManager()->createFilterModel()};

    filterModel->setSelectedEngine(filterModel->findEngine(m_analyticsSetup->engine()));
    filterModel->setSelectedDevices(commonSetup()->selectedCameras());

    if (filterModel->findFilterObjectType(objectTypeIds))
        m_analyticsSetup->setObjectTypes(objectTypeIds);
    else
        m_analyticsSetup->setObjectTypes({});
}

void AnalyticsSearchSynchronizer::updateTimeSelectionFromSettings()
{
    commonSetup()->setTimeSelection(system()->userSettings()->objectSearchTimeSelection());
}

void AnalyticsSearchSynchronizer::updateAreaFromSettings()
{
    const QRectF area = commonSetup()->selectedCameras().size() != 1
        ? QRectF()
        : system()->userSettings()->objectSearchArea();

    m_analyticsSetup->setArea(area);

    if (area.isValid())
    {
        m_analyticsSetup->setAreaSelectionActive(true);
        if (QnMediaResourceWidget* mediaWidget = this->mediaWidget())
            mediaWidget->setAnalyticsFilterRect(area);
    }
}

void AnalyticsSearchSynchronizer::updateAttributeFiltersFromSettings()
{
    m_analyticsSetup->setAttributeFilters(
        system()->userSettings()->objectSearchAttributeFilters());
}

void AnalyticsSearchSynchronizer::updateCameraSelectionFromSettings()
{
    const core::EventSearch::CameraSelection cameraSelection =
        system()->userSettings()->objectSearchCameraSelection();

    if (m_updating
        || (commonSetup()->cameraSelection() == cameraSelection
            && (commonSetup()->selectedCamerasIds()
                == system()->userSettings()->objectSearchCustomCameras())))
    {
        return;
    }

    switch (cameraSelection)
    {
        case core::EventSearch::CameraSelection::all:
        case core::EventSearch::CameraSelection::layout:
        case core::EventSearch::CameraSelection::current:
            commonSetup()->setCameraSelection(cameraSelection);
            break;

        case core::EventSearch::CameraSelection::custom:
            commonSetup()->setSelectedCamerasIds(
                system()->userSettings()->objectSearchCustomCameras());
            break;

        default:
            commonSetup()->setCameraSelection(core::EventSearch::CameraSelection::layout);
            break;
    }
}

void AnalyticsSearchSynchronizer::updateEngineIdFromSettings()
{
    const nx::Uuid& engineId = system()->userSettings()->objectSearchEngineId();

    if (m_analyticsSetup->engine() == engineId)
        return;

    if (system()->taxonomyManager()->currentTaxonomy()->engineById(engineId.toString()))
        m_analyticsSetup->setEngine(engineId);
    else
        m_analyticsSetup->setEngine({});
}

void AnalyticsSearchSynchronizer::updateFiltersFromSettings()
{
    updateCameraSelectionFromSettings();
    updateEngineIdFromSettings();
    updateObjectTypesFromSettings();
    updateTimeSelectionFromSettings();
    updateAreaFromSettings();
    updateAttributeFiltersFromSettings();
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

void AnalyticsSearchSynchronizer::ensureVisible(milliseconds timestamp, const nx::Uuid& trackId)
{
    for (auto instance: instances())
    {
        if (auto setup = instance->m_analyticsSetup)
            emit setup->ensureVisibleRequested(timestamp, trackId);
    }
}

void AnalyticsSearchSynchronizer::setupInstanceSynchronization()
{
    if (!NX_ASSERT(commonSetup() && m_analyticsSetup))
        return;

    connect(this, &AnalyticsSearchSynchronizer::activeChanged, this,
        [this]()
        {
            for (auto instance: instancesToNotify())
                instance->setActive(active());
        });

    connect(commonSetup()->textFilter(), &core::TextFilterSetup::textChanged, this,
        [this]()
        {
            for (auto instance: instancesToNotify())
                instance->commonSetup()->textFilter()->setText(commonSetup()->textFilter()->text());
        });

    connect(commonSetup(), &CommonObjectSearchSetup::timeSelectionChanged, this,
        [this]()
        {
            const core::EventSearch::TimeSelection timeSelection = commonSetup()->timeSelection();

            for (auto instance: instancesToNotify())
                instance->commonSetup()->setTimeSelection(timeSelection);
        });

    connect(commonSetup(), &CommonObjectSearchSetup::cameraSelectionChanged, this,
        [this]()
        {
            const auto cameraSelection = commonSetup()->cameraSelection();
            for (auto instance: instancesToNotify())
                instance->commonSetup()->setCameraSelection(cameraSelection);
        });

    connect(commonSetup(), &CommonObjectSearchSetup::selectedCamerasChanged, this,
        [this]()
        {
            if (commonSetup()->cameraSelection() != core::EventSearch::CameraSelection::custom)
                return;

            const QnVirtualCameraResourceSet& selectedCameras = commonSetup()->selectedCameras();
            for (auto instance: instancesToNotify())
                instance->commonSetup()->setSelectedCameras(selectedCameras);
        });

    connect(m_analyticsSetup, &core::AnalyticsSearchSetup::engineChanged, this,
        [this]()
        {
            const nx::Uuid engineId = m_analyticsSetup->engine();

            for (auto instance: instancesToNotify())
                instance->m_analyticsSetup->setEngine(engineId);
        });

    connect(m_analyticsSetup, &core::AnalyticsSearchSetup::objectTypesChanged, this,
        [this]()
        {
            const auto objectTypes = m_analyticsSetup->objectTypes();
            for (auto instance: instancesToNotify())
                instance->m_analyticsSetup->setObjectTypes(objectTypes);
        });

    connect(m_analyticsSetup, &core::AnalyticsSearchSetup::attributeFiltersChanged, this,
        [this]()
        {
            const QStringList& attributeFilters = m_analyticsSetup->attributeFilters();

            for (auto instance: instancesToNotify())
                instance->m_analyticsSetup->setAttributeFilters(attributeFilters);
        });

    connect(m_analyticsSetup, &core::AnalyticsSearchSetup::areaChanged, this,
        [this]()
        {
            const QRectF area = m_analyticsSetup->area();

            for (auto instance: instancesToNotify())
                instance->m_analyticsSetup->setArea(area);
        });

    connect(m_analyticsSetup, &core::AnalyticsSearchSetup::areaSelectionActiveChanged, this,
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

    commonSetup()->textFilter()->setText(master->commonSetup()->textFilter()->text());
    commonSetup()->setTimeSelection(master->commonSetup()->timeSelection());
    commonSetup()->setCameraSelection(master->commonSetup()->cameraSelection());

    if (commonSetup()->cameraSelection() == core::EventSearch::CameraSelection::custom)
        commonSetup()->setSelectedCameras(master->commonSetup()->selectedCameras());

    m_analyticsSetup->setEngine(master->m_analyticsSetup->engine());
    m_analyticsSetup->setObjectTypes(master->m_analyticsSetup->objectTypes());
    m_analyticsSetup->setAttributeFilters(master->m_analyticsSetup->attributeFilters());
    m_analyticsSetup->setArea(master->m_analyticsSetup->area());
    m_analyticsSetup->setAreaSelectionActive(master->m_analyticsSetup->areaSelectionActive());
}

void AnalyticsSearchSynchronizer::setupSettingsStorage()
{
    if (!m_settingsSyncConnections.isEmpty())
        return;

    m_settingsSyncConnections << connect(system(), &SystemContext::userChanged, this,
        [this](const QnUserResourcePtr& user)
        {
            if (!user)
                m_settingsSyncConnections.reset();
        });

    m_settingsSyncConnections << connect(
        commonSetup(), &CommonObjectSearchSetup::selectedCamerasChanged, this,
        [this]()
        {
            // When the initial connection to the system is established, the settings become
            // available, but the layout is not loaded yet, so we do not have the list of the
            // selected cameras. This handler helps to resolve that issue. It re-applies
            // camera-dependent settings when cameras become available. As a side effect it tries
            // to re-apply the settings every time the camera selection is changed, but that does
            // not affect the behavior, as the settings are in sync with the current state.
            updateObjectTypesFromSettings();
            updateAreaFromSettings();
        });

    m_settingsSyncConnections << connect(
        commonSetup(), &CommonObjectSearchSetup::timeSelectionChanged, this,
        [this]()
        {
            const core::EventSearch::TimeSelection timeSelection = commonSetup()->timeSelection();

            if (timeSelection != core::EventSearch::TimeSelection::selection)
            {
                //< Timeline selection should not be saved according to the specification.
                system()->userSettings()->objectSearchTimeSelection = timeSelection;
            }
        });

    m_settingsSyncConnections << connect(
        commonSetup(), &CommonObjectSearchSetup::cameraSelectionChanged, this,
        [this]()
        {
            system()->userSettings()->objectSearchCameraSelection =
                commonSetup()->cameraSelection();
        });

    m_settingsSyncConnections << connect(
        commonSetup(), &CommonObjectSearchSetup::selectedCamerasChanged, this,
        [this]()
        {
            system()->userSettings()->objectSearchCustomCameras =
                commonSetup()->selectedCamerasIds();
        });

    m_settingsSyncConnections << connect(
        m_analyticsSetup, &core::AnalyticsSearchSetup::engineChanged, this,
        [this]()
        {
            system()->userSettings()->objectSearchEngineId = m_analyticsSetup->engine();
        });

    m_settingsSyncConnections << connect(
        m_analyticsSetup, &core::AnalyticsSearchSetup::objectTypesChanged, this,
        [this]()
        {
            system()->userSettings()->objectSearchObjectTypeIds = m_analyticsSetup->objectTypes();
        });

    m_settingsSyncConnections << connect(
        m_analyticsSetup, &core::AnalyticsSearchSetup::attributeFiltersChanged, this,
        [this]()
        {
            system()->userSettings()->objectSearchAttributeFilters =
                m_analyticsSetup->attributeFilters();
        });

    m_settingsSyncConnections << connect(
        m_analyticsSetup, &core::AnalyticsSearchSetup::areaChanged, this,
        [this]()
        {
            system()->userSettings()->objectSearchArea = m_analyticsSetup->area();
        });
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
