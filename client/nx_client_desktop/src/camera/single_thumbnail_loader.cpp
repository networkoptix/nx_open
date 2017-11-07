#include "single_thumbnail_loader.h"

#include <api/server_rest_connection.h>

#include <camera/camera_thumbnail_manager.h>

#include <common/common_module.h>

#include <core/resource/media_server_resource.h>
#include <core/resource/camera_resource.h>

#include <nx/fusion/model_functions.h>

namespace {

QnThumbnailRequestData makeRequestData(
    const QnVirtualCameraResourcePtr& camera,
    qint64 msecSinceEpoch,
    int rotation,
    const QSize& size,
    QnThumbnailRequestData::ThumbnailFormat format,
    QnThumbnailRequestData::AspectRatio aspectRatio,
    QnThumbnailRequestData::RoundMethod roundMethod)
{
    QnThumbnailRequestData data;
    data.camera = camera;
    data.msecSinceEpoch = msecSinceEpoch;
    data.rotation = rotation;
    data.size = size;
    data.imageFormat = format;
    data.format = Qn::SerializationFormat::UbjsonFormat;
    data.aspectRatio = aspectRatio;
    data.roundMethod = roundMethod;
    return data;
}

} // namespace

QnSingleThumbnailLoader::QnSingleThumbnailLoader(
    const QnVirtualCameraResourcePtr& camera,
    qint64 msecSinceEpoch,
    int rotation,
    const QSize& size,
    ThumbnailFormat format,
    AspectRatio aspectRatio,
    RoundMethod roundMethod,
    QObject* parent)
    :
    QnSingleThumbnailLoader(
        makeRequestData(camera, msecSinceEpoch, rotation, size, format, aspectRatio, roundMethod),
        parent)
{
    NX_ASSERT(camera, Q_FUNC_INFO, "Camera must exist here");
    NX_ASSERT(commonModule()->currentServer(), Q_FUNC_INFO, "We must be connected here");
}

QnSingleThumbnailLoader::QnSingleThumbnailLoader(
    const QnThumbnailRequestData& data,
    QObject* parent)
    :
    base_type(parent),
    m_request(data),
    m_status(Qn::ThumbnailStatus::Invalid)
{
    if (!data.camera || !data.camera->hasVideo(nullptr))
    {
        setStatus(Qn::ThumbnailStatus::NoData);
        return;
    }

    /* Making connection through event loop to handle data from another thread. */
    connect(this, &QnSingleThumbnailLoader::imageLoaded, this,
        [this](const QByteArray &data)
        {
            if (!data.isEmpty())
            {
                const auto imageFormat = QnLexical::serialized(m_request.imageFormat).toUtf8();
                m_image.loadFromData(data, imageFormat);
            }
            else
            {
                setStatus(Qn::ThumbnailStatus::NoData);
            }

            emit imageChanged(m_image);
            emit sizeHintChanged(sizeHint());
        }, Qt::QueuedConnection);
}

QnThumbnailRequestData QnSingleThumbnailLoader::requestData() const
{
    return m_request;
}

void QnSingleThumbnailLoader::setRequestData(const QnThumbnailRequestData& data)
{
    m_request = data;
}

QImage QnSingleThumbnailLoader::image() const
 {
    return m_image;
}

QSize QnSingleThumbnailLoader::sizeHint() const
{
    if (!m_image.isNull())
        return m_image.size();

    return QnCameraThumbnailManager::sizeHintForCamera(m_request.camera, m_request.size);
}

Qn::ThumbnailStatus QnSingleThumbnailLoader::status() const
{
    return m_status;
}

void QnSingleThumbnailLoader::doLoadAsync()
{
    if (m_status == Qn::ThumbnailStatus::Loaded || m_status == Qn::ThumbnailStatus::Loading)
        return;

    setStatus(Qn::ThumbnailStatus::Loading);

    if (!commonModule()->currentServer())
    {
        setStatus(Qn::ThumbnailStatus::NoData);
        emit imageLoaded(QByteArray());
        return;
    }

    QPointer<QnSingleThumbnailLoader> guard(this);
    auto handle = commonModule()->currentServer()->restConnection()->cameraThumbnailAsync(m_request,
        [this, guard] (bool success, rest::Handle id, const QByteArray &imageData)
        {
            if (!guard)
                return;

            Q_UNUSED(id);
            setStatus(success ? Qn::ThumbnailStatus::Loaded : Qn::ThumbnailStatus::NoData);
            if (!success)
                return;
            emit imageLoaded(imageData);
        });

    if (handle <= 0)
    {
        setStatus(Qn::ThumbnailStatus::NoData);
        emit imageLoaded(QByteArray());
    }
}

void QnSingleThumbnailLoader::setStatus(Qn::ThumbnailStatus status)
{
    if (status == m_status)
        return;

    m_status = status;
    emit statusChanged(status);
}


