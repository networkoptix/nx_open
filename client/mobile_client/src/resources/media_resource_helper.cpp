#include "media_resource_helper.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <nx/fusion/model_functions.h>

class QnMediaResourceHelperPrivate : public QObject
{
    QnMediaResourceHelper* q_ptr;
    Q_DECLARE_PUBLIC(QnMediaResourceHelper)

public:
    QnVirtualCameraResourcePtr camera;

    QnMediaResourceHelperPrivate(QnMediaResourceHelper* parent);

    void at_propertyChanged(const QnResourcePtr& resource, const QString& key);
};

QnMediaResourceHelper::QnMediaResourceHelper(QObject* parent):
    base_type(parent),
    d_ptr(new QnMediaResourceHelperPrivate(this))
{
}

QnMediaResourceHelper::~QnMediaResourceHelper()
{
}

QString QnMediaResourceHelper::resourceId() const
{
    Q_D(const QnMediaResourceHelper);
    return d->camera ? d->camera->getId().toString() : QString();
}

void QnMediaResourceHelper::setResourceId(const QString& id)
{
    Q_D(QnMediaResourceHelper);

    auto camera = qnResPool->getResourceById<QnVirtualCameraResource>(QnUuid::fromStringSafe(id));
    if (camera == d->camera)
        return;

    if (d->camera)
        disconnect(d->camera, nullptr, this, nullptr);

    d->camera = camera;

    if (d->camera)
    {
        connect(d->camera, &QnResource::nameChanged,
            this, &QnMediaResourceHelper::resourceNameChanged);
        connect(d->camera, &QnResource::statusChanged,
            this, &QnMediaResourceHelper::resourceStatusChanged);
        connect(d->camera, &QnResource::propertyChanged,
            d, &QnMediaResourceHelperPrivate::at_propertyChanged);
        connect(d->camera, &QnResource::videoLayoutChanged,
            this, &QnMediaResourceHelper::videoLayoutChanged);
    }

    emit resourceIdChanged();
    emit resourceNameChanged();
    emit customAspectRatioChanged();
    emit customRotationChanged();
    emit resourceStatusChanged();
}

Qn::ResourceStatus QnMediaResourceHelper::resourceStatus() const
{
    Q_D(const QnMediaResourceHelper);
    return d->camera ? d->camera->getStatus() : Qn::NotDefined;
}

QString QnMediaResourceHelper::resourceName() const
{
    Q_D(const QnMediaResourceHelper);
    return d->camera ? d->camera->getName() : QString();
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
    return d->camera ? d->camera->getVideoLayout()->size() : QSize();
}

QPoint QnMediaResourceHelper::channelPosition(int channel) const
{
    Q_D(const QnMediaResourceHelper);
    return d->camera ? d->camera->getVideoLayout()->position(channel) : QPoint();
}

QnMediaResourceHelperPrivate::QnMediaResourceHelperPrivate(QnMediaResourceHelper* parent):
    q_ptr(parent)
{
}

void QnMediaResourceHelperPrivate::at_propertyChanged(
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
