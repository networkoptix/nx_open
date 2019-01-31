#include "media_resource_helper.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/param.h>
#include <utils/common/connective.h>

#include <nx/fusion/model_functions.h>

namespace nx::vms::client::core {

namespace {

static constexpr bool kFisheyeDewarpingFeatureEnabled = true;

} // namespace

class MediaResourceHelper::Private: public Connective<QObject>
{
    MediaResourceHelper* const q = nullptr;

public:
    QnVirtualCameraResourcePtr camera;
    QnMediaServerResourcePtr server;

    Private(MediaResourceHelper* q);

    void handlePropertyChanged(const QnResourcePtr& resource, const QString& key);
    void updateServer();
    void handleResourceChanged();
};

MediaResourceHelper::MediaResourceHelper(QObject* parent):
    base_type(parent),
    d(new Private(this))
{
    connect(this, &ResourceHelper::resourceIdChanged, d, &Private::handleResourceChanged);
}

MediaResourceHelper::~MediaResourceHelper()
{
}

bool MediaResourceHelper::isWearableCamera() const
{
    return d->camera && d->camera->flags().testFlag(Qn::wearable_camera);
}

bool MediaResourceHelper::analogCameraWithoutLicense() const
{
    return d->camera && d->camera->isDtsBased() && !d->camera->isLicenseUsed();
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
    return d->camera ? d->camera->getProperty(QnMediaResource::rotationKey()).toInt() : 0;
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

QnMediaDewarpingParams MediaResourceHelper::fisheyeParams() const
{
    return (d->camera && kFisheyeDewarpingFeatureEnabled)
        ? d->camera->getDewarpingParams()
        : QnMediaDewarpingParams();
}

MediaResourceHelper::Private::Private(MediaResourceHelper* q):
    q(q)
{
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
    else if (key == QnMediaResource::rotationKey())
    {
        emit q->customRotationChanged();
    }
    else if (key == ResourcePropertyKey::kMediaStreams)
    {
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
    if (currentResource && !cameraResource)
    {
        q->setResourceId(QString()); //< We support only camera resources.
        return;
    }

    if (camera)
        camera->disconnect(this);

    updateServer();

    camera = cameraResource;

    if (camera)
    {
        connect(camera, &QnResource::propertyChanged, this, &Private::handlePropertyChanged);
        connect(camera, &QnResource::parentIdChanged, this, &Private::updateServer);

        connect(camera, &QnResource::videoLayoutChanged,
            q, &MediaResourceHelper::videoLayoutChanged);
        connect(camera, &QnResource::mediaDewarpingParamsChanged,
            q, &MediaResourceHelper::fisheyeParamsChanged);
        connect(camera, &QnVirtualCameraResource::licenseUsedChanged,
            q, &MediaResourceHelper::analogCameraWithoutLicenseChanged);

        updateServer();
    }

    emit q->customAspectRatioChanged();
    emit q->customRotationChanged();
    emit q->resourceStatusChanged();
    emit q->videoLayoutChanged();
    emit q->fisheyeParamsChanged();
    emit q->analogCameraWithoutLicenseChanged();
    emit q->wearableCameraChanged();
}

} // namespace nx::vms::client::core
