// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "media_resource_helper.h"

#include <core/resource/camera_media_stream_info.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource_media_layout.h>
#include <core/resource/resource_property_key.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/log/format.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/cross_system/cloud_cross_system_context.h>
#include <nx/vms/client/core/cross_system/cloud_cross_system_manager.h>
#include <nx/vms/client/core/cross_system/cross_system_camera_resource.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/client/core/system_finder/system_description.h>

namespace nx::vms::client::core {

namespace {

static constexpr bool kFisheyeDewarpingFeatureEnabled = true;

} // namespace

class MediaResourceHelper::Private: public QObject
{
    MediaResourceHelper* const q = nullptr;

public:
    QnVirtualCameraResourcePtr camera;
    QnMediaServerResourcePtr server;
    QPointer<CloudCrossSystemContext> crossSystem;

    Private(MediaResourceHelper* q);

    void handlePropertyChanged(const QnResourcePtr& resource, const QString& key);
    void updateServer();
    void handleResourceChanged();
};

MediaResourceHelper::MediaResourceHelper(QObject* parent):
    base_type(parent),
    d(new Private(this))
{
    connect(this, &ResourceHelper::resourceNameChanged,
        this, &MediaResourceHelper::qualifiedResourceNameChanged);

    connect(this, &MediaResourceHelper::crossSystemNameChanged,
        this, &MediaResourceHelper::qualifiedResourceNameChanged);
}

MediaResourceHelper::~MediaResourceHelper()
{
}

bool MediaResourceHelper::isVirtualCamera() const
{
    return d->camera && d->camera->flags().testFlag(Qn::virtual_camera);
}

bool MediaResourceHelper::audioEnabled() const
{
    return d->camera && d->camera->isAudioEnabled();
}

MediaPlayer::VideoQuality MediaResourceHelper::livePreviewVideoQuality() const
{
    if (!d->camera)
        return nx::media::Player::LowIframesOnlyVideoQuality;

    static const auto isLowNativeStream =
        [](const CameraMediaStreamInfo& info)
        {
            if (info.transcodingRequired)
                return false;

            static const QSize kMaximalSize(800, 600);
            const auto size = info.getResolution();
            return !size.isEmpty()
                && size.width() <= kMaximalSize.width()
                && size.height() < kMaximalSize.height();
        };

    if (isLowNativeStream(d->camera->streamInfo(nx::vms::api::StreamIndex::secondary)))
        return nx::media::Player::LowVideoQuality;

    // Checks if primary stream is low quality one.
    return isLowNativeStream(d->camera->streamInfo(nx::vms::api::StreamIndex::primary))
        ? nx::media::Player::HighVideoQuality
        : nx::media::Player::LowIframesOnlyVideoQuality;
}

bool MediaResourceHelper::analogCameraWithoutLicense() const
{
    return d->camera && d->camera->isDtsBased() && !d->camera->isScheduleEnabled();
}

QString MediaResourceHelper::serverName() const
{
    return d->server ? d->server->getName() : QString();
}

qreal MediaResourceHelper::customAspectRatio() const
{
    if (!d->camera)
        return 0.0;

    const auto customRatio = d->camera->customAspectRatio();
    return customRatio.isValid() ? customRatio.toFloat() : 0.0;
}

qreal MediaResourceHelper::aspectRatio() const
{
    if (!d->camera)
        return 0.0;

    const auto ratio = d->camera->aspectRatio();
    return ratio.isValid() ? ratio.toFloat() : 0.0;
}

int MediaResourceHelper::customRotation() const
{
    return d->camera ? d->camera->forcedRotation().value_or(0) : 0;
}

int MediaResourceHelper::channelCount() const
{
    return d->camera ? d->camera->getVideoLayout()->channelCount() : 0;
}

QSize MediaResourceHelper::layoutSize() const
{
    return d->camera ? d->camera->getVideoLayout()->size() : QSize(1, 1);
}

QPoint MediaResourceHelper::channelPosition(int channel) const
{
    return d->camera ? d->camera->getVideoLayout()->position(channel) : QPoint();
}

MediaDewarpingParams MediaResourceHelper::fisheyeParams() const
{
    return (d->camera && kFisheyeDewarpingFeatureEnabled)
        ? MediaDewarpingParams(d->camera->getDewarpingParams())
        : MediaDewarpingParams();
}

bool MediaResourceHelper::needsCloudAuthorization() const
{
    return d->crossSystem && (d->crossSystem->needsCloudAuthorization()
        || d->crossSystem->status() == CloudCrossSystemContext::Status::connectionFailure);
}

QString MediaResourceHelper::crossSystemName() const
{
    if (!d->crossSystem)
        return {};

    const auto systemDescription = d->crossSystem->systemDescription();
    return systemDescription ? systemDescription->name() : QString{};
}

QString MediaResourceHelper::qualifiedResourceName() const
{
    return crossSystemName().isEmpty()
        ? resourceName()
        : nx::format("../%1", resourceName()).toQString();
}

void MediaResourceHelper::cloudAuthorize() const
{
    if (NX_ASSERT(d->crossSystem))
        d->crossSystem->cloudAuthorize();
}

MediaPlayer::VideoQuality MediaResourceHelper::streamQuality(
    nx::vms::api::StreamIndex streamIndex) const
{
    return streamIndex == nx::vms::api::StreamIndex::primary
        ? MediaPlayer::HighVideoQuality
        : MediaPlayer::LowVideoQuality;
}

MediaResourceHelper::Private::Private(MediaResourceHelper* q):
    q(q)
{
    connect(q, &ResourceHelper::resourceChanged, this, &Private::handleResourceChanged);
}

void MediaResourceHelper::Private::handlePropertyChanged(
    const QnResourcePtr& resource, const QString& key)
{
    if (camera != resource)
        return;

    if (key == QnMediaResource::customAspectRatioKey())
    {
        emit q->customAspectRatioChanged();
        emit q->aspectRatioChanged();
    }
    else if (key == nx::vms::api::device_properties::kMediaStreams)
    {
        emit q->livePreviewVideoQualityChanged();
        emit q->aspectRatioChanged();
    }
}

void MediaResourceHelper::Private::updateServer()
{
    if (server)
    {
        server->disconnect(this);
        server.clear();
    }

    if (camera)
    {
        server = camera->getParentServer();

        if (server)
        {
            connect(server.data(), &QnMediaServerResource::nameChanged,
                q, &MediaResourceHelper::serverNameChanged);
        }
    }

    emit q->serverNameChanged();
}

void MediaResourceHelper::Private::handleResourceChanged()
{
    const auto currentResource = q->resource();
    const auto cameraResource = currentResource.dynamicCast<QnVirtualCameraResource>();
    if (currentResource && !NX_ASSERT(cameraResource))
    {
        q->setResource({}); //< We support only camera resources.
        return;
    }

    if (camera)
        camera->disconnect(this);

    if (crossSystem)
    {
        crossSystem->disconnect(q);
        crossSystem.clear();
    }

    updateServer();

    camera = cameraResource;

    if (camera)
    {
        connect(camera.get(), &QnResource::propertyChanged, this, &Private::handlePropertyChanged);
        connect(camera.get(), &QnResource::parentIdChanged, this, &Private::updateServer);

        connect(camera.get(), &QnResource::videoLayoutChanged,
            q, &MediaResourceHelper::videoLayoutChanged);
        connect(camera.get(), &QnResource::mediaDewarpingParamsChanged,
            q, &MediaResourceHelper::fisheyeParamsChanged);
        connect(camera.get(), &QnVirtualCameraResource::scheduleEnabledChanged,
            q, &MediaResourceHelper::analogCameraWithoutLicenseChanged);
        connect(camera.get(), &QnResource::rotationChanged,
            q, &MediaResourceHelper::customRotationChanged);
        connect(camera.get(), &QnVirtualCameraResource::audioEnabledChanged,
            q, &MediaResourceHelper::audioEnabledChanged);

        updateServer();

        if (auto crossSystemCamera = camera.objectCast<CrossSystemCameraResource>())
        {
            crossSystem = appContext()->cloudCrossSystemManager()->systemContext(
                crossSystemCamera->systemId());

            if (crossSystem)
            {
                connect(crossSystem.get(), &CloudCrossSystemContext::needsCloudAuthorizationChanged,
                    q, &MediaResourceHelper::needsCloudAuthorizationChanged);

                connect(crossSystem.get(), &CloudCrossSystemContext::statusChanged,
                    q, &MediaResourceHelper::needsCloudAuthorizationChanged);

                connect(
                    crossSystem->systemDescription().get(), &SystemDescription::systemNameChanged,
                    q, &MediaResourceHelper::crossSystemNameChanged);
            }
        }
    }

    emit q->customAspectRatioChanged();
    emit q->customRotationChanged();
    emit q->resourceStatusChanged();
    emit q->videoLayoutChanged();
    emit q->fisheyeParamsChanged();
    emit q->analogCameraWithoutLicenseChanged();
    emit q->virtualCameraChanged();
    emit q->audioEnabledChanged();
    emit q->livePreviewVideoQualityChanged();
    emit q->needsCloudAuthorizationChanged();
    emit q->crossSystemNameChanged();
}

} // namespace nx::vms::client::core
