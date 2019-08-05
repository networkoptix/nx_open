#include "media_resource_widget_p.h"

#include <core/resource_management/resource_pool.h>

#include <core/resource/client_camera.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>

#include <camera/cam_display.h>
#include <camera/resource_display.h>
#include <nx/client/core/media/consuming_motion_metadata_provider.h>
#include <nx/vms/client/desktop/ui/graphics/items/resource/widget_analytics_controller.h>
#include <nx/vms/client/desktop/analytics/analytics_metadata_provider_factory.h>
#include <ui/workbench/workbench_access_controller.h>

#include <utils/license_usage_helper.h>

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

} // namespace nx::vms::client::desktop
