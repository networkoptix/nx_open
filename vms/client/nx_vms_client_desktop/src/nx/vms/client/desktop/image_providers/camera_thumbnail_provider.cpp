#include "camera_thumbnail_provider.h"

#include <api/server_rest_connection.h>
#include <client/client_module.h>
#include <common/common_module.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/camera_resource.h>

#include <nx/fusion/model_functions.h>
#include <nx/network/http/custom_headers.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/desktop/image_providers/camera_thumbnail_manager.h>
#include <nx/vms/client/desktop/utils/video_cache.h>

namespace nx::vms::client::desktop {

using namespace std::chrono;

namespace {

// Allowed timestamp error when pre-looking an existing frame in the global VideoCache.
static constexpr microseconds kAllowedTimeDeviation = 35ms;

} // namespace

CameraThumbnailProvider::CameraThumbnailProvider(
    const nx::api::CameraImageRequest& request,
    QObject* parent)
    :
    base_type(parent),
    m_request(request),
    m_status(Qn::ThumbnailStatus::Invalid)
{
    NX_ASSERT(request.camera, "Camera must exist here");
    NX_ASSERT(commonModule()->currentServer(), "We must be connected here");

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
                NX_VERBOSE(this, "imageDataLoadedInternal(%1) - got response with data",
                    m_request.camera->getName());
                const auto imageFormat = QnLexical::serialized(m_request.imageFormat).toUtf8();
                m_image.loadFromData(data, imageFormat);
            }
            else if (nextStatus != Qn::ThumbnailStatus::NoData)
            {
                // We should not be here
                NX_VERBOSE(this, "imageDataLoadedInternal(%1) - empty data but status not NoData!",
                    m_request.camera->getName());
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

bool CameraThumbnailProvider::tryLoad()
{
    if (m_status == Qn::ThumbnailStatus::Loading)
        return false;

    if (!m_request.camera)
    {
        setStatus(Qn::ThumbnailStatus::NoData);
        return true;
    }

    auto cache = qnClientModule->videoCache();
    if (!cache || nx::api::CameraImageRequest::isSpecialTimeValue(m_request.usecSinceEpoch))
        return false;

    std::chrono::microseconds requiredTime(m_request.usecSinceEpoch);
    std::chrono::microseconds actualTime{};

    const auto image = cache->image(m_request.camera->getId(), requiredTime, &actualTime);
    if (image.isNull() || abs(requiredTime - actualTime) > kAllowedTimeDeviation)
        return false;

    setStatus(Qn::ThumbnailStatus::Loading);

    NX_VERBOSE(this, "doLoadAsync(%1) - got image from the cache, error is %2 us",
        m_request.camera->getName(), requiredTime - actualTime);

    m_image = image;
    m_timestampUs = actualTime.count();

    emit imageChanged(m_image);
    emit sizeHintChanged(sizeHint());

    setStatus(Qn::ThumbnailStatus::Loaded);
    return true;
}

void CameraThumbnailProvider::doLoadAsync()
{
    if (m_status == Qn::ThumbnailStatus::Loading)
        return;

    if (!m_request.camera)
    {
        setStatus(Qn::ThumbnailStatus::NoData);
        return;
    }

    if (!commonModule()->currentServer())
    {
        NX_VERBOSE(this, "doLoadAsync(%1) - no server is available. Returning early",
            m_request.camera->getName());
        emit imageDataLoadedInternal(QByteArray(), Qn::ThumbnailStatus::NoData, 0);
        return;
    }

    setStatus(Qn::ThumbnailStatus::Loading);

    QPointer<CameraThumbnailProvider> guard(this);
    NX_VERBOSE(this, "doLoadAsync(%1) - sending request to the server",
        m_request.camera->getName());

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

