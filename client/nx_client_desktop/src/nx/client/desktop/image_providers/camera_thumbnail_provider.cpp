#include "camera_thumbnail_provider.h"

#include <api/server_rest_connection.h>

#include <nx/client/desktop/image_providers/camera_thumbnail_manager.h>

#include <common/common_module.h>

#include <core/resource/media_server_resource.h>
#include <core/resource/camera_resource.h>

#include <nx/fusion/model_functions.h>

namespace nx {
namespace client {
namespace desktop {

CameraThumbnailProvider::CameraThumbnailProvider(
    const api::CameraImageRequest& request,
    QObject* parent)
    :
    base_type(parent),
    m_request(request),
    m_status(Qn::ThumbnailStatus::Invalid)
{
    NX_ASSERT(request.camera, Q_FUNC_INFO, "Camera must exist here");
    NX_ASSERT(commonModule()->currentServer(), Q_FUNC_INFO, "We must be connected here");

    if (!request.camera || !request.camera->hasVideo(nullptr))
    {
        setStatus(Qn::ThumbnailStatus::NoData);
        return;
    }

    /* Making connection through event loop to handle data from another thread. */
    connect(this, &CameraThumbnailProvider::imageDataLoadedInternal, this,
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

api::CameraImageRequest CameraThumbnailProvider::requestData() const
{
    return m_request;
}

void CameraThumbnailProvider::setRequestData(const api::CameraImageRequest& data)
{
    m_request = data;
}

QImage CameraThumbnailProvider::image() const
 {
    return m_image;
}

QSize CameraThumbnailProvider::sizeHint() const
{
    if (!m_image.isNull())
        return m_image.size();

    return QnCameraThumbnailManager::sizeHintForCamera(m_request.camera, m_request.size);
}

Qn::ThumbnailStatus CameraThumbnailProvider::status() const
{
    return m_status;
}

void CameraThumbnailProvider::doLoadAsync()
{
    if (m_status == Qn::ThumbnailStatus::Loaded || m_status == Qn::ThumbnailStatus::Loading)
        return;

    setStatus(Qn::ThumbnailStatus::Loading);

    if (!commonModule()->currentServer())
    {
        setStatus(Qn::ThumbnailStatus::NoData);
        emit imageDataLoadedInternal(QByteArray());
        return;
    }

    QPointer<CameraThumbnailProvider> guard(this);
    auto handle = commonModule()->currentServer()->restConnection()->cameraThumbnailAsync(
        m_request,
        [this, guard] (bool success, rest::Handle /*id*/, const QByteArray& imageData)
        {
            if (!guard)
                return;

            setStatus(success ? Qn::ThumbnailStatus::Loaded : Qn::ThumbnailStatus::NoData);
            if (success)
                emit imageDataLoadedInternal(imageData);
        });

    if (handle <= 0)
    {
        setStatus(Qn::ThumbnailStatus::NoData);
        emit imageDataLoadedInternal(QByteArray());
    }
}

void CameraThumbnailProvider::setStatus(Qn::ThumbnailStatus status)
{
    if (status == m_status)
        return;

    m_status = status;
    emit statusChanged(status);
}

} // namespace desktop
} // namespace client
} // namespace nx

