// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_thumbnail_provider.h"

#include <api/server_rest_connection.h>
#include <core/resource/camera_resource.h>
#include <nx/fusion/model_functions.h>
#include <nx/network/http/custom_headers.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/client/core/utils/video_cache.h>
#include <nx/vms/common/pixelation/pixelation_settings.h>

#include "camera_thumbnail_manager.h"

namespace nx::vms::client::core {

using namespace std::chrono;

namespace {

// Allowed timestamp error when pre-looking an existing frame in the global VideoCache.
static constexpr microseconds kAllowedTimeDeviation = 150ms;

} // namespace

const QString CameraThumbnailProvider::kFrameTimestampKey = "FrameTimestampUs";
const QString CameraThumbnailProvider::kFrameFromPluginKey = "FrameFromPlugin";

CameraThumbnailProvider::CameraThumbnailProvider(
    const nx::api::CameraImageRequest& request,
    QObject* parent)
    :
    base_type(parent),
    m_request(request),
    m_status(ThumbnailStatus::Invalid)
{
    if (!NX_ASSERT(request.camera, "Camera must exist here") || !request.camera->hasVideo())
    {
        // We will rise an event there. But client could not expect it before doLoadAsync call
        setStatus(ThumbnailStatus::NoData);
        return;
    }

    /* Making connection through event loop to handle data from another thread. */
    connect(this, &CameraThumbnailProvider::imageDataLoadedInternal, this,
        [this](const QByteArray &data, ThumbnailStatus nextStatus, qint64 timestampUs,
            bool frameFromPlugin)
        {
            if (!data.isEmpty())
            {
                NX_VERBOSE(this, "imageDataLoadedInternal(%1) - got response with data",
                    m_request.camera->getName());
                const auto imageFormat =
                    QByteArray::fromStdString(nx::reflect::toString(m_request.format));
                m_image.loadFromData(data, imageFormat);
            }
            else if (nextStatus != ThumbnailStatus::NoData)
            {
                // We should not be here
                NX_VERBOSE(this, "imageDataLoadedInternal(%1) - empty data but status not NoData!",
                    m_request.camera->getName());
            }

            m_timestampUs = timestampUs;

            m_image.setText(kFrameTimestampKey, QString::number(timestampUs));
            m_image.setText(kFrameFromPluginKey, frameFromPlugin ? "1" : "0");

            emit imageChanged(m_image);
            emit sizeHintChanged(sizeHint());

            setStatus(nextStatus);
        }, Qt::QueuedConnection);
}

nx::api::CameraImageRequest CameraThumbnailProvider::requestData() const
{
    return m_request;
}

void CameraThumbnailProvider::setRequestData(
    const nx::api::CameraImageRequest& data, bool resetStatus)
{
    m_request = data;

    if (resetStatus)
        setStatus(ThumbnailStatus::Invalid);
}

QImage CameraThumbnailProvider::image() const
 {
    return m_image;
}

QSize CameraThumbnailProvider::sizeHint() const
{
    return CameraThumbnailManager::sizeHintForCamera(m_request.camera, (QSize&) m_request.size);
}

ThumbnailStatus CameraThumbnailProvider::status() const
{
    return m_status;
}

bool CameraThumbnailProvider::tryLoad()
{
    if (m_status == ThumbnailStatus::Loading)
        return false;

    if (!m_request.camera)
    {
        setStatus(ThumbnailStatus::NoData);
        return true;
    }

    auto systemContext = SystemContext::fromResource(m_request.camera);
    auto cache = systemContext->videoCache();
    if (!cache || nx::api::CameraImageRequest::isSpecialTimeValue(m_request.timestampMs)
        || systemContext->pixelationSettings()->isAllTypes() || !systemContext->pixelationSettings()->objectTypeIds().empty())
    {
        return false;
    }

    std::chrono::microseconds requiredTime(m_request.timestampMs);
    std::chrono::microseconds actualTime{};

    const auto image = cache->image(m_request.camera->getId(), requiredTime, &actualTime);
    if (image.isNull() || abs(requiredTime - actualTime) > kAllowedTimeDeviation)
    {
        if (image.isNull())
        {
            NX_VERBOSE(this, "tryLoad(%1) - got no image from the cache",
                m_request.camera->getName());
        }
        else
        {
            NX_VERBOSE(this, "tryLoad(%1) - got image from the cache, deviation is too big: %2 ms",
                m_request.camera->getName(), (requiredTime - actualTime).count() / 1000);
        }

        return false;
    }

    setStatus(ThumbnailStatus::Loading);

    NX_VERBOSE(this, "tryLoad(%1) - got image from the cache, deviation is %2 ms",
        m_request.camera->getName(), (requiredTime - actualTime).count() / 1000);

    const int rotation = m_request.rotation < 0
        ? m_request.camera->forcedRotation().value_or(0)
        : m_request.rotation;

    m_image = image.transformed(QTransform().rotate(rotation));
    m_timestampUs = actualTime.count();
    m_requestId = 0;

    emit imageChanged(m_image);
    emit sizeHintChanged(sizeHint());

    setStatus(ThumbnailStatus::Loaded);
    return true;
}

void CameraThumbnailProvider::doLoadAsync()
{
    if (m_status == ThumbnailStatus::Loading)
        return;

    if (!m_request.camera)
    {
        setStatus(ThumbnailStatus::NoData);
        return;
    }

    const bool isLiveRequest =
        nx::api::CameraImageRequest::isSpecialTimeValue(m_request.timestampMs);

    if (isLiveRequest && !m_request.camera->isOnline())
    {
        NX_VERBOSE(this,
            "%1 (%2) - camera is offline or unauthorized. Returning early", Q_FUNC_INFO,
            m_request.camera->getName());
        emit imageDataLoadedInternal(QByteArray(), ThumbnailStatus::NoData, 0, false);
        return;
    }

    auto systemContext = SystemContext::fromResource(m_request.camera);
    if (!NX_ASSERT(systemContext))
    {
        NX_VERBOSE(this, "doLoadAsync(%1) - camera is not in a system context.",
            m_request.camera->getName());
        emit imageDataLoadedInternal(QByteArray(), ThumbnailStatus::NoData, 0, false);
        return;
    }

    auto api = systemContext->connectedServerApi();
    if (!api)
    {
        NX_VERBOSE(this, "doLoadAsync(%1) - no server is available. Returning early",
            m_request.camera->getName());
        emit imageDataLoadedInternal(QByteArray(), ThumbnailStatus::NoData, 0, false);
        return;
    }

    setStatus(ThumbnailStatus::Loading);

    QPointer<CameraThumbnailProvider> guard(this);
    NX_VERBOSE(this, "doLoadAsync(%1) - sending request to the server",
        m_request.camera->getName());

    const auto callback = nx::utils::guarded(this,
        [this](
            bool success,
            rest::Handle requestId,
            QByteArray imageData,
            const nx::network::http::HttpHeaders& headers)
        {
            if (requestId != m_requestId)
                return;

            ThumbnailStatus nextStatus =
                success ? ThumbnailStatus::Loaded : ThumbnailStatus::NoData;

            qint64 timestampUs = 0;
            bool frameFromPlugin = false;

            // TODO: add error message processing.
            const bool isJsonError = nx::network::http::getHeaderValue(headers, "Content-Type")
                == Qn::serializationFormatToHttpContentType(Qn::SerializationFormat::json);

            if (imageData.isEmpty() || isJsonError)
            {
                nextStatus = ThumbnailStatus::NoData;
            }
            else
            {
                timestampUs = nx::utils::stoll(nx::network::http::getHeaderValue(
                    headers, Qn::FRAME_TIMESTAMP_US_HEADER_NAME));

                frameFromPlugin = QnLexical::deserialized<bool>(nx::network::http::getHeaderValue(
                    headers, Qn::FRAME_ENCODED_BY_PLUGIN));
            }

            m_requestId = 0;
            emit imageDataLoadedInternal(imageData, nextStatus, timestampUs, frameFromPlugin);
        });

    m_requestId = api->cameraThumbnailAsync(m_request, callback, thread());
    if (m_requestId <= 0)
    {
        // It will change to status NoData as well
        emit imageDataLoadedInternal(
            /*data*/ QByteArray(),
            ThumbnailStatus::NoData,
            /*timestampUs*/ 0,
            /*frameFromPlugin*/ false);
    }
}

void CameraThumbnailProvider::setStatus(ThumbnailStatus status)
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

} // namespace nx::vms::client::core
