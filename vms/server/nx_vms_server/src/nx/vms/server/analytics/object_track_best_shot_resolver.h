#pragma once

#include <functional>
#include <optional>

#include <analytics/common/object_metadata.h>
#include <analytics/db/analytics_db_types.h>

#include <nx/utils/url.h>
#include <nx/network/aio/timer.h>
#include <nx/sdk/analytics/i_object_track_best_shot_packet.h>

namespace nx::network::http { class AsyncClient; }

namespace nx::vms::server::analytics {

class ObjectTrackBestShotResolver
{

public:
    using ImageHandler = std::function<void(
        std::optional<nx::analytics::db::Image>,
        nx::common::metadata::ObjectMetadataType objectMetadataType)>;

public:
    ObjectTrackBestShotResolver() = default;

    virtual ~ObjectTrackBestShotResolver();

    void resolve(
        const nx::sdk::Ptr<nx::sdk::analytics::IObjectTrackBestShotPacket>& bestShotPacket,
        ImageHandler imageHandler);

    void stop();

private:
    using RequestId = int64_t;

private:
    void initiateImageRequest(nx::utils::Url imageUrl, ImageHandler imageHandler);

    void initiateImageRequestInternal(nx::utils::Url imageUrl, ImageHandler imageHandler);

    std::unique_ptr<nx::network::http::AsyncClient> makeHttpClient() const;

    struct ImageResult
    {
        std::optional<nx::analytics::db::Image> image;
        ImageHandler imageHandler;
    };

    ImageResult handleResponse(RequestId requestId);

    void notifyImageResolved(
        std::optional<nx::analytics::db::Image> image,
        ImageHandler imageHandler,
        nx::common::metadata::ObjectMetadataType bestShotType);

private:
    struct RequestContext
    {
        std::unique_ptr<nx::network::http::AsyncClient> httpClient;
        ImageHandler imageHandler;
    };

    RequestId m_maxRequestId = 0;
    std::map<RequestId, RequestContext> m_requestContexts;
    nx::network::aio::Timer m_timer;
};

} // namespace nx::vms::server::analytics
