#include "object_track_best_shot_resolver.h"

#include <chrono>

#include <nx/network/http/http_async_client.h>

namespace nx::vms::server::analytics {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

using Image = nx::analytics::db::Image;
using AsyncClient = nx::network::http::AsyncClient;
using ObjectMetadataType = nx::common::metadata::ObjectMetadataType;

ObjectTrackBestShotResolver::~ObjectTrackBestShotResolver()
{
    stop();
}

void ObjectTrackBestShotResolver::resolve(
    const Ptr<IObjectTrackBestShotPacket1>& bestShotPacket,
    ImageHandler imageHandler)
{
    if (!NX_ASSERT(bestShotPacket))
        notifyImageResolved(std::nullopt, std::move(imageHandler), ObjectMetadataType::undefined);

    Image image;
    image.imageDataFormat = bestShotPacket->imageDataFormat();
    image.imageData = QByteArray(bestShotPacket->imageData(), bestShotPacket->imageDataSize());

    if (!image.imageDataFormat.isEmpty() && !image.imageData.isEmpty())
    {
        notifyImageResolved(
            std::move(image),
            std::move(imageHandler),
            ObjectMetadataType::externalBestShot);

        return;
    }

    nx::utils::Url imageUrl(bestShotPacket->imageUrl());
    if (!imageUrl.isValid())
    {
        const nx::sdk::analytics::Rect boundingBox = bestShotPacket->boundingBox();
        const int64_t timestampUs = bestShotPacket->timestampUs();

        const ObjectMetadataType bestShotType = (boundingBox.isValid() && timestampUs > 0)
            ? ObjectMetadataType::bestShot
            : ObjectMetadataType::undefined;

        notifyImageResolved(std::nullopt, std::move(imageHandler), bestShotType);
        return;
    }

    initiateImageRequest(imageUrl, std::move(imageHandler));
}

void ObjectTrackBestShotResolver::initiateImageRequest(
    nx::utils::Url imageUrl,
    ImageHandler imageHandler)
{
    m_timer.post(
        [this, imageUrl = std::move(imageUrl), imageHandler = std::move(imageHandler)]()
        {
            initiateImageRequestInternal(std::move(imageUrl), std::move(imageHandler));
        });
}

void ObjectTrackBestShotResolver::initiateImageRequestInternal(
    nx::utils::Url imageUrl, ImageHandler imageHandler)
{
    const RequestId requestId = ++m_maxRequestId;
    const auto emplacementResult = m_requestContexts.emplace(
        requestId, RequestContext{ makeHttpClient(), std::move(imageHandler) });

    if (!NX_ASSERT(emplacementResult.second))
        return;

    const auto& context = emplacementResult.first->second;

    context.httpClient->doGet(
        imageUrl,
        [this, requestId]()
        {
            ImageResult imageResult = handleResponse(requestId);
            notifyImageResolved(
                std::move(imageResult.image),
                std::move(imageResult.imageHandler),
                ObjectMetadataType::externalBestShot);

            m_requestContexts.erase(requestId);
        });
}

std::unique_ptr<AsyncClient> ObjectTrackBestShotResolver::makeHttpClient() const
{
    auto httpClient = std::make_unique<AsyncClient>();
    httpClient->bindToAioThread(m_timer.getAioThread());

    return httpClient;
}

ObjectTrackBestShotResolver::ImageResult ObjectTrackBestShotResolver::handleResponse(
    RequestId requestId)
{
    const auto& contextItr = m_requestContexts.find(requestId);
    if (!NX_ASSERT(contextItr != m_requestContexts.cend()))
        return {};

    RequestContext& context = contextItr->second;
    ImageResult result;
    result.imageHandler = std::move(context.imageHandler);

    const auto* response = context.httpClient->response();
    if (!response || response->statusLine.statusCode != nx::network::http::StatusCode::ok)
        return result;

    Image image;
    image.imageData = context.httpClient->fetchMessageBodyBuffer();
    image.imageDataFormat = getHeaderValue(response->headers, "Content-Type");
    result.image = std::move(image);

    return result;
}

void ObjectTrackBestShotResolver::notifyImageResolved(
    std::optional<nx::analytics::db::Image> image,
    ImageHandler imageHandler,
    ObjectMetadataType bestShotType)
{
    if (!NX_ASSERT(imageHandler))
        return;

    m_timer.post(
        [image = std::move(image), imageHandler = std::move(imageHandler), bestShotType]()
        {
            imageHandler(std::move(image), bestShotType);
        });
}

void ObjectTrackBestShotResolver::stop()
{
    nx::utils::promise<void> promise;
    m_timer.post(
        [this, &promise]()
        {
            for (auto& [_, requestContext]: m_requestContexts)
                requestContext.httpClient->pleaseStopSync();

            m_timer.pleaseStopSync();
            promise.set_value();
        });

    promise.get_future().wait();
}

} // namespace nx::vms::server::analytics
