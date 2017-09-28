#include "media_resource_helper.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <nx/fusion/model_functions.h>
#include <utils/common/connective.h>

namespace {

static constexpr bool kFisheyeDewarpingFeatureEnabled = true;

} // namespace

class QnMediaResourceHelperPrivate : public Connective<QObject>
{
    QnMediaResourceHelper* q_ptr;
    Q_DECLARE_PUBLIC(QnMediaResourceHelper)

public:
    QnVirtualCameraResourcePtr camera;
    QnMediaServerResourcePtr server;

    QnMediaResourceHelperPrivate(QnMediaResourceHelper* parent);

    void handlePropertyChanged(const QnResourcePtr& resource, const QString& key);
    void updateServer();
    void handleResourceChanged();
};


QnMediaResourceHelper::QnMediaResourceHelper(QObject* parent):
    base_type(parent),
    d_ptr(new QnMediaResourceHelperPrivate(this))
{
    Q_D(QnMediaResourceHelper);

    connect(this, &QnResourceHelper::resourceIdChanged,
        d, &QnMediaResourceHelperPrivate::handleResourceChanged);
}

QnMediaResourceHelper::~QnMediaResourceHelper()
{
}

QString QnMediaResourceHelper::serverName() const
{
    Q_D(const QnMediaResourceHelper);
    return d->server ? d->server->getName() : QString();
}

qreal QnMediaResourceHelper::customAspectRatio() const
{
    Q_D(const QnMediaResourceHelper);
    return d->camera ? d->camera->customAspectRatio() : 0.0;
}

int QnMediaResourceHelper::customRotation() const
{
    Q_D(const QnMediaResourceHelper);
    return d->camera ? d->camera->getProperty(QnMediaResource::rotationKey()).toInt() : 0;
}

int QnMediaResourceHelper::channelCount() const
{
    Q_D(const QnMediaResourceHelper);
    return d->camera ? d->camera->getVideoLayout()->channelCount() : 0;
}

QSize QnMediaResourceHelper::layoutSize() const
{
    Q_D(const QnMediaResourceHelper);
    return d->camera ? d->camera->getVideoLayout()->size() : QSize(1, 1);
}

QPoint QnMediaResourceHelper::channelPosition(int channel) const
{
    Q_D(const QnMediaResourceHelper);
    return d->camera ? d->camera->getVideoLayout()->position(channel) : QPoint();
}

QnMediaDewarpingParams QnMediaResourceHelper::fisheyeParams() const
{
    Q_D(const QnMediaResourceHelper);
    return (d->camera && kFisheyeDewarpingFeatureEnabled)
        ? d->camera->getDewarpingParams()
        : QnMediaDewarpingParams();
}

QnMediaResourceHelperPrivate::QnMediaResourceHelperPrivate(QnMediaResourceHelper* parent):
    q_ptr(parent)
{
}

void QnMediaResourceHelperPrivate::handlePropertyChanged(
    const QnResourcePtr& resource, const QString& key)
{
    Q_Q(QnMediaResourceHelper);

    if (camera != resource)
        return;

    if (key == QnMediaResource::customAspectRatioKey())
        emit q->customAspectRatioChanged();
    else if (key == QnMediaResource::rotationKey())
        emit q->customRotationChanged();
}

void QnMediaResourceHelperPrivate::updateServer()
{
    Q_Q(QnMediaResourceHelper);

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
                q, &QnMediaResourceHelper::serverNameChanged);
        }
    }

    emit q->serverNameChanged();
}

void QnMediaResourceHelperPrivate::handleResourceChanged()
{
    Q_Q(QnMediaResourceHelper);

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
        connect(camera, &QnResource::propertyChanged,
            this, &QnMediaResourceHelperPrivate::handlePropertyChanged);
        connect(camera, &QnResource::parentIdChanged,
            this, &QnMediaResourceHelperPrivate::updateServer);

        connect(camera, &QnResource::videoLayoutChanged,
            q, &QnMediaResourceHelper::videoLayoutChanged);
        connect(camera, &QnResource::mediaDewarpingParamsChanged,
            q, &QnMediaResourceHelper::fisheyeParamsChanged);

        updateServer();
    }

    emit q->customAspectRatioChanged();
    emit q->customRotationChanged();
    emit q->resourceStatusChanged();
    emit q->videoLayoutChanged();
    emit q->fisheyeParamsChanged();
}
