#include "camera_thumbnail_provider.h"

#include <api/server_rest_connection.h>

#include <nx/vms/client/desktop/image_providers/camera_thumbnail_manager.h>

#include <common/common_module.h>

#include <core/resource/media_server_resource.h>
#include <core/resource/camera_resource.h>

#include <nx/fusion/model_functions.h>
#include <nx/network/http/custom_headers.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/log.h>

namespace nx::vms::client::desktop {

CameraThumbnailProvider::CameraThumbnailProvider(
    const nx::api::CameraImageRequest& request,
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
        // We will rise an event there. But client could not expect it before doLoadAsync call
        setStatus(Qn::ThumbnailStatus::NoData);
        return;
    }

    /* Making connection through event loop to handle data from another thread. */
    connect(this, &CameraThumbnailProvider::imageDataLoadedInternal, this,
        [this](const QByteArray &data, Qn::ThumbnailStatus nextStatus, qint64 timestampUs)
        {
            if (!data.isEmpty())
            {
                NX_VERBOSE(this) << "CameraThumbnailProvider::imageDataLoadedInternal(" << m_request.camera->getName() << ") - got response with data";
                const auto imageFormat = QnLexical::serialized(m_request.imageFormat).toUtf8();
                m_image.loadFromData(data, imageFormat);
            }
            else if (nextStatus != Qn::ThumbnailStatus::NoData)
            {
                // We should not be here
                NX_VERBOSE(this) << "CameraThumbnailProvider::imageDataLoadedInternal(" << m_request.camera->getName() << ") - empty data but status not NoData!";
            }

            m_timestampUs = timestampUs;

            emit imageChanged(m_image);
            emit sizeHintChanged(sizeHint());

            setStatus(nextStatus);
        }, Qt::QueuedConnection);
}

nx::api::CameraImageRequest CameraThumbnailProvider::requestData() const
{
    return m_request;
}

void CameraThumbnailProvider::setRequestData(const nx::api::CameraImageRequest& data)
{
    m_request = data;
}

QImage CameraThumbnailProvider::image() const
 {
    return m_image;
}

QSize CameraThumbnailProvider::sizeHint() const
{
    return CameraThumbnailManager::sizeHintForCamera(m_request.camera, m_request.size);
}

Qn::ThumbnailStatus CameraThumbnailProvider::status() const
{
    return m_status;
}

void CameraThumbnailProvider::doLoadAsync()
{
    if (m_status == Qn::ThumbnailStatus::Loading)
        return;

    setStatus(Qn::ThumbnailStatus::Loading);

    if (!commonModule()->currentServer())
    {
        NX_VERBOSE(this) << "CameraThumbnailProvider::doLoadAsync(" << m_request.camera->getName() << ") - no server is available. Returning early";
        emit imageDataLoadedInternal(QByteArray(), Qn::ThumbnailStatus::NoData, 0);
        return;
    }

    QPointer<CameraThumbnailProvider> guard(this);
    NX_VERBOSE(this) << "CameraThumbnailProvider::doLoadAsync(" << m_request.camera->getName() << ") - sending request to the server";

    const auto callback = nx::utils::guarded(this,
        [this](
            bool success,
            rest::Handle /*requestId*/,
            QByteArray imageData,
            const nx::network::http::HttpHeaders& headers)
        {

            Qn::ThumbnailStatus nextStatus =
                success ? Qn::ThumbnailStatus::Loaded : Qn::ThumbnailStatus::NoData;

            qint64 timestampUs = 0;

            if (imageData.isEmpty())
            {
                nextStatus = Qn::ThumbnailStatus::NoData;
            }
            else
            {
                timestampUs = nx::network::http::getHeaderValue(
                    headers, Qn::FRAME_TIMESTAMP_US_HEADER_NAME).toLongLong();
            }

            emit imageDataLoadedInternal(imageData, nextStatus, timestampUs);
        });

    auto handle = commonModule()->currentServer()->restConnection()->cameraThumbnailAsync(
        m_request, callback, thread());

    if (handle <= 0)
    {
        // It will change to status NoData as well
        emit imageDataLoadedInternal(QByteArray(), Qn::ThumbnailStatus::NoData, 0);
    }
}

void CameraThumbnailProvider::setStatus(Qn::ThumbnailStatus status)
{
    if (status == m_status)
        return;

    m_status = status;
    emit statusChanged(status);
}

qint64 CameraThumbnailProvider::timestampUs() const
{
    return m_timestampUs;
}

} // namespace nx::vms::client::desktop

