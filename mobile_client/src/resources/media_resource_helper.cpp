#include "media_resource_helper.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <nx/fusion/model_functions.h>

namespace {

QSize sizeFromResolutionString(const QString& resolution)
{
    int pos = resolution.indexOf(QLatin1Char('x'), 0, Qt::CaseInsensitive);
    if (pos == -1)
        return QSize();

    return QSize(resolution.left(pos).toInt(), resolution.mid(pos + 1).toInt());
}

} // anonymous namespace

class QnMediaResourceHelperPrivate : public QObject
{
    QnMediaResourceHelper* q_ptr;
    Q_DECLARE_PUBLIC(QnMediaResourceHelper)

public:
    QnVirtualCameraResourcePtr camera;
    qreal aspectRatio;

    QnMediaResourceHelperPrivate(QnMediaResourceHelper* parent);

    void at_propertyChanged(const QnResourcePtr& resource, const QString& key);
    void updateAspectRatio();
};

QnMediaResourceHelper::QnMediaResourceHelper(QObject* parent) :
    base_type(parent),
    d_ptr(new QnMediaResourceHelperPrivate(this))
{
    connect(this, &QnMediaResourceHelper::aspectRatioChanged,
            this, &QnMediaResourceHelper::rotatedAspectRatioChanged);
    connect(this, &QnMediaResourceHelper::rotationChanged,
            this, &QnMediaResourceHelper::rotatedAspectRatioChanged);
}

QnMediaResourceHelper::~QnMediaResourceHelper()
{
}

QString QnMediaResourceHelper::resourceId() const
{
    Q_D(const QnMediaResourceHelper);
    return d->camera ? d->camera->getId().toString()
                     : QString();
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
    d->aspectRatio = 0.0;

    if (!d->camera)
        return;

    connect(d->camera,  &QnResource::nameChanged,
            this,       &QnMediaResourceHelper::resourceNameChanged);
    connect(d->camera,  &QnResource::statusChanged,
            this,       &QnMediaResourceHelper::resourceStatusChanged);
    connect(d->camera,  &QnResource::propertyChanged,
            d,          &QnMediaResourceHelperPrivate::at_propertyChanged);

    emit resourceIdChanged();
    emit resourceNameChanged();
    emit rotationChanged();
    emit resourceStatusChanged();
    d->updateAspectRatio();
}

Qn::ResourceStatus QnMediaResourceHelper::resourceStatus() const
{
    Q_D(const QnMediaResourceHelper);
    return d->camera ? d->camera->getStatus()
                     : Qn::NotDefined;
}

QString QnMediaResourceHelper::resourceName() const
{
    Q_D(const QnMediaResourceHelper);
    return d->camera ? d->camera->getName()
                     : QString();
}

qreal QnMediaResourceHelper::aspectRatio() const
{
    Q_D(const QnMediaResourceHelper);
    auto aspectRatio = d->aspectRatio;
    if (qFuzzyIsNull(aspectRatio))
        return 0;

    QSize layoutSize = d->camera->getVideoLayout()->size();
    if (!layoutSize.isEmpty())
        aspectRatio *= static_cast<qreal>(layoutSize.width()) / layoutSize.height();

    return aspectRatio;
}

qreal QnMediaResourceHelper::rotatedAspectRatio() const
{
    qreal ar = aspectRatio();
    if (qFuzzyIsNull(ar))
        return ar;

    int rot = rotation();
    if (rot % 90 == 0 && rot % 180 != 0)
        return 1.0 / ar;

    return ar;
}

int QnMediaResourceHelper::rotation() const
{
    Q_D(const QnMediaResourceHelper);
    return d->camera ? d->camera->getProperty(QnMediaResource::rotationKey()).toInt()
                     : 0;
}

QnMediaResourceHelperPrivate::QnMediaResourceHelperPrivate(QnMediaResourceHelper* parent) :
    q_ptr(parent)
{
}

void QnMediaResourceHelperPrivate::at_propertyChanged(const QnResourcePtr& resource, const QString& key)
{
    Q_Q(QnMediaResourceHelper);

    NX_ASSERT(camera == resource);
    if (camera != resource)
        return;

    if (key == Qn::CAMERA_MEDIA_STREAM_LIST_PARAM_NAME)
        updateAspectRatio();
    else if (key == QnMediaResource::rotationKey())
        emit q->rotationChanged();
}

void QnMediaResourceHelperPrivate::updateAspectRatio()
{
    if (!camera)
        return;

    const auto streamsString = camera->getProperty(Qn::CAMERA_MEDIA_STREAM_LIST_PARAM_NAME).toLatin1();
    auto supportedStreams = QJson::deserialized<CameraMediaStreams>(streamsString);

    for (const auto& info: supportedStreams.streams)
    {
        if (info.resolution != CameraMediaStreamInfo::anyResolution)
        {
            const auto resolution = sizeFromResolutionString(info.resolution);
            const auto ar = static_cast<qreal>(resolution.width()) / resolution.height();
            if (ar != aspectRatio)
            {
                Q_Q(QnMediaResourceHelper);
                aspectRatio = ar;
                emit q->aspectRatioChanged();
            }
            break;
        }
    }
}
