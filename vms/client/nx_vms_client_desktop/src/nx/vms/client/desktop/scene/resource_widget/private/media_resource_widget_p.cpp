// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "media_resource_widget_p.h"

#include <QtQml/QQmlEngine>

#include <analytics/db/analytics_db_types.h>
#include <api/server_rest_connection.h>
#include <camera/cam_display.h>
#include <camera/resource_display.h>
#include <client_core/client_core_module.h>
#include <core/resource/client_camera.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/motion_window.h>
#include <core/resource_access/resource_access_filter.h>
#include <core/resource_management/resource_pool.h>
#include <nx/analytics/metadata_log_parser.h>
#include <nx/streaming/abstract_archive_stream_reader.h>
#include <nx/utils/guarded_callback.h>
#include <nx/vms/client/core/analytics/analytics_metadata_provider_factory.h>
#include <nx/vms/client/core/media/consuming_motion_metadata_provider.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/ui/graphics/items/resource/widget_analytics_controller.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>
#include <nx/vms/license/usage_helper.h>
#include <utils/common/delayed.h>

using nx::vms::api::StreamDataFilter;
using nx::vms::api::StreamDataFilters;
using namespace std::chrono;
using namespace nx::vms::client::core::analytics;

namespace nx::vms::client::desktop {

namespace {

template<typename T>
nx::media::AbstractMetadataConsumerPtr getMetadataConsumer(T* provider)
{
    const auto consumingProvider =
        dynamic_cast<nx::vms::client::core::AbstractMetadataConsumerOwner*>(provider);

    return consumingProvider
        ? consumingProvider->metadataConsumer()
        : nx::media::AbstractMetadataConsumerPtr();
}

}  // namespace

//-------------------------------------------------------------------------------------------------

/**
 * Since we have zoom windows which just use display from the source widget we can't turn on/off
 * anlytics objects for each widget independently. To have an ability to do this we introduce
 * this watcher. Watcher observes if there is at least one widget which wants to show analytics
 * objects and calculates if metadata stream should be enabled or disabled for the source display.
 */
class AnalyticsAvailabilityWatcher
{
public:
    static AnalyticsAvailabilityWatcher& instance();

    void setAnalyticsEnabled(
        const QnResourceDisplayPtr& display,
        MediaResourceWidgetPrivate* widget,
        bool enabled);

    bool isAnalyticsEnabled(const QnResourceDisplayPtr& display) const;

    bool isEnabledForWidget(
        const QnResourceDisplayPtr& display,
        MediaResourceWidgetPrivate* widget) const;

private:
    using WidgetsSet = QSet<MediaResourceWidgetPrivate*>;
    QHash<QnResourceDisplayPtr, WidgetsSet> m_widgetsForDisplay;
};

AnalyticsAvailabilityWatcher& AnalyticsAvailabilityWatcher::instance()
{
    static AnalyticsAvailabilityWatcher kInstance;
    return kInstance;
}

void AnalyticsAvailabilityWatcher::setAnalyticsEnabled(
    const QnResourceDisplayPtr& display,
    MediaResourceWidgetPrivate* widget,
    bool enabled)
{
    if (!display)
        return;

    const auto itWidgets = m_widgetsForDisplay.find(display);
    if (itWidgets == m_widgetsForDisplay.end())
    {
        if (enabled)
            m_widgetsForDisplay.insert(display, {widget});

        return;
    }

    if (enabled)
    {
        itWidgets->insert(widget);
        return;
    }

    itWidgets->remove(widget);
    if (itWidgets->isEmpty())
        m_widgetsForDisplay.erase(itWidgets);
}

bool AnalyticsAvailabilityWatcher::isAnalyticsEnabled(const QnResourceDisplayPtr& display) const
{
    const auto itWidgets = m_widgetsForDisplay.find(display);
    return itWidgets != m_widgetsForDisplay.end() && !itWidgets->isEmpty();
}

bool AnalyticsAvailabilityWatcher::isEnabledForWidget(
    const QnResourceDisplayPtr& display,
    MediaResourceWidgetPrivate* widget) const
{
    const auto itWidgets = m_widgetsForDisplay.find(display);
    return itWidgets != m_widgetsForDisplay.end() && itWidgets->contains(widget);
}

//-------------------------------------------------------------------------------------------------

MediaResourceWidgetPrivate::MediaResourceWidgetPrivate(
    const QnResourcePtr& resource,
    QObject* parent)
    :
    base_type(parent),
    resource(resource),
    mediaResource(resource.dynamicCast<QnMediaResource>()),
    camera(resource.dynamicCast<QnClientCameraResource>()),
    hasVideo(mediaResource->hasVideo(nullptr)),
    isIoModule(camera && camera->hasFlags(Qn::io_module)),
    motionMetadataProvider(new client::core::ConsumingMotionMetadataProvider()),
    analyticsMetadataProvider(
        AnalyticsMetadataProviderFactory::instance()->createMetadataProvider(resource)),
    taxonomyManager(SystemContext::fromResource(resource)->taxonomyManager()),
    m_accessController(SystemContext::fromResource(resource)->accessController()),
    m_accessNotifier(m_accessController
        ? m_accessController->createNotifier(resource)
        : AccessController::NotifierPtr{})
{
    QSignalBlocker blocker(this);

    NX_ASSERT(mediaResource && m_accessController && m_accessNotifier);

    ioModuleMonitor.reset(new core::IOModuleMonitor(camera));

    connect(resource.get(), &QnResource::statusChanged, this,
        &MediaResourceWidgetPrivate::updateIsOffline);
    connect(resource.get(), &QnResource::statusChanged, this,
        &MediaResourceWidgetPrivate::updateIsUnauthorized);

    connect(m_accessNotifier.get(), &AccessController::Notifier::permissionsChanged, this,
        &MediaResourceWidgetPrivate::updateAccess);

    if (camera)
    {
        using namespace nx::vms::license;
        m_licenseStatusHelper.reset(new SingleCamLicenseStatusHelper(camera));
        connect(m_licenseStatusHelper.get(), &SingleCamLicenseStatusHelper::licenseStatusChanged,
            this, &MediaResourceWidgetPrivate::licenseStatusChanged);
        connect(m_licenseStatusHelper.get(), &SingleCamLicenseStatusHelper::licenseStatusChanged,
            this, &MediaResourceWidgetPrivate::stateChanged);

        connect(camera.get(), &QnVirtualCameraResource::userEnabledAnalyticsEnginesChanged, this,
            &MediaResourceWidgetPrivate::updateIsAnalyticsSupported);
        connect(camera.get(), &QnVirtualCameraResource::compatibleAnalyticsEnginesChanged, this,
            &MediaResourceWidgetPrivate::updateIsAnalyticsSupported);
        connect(camera.get(), &QnVirtualCameraResource::compatibleObjectTypesMaybeChanged, this,
            &MediaResourceWidgetPrivate::updateIsAnalyticsSupported);
        connect(camera.get(), &QnVirtualCameraResource::motionRegionChanged, this,
            [this] { m_motionSkipMaskCache.reset(); } );

        if (taxonomyManager)
        {
            connect(taxonomyManager, &TaxonomyManager::currentTaxonomyChanged,
                this, &MediaResourceWidgetPrivate::updateIsAnalyticsSupported);
        }

        updateAccess();
        updateIsAnalyticsSupported();

        const QString debugAnalyticsLogFile = ini().debugAnalyticsVideoOverlayFromLogFile;
        if (!debugAnalyticsLogFile.isEmpty())
        {
            auto metadataLogParser = std::make_unique<nx::analytics::MetadataLogParser>();
            if (metadataLogParser->loadLogFile(debugAnalyticsLogFile))
                std::swap(metadataLogParser, this->analyticsMetadataLogParser);
        }
    }
}

MediaResourceWidgetPrivate::~MediaResourceWidgetPrivate()
{
    AnalyticsAvailabilityWatcher::instance().setAnalyticsEnabled(display(), this, false);
    if (const auto consumer = motionMetadataConsumer())
        m_display->removeMetadataConsumer(consumer);
    if (const auto consumer = analyticsMetadataConsumer())
        m_display->removeMetadataConsumer(consumer);
}

QnResourceDisplayPtr MediaResourceWidgetPrivate::display() const
{
    return m_display;
}

void MediaResourceWidgetPrivate::setDisplay(const QnResourceDisplayPtr& display)
{
    // TODO: #dklychkov Make QnMediaResourceWidget::setDisplay work with the previous
    // implementation which has an assert for repeated setDisplay call.

    if (display == m_display)
        return;

    const bool anlyticsEnabledForWidget =
        AnalyticsAvailabilityWatcher::instance().isEnabledForWidget(m_display, this);
    if (anlyticsEnabledForWidget)
        AnalyticsAvailabilityWatcher::instance().setAnalyticsEnabled(m_display, this, false);

    if (m_display)
        m_display->camDisplay()->disconnect(this);

    m_display = display;

    if (anlyticsEnabledForWidget)
        AnalyticsAvailabilityWatcher::instance().setAnalyticsEnabled(m_display, this, true);

    if (m_display)
    {
        connect(m_display->camDisplay(), &QnCamDisplay::liveMode, this,
            &MediaResourceWidgetPrivate::updateIsPlayingLive);
    }

    {
        QSignalBlocker blocker(this);
        updateIsPlayingLive();
        updateIsOffline();
        updateIsUnauthorized();
    }
}

bool MediaResourceWidgetPrivate::isPlayingLive() const
{
    return m_isPlayingLive;
}

bool MediaResourceWidgetPrivate::isOffline() const
{
    return m_isOffline;
}

bool MediaResourceWidgetPrivate::isUnauthorized() const
{
    return m_isUnauthorized;
}

bool MediaResourceWidgetPrivate::hasAccess() const
{
    return m_hasAccess;
}

MediaResourceWidgetPrivate::AccessController* MediaResourceWidgetPrivate::accessController() const
{
    return m_accessController;
}

bool MediaResourceWidgetPrivate::supportsBasicPtz() const
{
    return supportsPtzCapabilities(Ptz::ContinuousPtzCapabilities | Ptz::ViewportPtzCapability);
}

bool MediaResourceWidgetPrivate::supportsPtzCapabilities(Ptz::Capabilities capabilities) const
{
    // Ptz is not available for the local files.
    if (!camera)
        return false;

    if (camera->hasAnyOfPtzCapabilities(Ptz::VirtualPtzCapability))
        return false;

    // Ptz is forbidden in exported files or on search layouts.
    if (isExportedLayout || isPreviewSearchLayout)
        return false;

    // Check if camera supports the capabilities.
    if (!camera->hasAnyOfPtzCapabilities(capabilities))
        return false;

    // Check permissions on the current camera.
    if (!m_accessController->hasPermissions(camera, Qn::WritePtzPermission))
        return false;

    // Check ptz redirect.
    if (!camera->isPtzRedirected())
        return true;

    const auto actualPtzTarget = camera->ptzRedirectedTo();

    // Ptz is redirected nowhere.
    if (!actualPtzTarget)
        return false;

    // Ptz is redirected to non-ptz camera.
    if (!actualPtzTarget->hasAnyOfPtzCapabilities(capabilities))
        return false;

    return m_accessController->hasPermissions(actualPtzTarget, Qn::WritePtzPermission);
}

nx::vms::license::UsageStatus MediaResourceWidgetPrivate::licenseStatus() const
{
    return m_licenseStatusHelper->status();
}

QSharedPointer<media::AbstractMetadataConsumer>
    MediaResourceWidgetPrivate::motionMetadataConsumer() const
{
    return getMetadataConsumer(motionMetadataProvider.data());
}

QSharedPointer<media::AbstractMetadataConsumer>
    MediaResourceWidgetPrivate::analyticsMetadataConsumer() const
{
    return getMetadataConsumer(analyticsMetadataProvider.data());
}

void MediaResourceWidgetPrivate::setMotionEnabled(bool enabled)
{
    setStreamDataFilter(nx::vms::api::StreamDataFilter::motion, enabled);
}

bool MediaResourceWidgetPrivate::isAnalyticsEnabledInStream() const
{
    if (!isAnalyticsSupported || m_forceDisabledAnalytics)
        return false;

    if (const auto reader = display()->archiveReader())
        return reader->streamDataFilter().testFlag(StreamDataFilter::objects);

    return false;
}

void MediaResourceWidgetPrivate::setAnalyticsEnabledInStream(bool enabled)
{
    NX_VERBOSE(this, "Setting analytics frames %1", enabled ? "enabled" : "disabled");
    AnalyticsAvailabilityWatcher::instance().setAnalyticsEnabled(display(), this, enabled);

    setStreamDataFilter(nx::vms::api::StreamDataFilter::objects,
        AnalyticsAvailabilityWatcher::instance().isAnalyticsEnabled(display()));
}

void MediaResourceWidgetPrivate::setAnalyticsFilter(const nx::analytics::db::Filter& value)
{
    m_forceDisabledAnalytics = !resource;

    if (m_forceDisabledAnalytics)
        analyticsController->clearAreas();

    NX_VERBOSE(this, "Update analytics filter to %1", value);
    analyticsController->setFilter(value);
}

const char* const MediaResourceWidgetPrivate::motionSkipMask(int channel) const
{
    static const MotionSkipMask kEmptyMask = QnRegion();

    ensureMotionSkip();
    return channel < m_motionSkipMaskCache->size()
        ? m_motionSkipMaskCache->at(channel).bitMask()
        : kEmptyMask.bitMask();
}

qreal MediaResourceWidgetPrivate::getStatisticsFps(int channelCount) const
{
    qreal fps = 0.0;

    for (int i = 0; i < channelCount; i++)
    {
        const auto provider = display()->mediaProvider();
        const QnMediaStreamStatistics* statistics = provider->getStatistics(i);
        fps = qMax(fps, static_cast<qreal>(statistics->getFrameRate()));
    }

    return fps;
}

void MediaResourceWidgetPrivate::updateIsPlayingLive()
{
    setIsPlayingLive(m_display && m_display->camDisplay()->isRealTimeSource());
}

void MediaResourceWidgetPrivate::setIsPlayingLive(bool value)
{
    if (m_isPlayingLive == value)
        return;

    m_isPlayingLive = value;

    {
        QSignalBlocker blocker(this);
        updateIsOffline();
        updateIsUnauthorized();
    }

    emit stateChanged();
}

void MediaResourceWidgetPrivate::updateIsOffline()
{
    setIsOffline(m_isPlayingLive && (resource->getStatus() == nx::vms::api::ResourceStatus::offline));
}

void MediaResourceWidgetPrivate::setIsOffline(bool value)
{
    if (m_isOffline == value)
        return;

    m_isOffline = value;
    emit stateChanged();
}

void MediaResourceWidgetPrivate::updateIsUnauthorized()
{
    setIsUnauthorized(m_isPlayingLive && (resource->getStatus() == nx::vms::api::ResourceStatus::unauthorized));
}

void MediaResourceWidgetPrivate::setIsUnauthorized(bool value)
{
    if (m_isUnauthorized == value)
        return;

    m_isUnauthorized = value;
    emit stateChanged();
}

void MediaResourceWidgetPrivate::updateAccess()
{
    const auto requiredPermission = QnResourceAccessFilter::isShareableMedia(resource)
        ? Qn::ViewContentPermission
        : Qn::ReadPermission;

    setHasAccess(m_accessController->hasPermissions(resource, requiredPermission));
}

void MediaResourceWidgetPrivate::setHasAccess(bool value)
{
    if (m_hasAccess == value)
        return;

    m_hasAccess = value;
    emit stateChanged();
}

bool MediaResourceWidgetPrivate::calculateIsAnalyticsSupported() const
{
    // Quick check if there are no objects-supporting Analytics Engines in the System.
    if (!taxonomyManager || taxonomyManager->relevantEngines().empty())
        return false;

    // Check whether at least one Analytics Engine in the System supports this Camera and also
    // supports objects.
    const auto taxonomy = taxonomyManager->currentTaxonomy();
    const auto compatibleEngines = camera->compatibleAnalyticsEngines();
    return std::any_of(compatibleEngines.cbegin(), compatibleEngines.cend(),
        [&](const QnUuid& engineId)
        {
            const auto engine = taxonomy->engineById(engineId.toString());
            // Engine can be absent for cross-system Cameras.
            return engine && taxonomyManager->isEngineRelevant(engine);
        });
}

void MediaResourceWidgetPrivate::updateIsAnalyticsSupported()
{
    bool isAnalyticsSupported = calculateIsAnalyticsSupported();
    if (this->isAnalyticsSupported == isAnalyticsSupported)
        return;

    this->isAnalyticsSupported = isAnalyticsSupported;
    emit analyticsSupportChanged();
}

void MediaResourceWidgetPrivate::setStreamDataFilter(StreamDataFilter filter, bool on)
{
    if (auto reader = display()->archiveReader())
    {
        StreamDataFilters filters = reader->streamDataFilter();
        filters.setFlag(filter, on);
        if (filters.testFlag(StreamDataFilter::motion)
            || filters.testFlag(StreamDataFilter::objects))
        {
            // Ensure filters are consistent.
            filters.setFlag(StreamDataFilter::media);
        }
        setStreamDataFilters(filters);
    }
}

void MediaResourceWidgetPrivate::setStreamDataFilters(StreamDataFilters filters)
{
    auto reader = display()->archiveReader();
    if (!reader)
        return;

    if (!reader->setStreamDataFilter(filters))
        return;

    if (display()->camDisplay()->isRealTimeSource())
        return;

    const auto positionUsec = display()->camDisplay()->getExternalTime();
    if (positionUsec == AV_NOPTS_VALUE || positionUsec == DATETIME_NOW)
        return;

    reader->jumpTo(/*mksek*/ positionUsec, /*skipTime*/ positionUsec);
}

void MediaResourceWidgetPrivate::ensureMotionSkip() const
{
    if (m_motionSkipMaskCache)
        return;

    m_motionSkipMaskCache = std::vector<MotionSkipMask>();

    if (camera)
    {
        const QList<QnMotionRegion> motionRegions = camera->getMotionRegionList();
        m_motionSkipMaskCache->reserve(motionRegions.size());
        for (const QnMotionRegion& region: motionRegions)
            m_motionSkipMaskCache->push_back(MotionSkipMask(region.getMotionMask()));
    }
}

} // namespace nx::vms::client::desktop
