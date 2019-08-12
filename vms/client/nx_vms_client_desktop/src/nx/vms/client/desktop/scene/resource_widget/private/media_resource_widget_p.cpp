#include "media_resource_widget_p.h"

#include <analytics/db/analytics_db_types.h>

#include <api/server_rest_connection.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/client_camera.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>

#include <nx/streaming/abstract_archive_stream_reader.h>

#include <camera/cam_display.h>
#include <camera/resource_display.h>
#include <nx/client/core/media/consuming_motion_metadata_provider.h>
#include <nx/vms/client/desktop/ui/graphics/items/resource/widget_analytics_controller.h>
#include <nx/vms/client/desktop/analytics/analytics_metadata_provider_factory.h>
#include <ui/workbench/workbench_access_controller.h>
#include <nx/vms/client/desktop/ini.h>

#include <utils/license_usage_helper.h>

#include <nx/analytics/metadata_log_parser.h>

#include <nx/utils/guarded_callback.h>

using nx::vms::api::StreamDataFilter;
using nx::vms::api::StreamDataFilters;
using namespace std::chrono;

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

MediaResourceWidgetPrivate::MediaResourceWidgetPrivate(
    const QnResourcePtr& resource,
    QnWorkbenchAccessController* accessController,
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
    m_accessController(accessController)
{
    QSignalBlocker blocker(this);

    NX_ASSERT(mediaResource);

    connect(resource, &QnResource::statusChanged, this,
        &MediaResourceWidgetPrivate::updateIsOffline);
    connect(resource, &QnResource::statusChanged, this,
        &MediaResourceWidgetPrivate::updateIsUnauthorized);

    if (camera)
    {
        m_licenseStatusHelper.reset(new QnSingleCamLicenseStatusHelper(camera));
        connect(m_licenseStatusHelper, &QnSingleCamLicenseStatusHelper::licenseStatusChanged,
            this, &MediaResourceWidgetPrivate::licenseStatusChanged);
        connect(m_licenseStatusHelper, &QnSingleCamLicenseStatusHelper::licenseStatusChanged,
            this, &MediaResourceWidgetPrivate::stateChanged);

        connect(camera, &QnVirtualCameraResource::userEnabledAnalyticsEnginesChanged, this,
			&MediaResourceWidgetPrivate::updateIsAnalyticsSupported);
        connect(camera, &QnVirtualCameraResource::compatibleAnalyticsEnginesChanged, this,
			&MediaResourceWidgetPrivate::updateIsAnalyticsSupported);

        updateIsAnalyticsSupported();
        requestAnalyticsObjectsExistence();

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

    if (m_display)
        m_display->camDisplay()->disconnect(this);

    m_display = display;

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

bool MediaResourceWidgetPrivate::canControlPtz() const
{
    // Ptz is not available for the local files.
    if (!camera)
        return false;

    // Ptz is forbidden in exported files or on search layouts.
    if (isExportedLayout || isPreviewSearchLayout)
        return false;

    // Check if we can control at least something on the current camera and fisheye is disabled.
    if (!camera->isPtzSupported())
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
    if (!actualPtzTarget->isPtzSupported())
        return false;

    return m_accessController->hasPermissions(actualPtzTarget, Qn::WritePtzPermission);
}

QnLicenseUsageStatus MediaResourceWidgetPrivate::licenseStatus() const
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

bool MediaResourceWidgetPrivate::isAnalyticsEnabled() const
{
    if (!isAnalyticsSupported || m_forceDisabledAnalytics)
        return false;

    if (const auto reader = display()->archiveReader())
        return reader->streamDataFilter().testFlag(StreamDataFilter::objectDetection);

    return false;
}

void MediaResourceWidgetPrivate::setAnalyticsEnabled(bool enabled)
{
    setStreamDataFilter(nx::vms::api::StreamDataFilter::objectDetection, enabled);

    if (enabled)
    {
        display()->camDisplay()->setForcedVideoBufferLength(
            milliseconds(ini().analyticsVideoBufferLengthMs));
    }
    else
    {
        analyticsController->clearAreas();

        // Earlier we didn't unset forced video buffer length to avoid micro-freezes required to
        // fill in the video buffer on succeeding analytics mode activations. But it looks like this
        // mode causes a lot of glitches when enabled, so we'd prefer to leave on a safe side.
        display()->camDisplay()->setForcedVideoBufferLength(milliseconds::zero());
    }
}

void MediaResourceWidgetPrivate::setAnalyticsFilter(const nx::analytics::db::Filter& value)
{
    m_forceDisabledAnalytics = !resource || (!value.deviceIds.empty()
        && std::find(value.deviceIds.cbegin(), value.deviceIds.cend(), resource->getId())
            == value.deviceIds.cend());

    if (m_forceDisabledAnalytics)
        analyticsController->clearAreas();

    analyticsController->setFilter(value);
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
    setIsOffline(m_isPlayingLive && (resource->getStatus() == Qn::Offline));
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
    setIsUnauthorized(m_isPlayingLive && (resource->getStatus() == Qn::Unauthorized));
}

void MediaResourceWidgetPrivate::setIsUnauthorized(bool value)
{
    if (m_isUnauthorized == value)
        return;

    m_isUnauthorized = value;
    emit stateChanged();
}

bool MediaResourceWidgetPrivate::calculateIsAnalyticsSupported() const
{
    // If we found some analytics objects in the archive, that's enough.
    if (m_analyticsObjectsFound)
        return true;

    // Analytics stream should be enabled only for cameras with enabled analytics engine and only
    // if at least one of these engines has the objects support.
    if (!camera || !analyticsMetadataProvider)
        return false;

    const auto engines = camera->enabledAnalyticsEngineResources();
    return std::any_of(engines.cbegin(), engines.cend(),
        [](const common::AnalyticsEngineResourcePtr& engine)
        {
            return !engine->manifest().objectTypes.empty();
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

void MediaResourceWidgetPrivate::requestAnalyticsObjectsExistence()
{
    NX_ASSERT(camera);
    const auto server = camera->getParentServer();
    if (!NX_ASSERT(server))
        return;

    const auto connection = server->restConnection();
    if (!NX_ASSERT(connection))
        return;

    nx::analytics::db::Filter filter;
    filter.deviceIds.push_back(camera->getId());
    filter.maxObjectTracksToSelect = 1;
    filter.timePeriod = {QnTimePeriod::kMinTimeValue, QnTimePeriod::kMaxTimeValue};

    auto callback = nx::utils::guarded(this,
        [this](bool success, rest::Handle /*handle*/, analytics::db::LookupResult&& result)
        {
            if (m_analyticsObjectsFound)
                return;

            m_analyticsObjectsFound = success && !result.empty();
            if (m_analyticsObjectsFound)
                updateIsAnalyticsSupported();

            if (!success)
                requestAnalyticsObjectsExistence();
        });
    connection->lookupObjectTracks(filter, /*isLocal*/ false, callback, this->thread());
}

void MediaResourceWidgetPrivate::setStreamDataFilter(StreamDataFilter filter, bool on)
{
    if (auto reader = display()->archiveReader())
    {
        StreamDataFilters filters = reader->streamDataFilter();
        filters.setFlag(filter, on);
        if (filters.testFlag(StreamDataFilter::motion)
            || filters.testFlag(StreamDataFilter::objectDetection))
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

    const auto positionUsec = display()->camDisplay()->getExternalTime();
    const auto isPaused = reader->isMediaPaused();

    if (isPaused)
        reader->jumpTo(/*mksek*/ positionUsec, /*skipTime*/ positionUsec);
    else
        reader->jumpTo(/*mksek*/ positionUsec, /*skipTime*/ 0);
}

} // namespace nx::vms::client::desktop
