#pragma once

#include <map>
#include <functional>
#include <chrono>

#include <nx/network/aio/timer.h>
#include <analytics/common/object_metadata.h>
#include <nx/utils/elapsed_timer.h>

namespace nx::vms::server::archive { class AbstractIframeSearchHelper; }

namespace nx::vms::server::analytics {

class ObjectTrackBestShotProxy
{
public:
    using BestShotHandler = std::function<void(nx::common::metadata::ObjectMetadataPacketPtr)>;

public:
    ObjectTrackBestShotProxy(
        archive::AbstractIframeSearchHelper* iframeSearchHelper,
        BestShotHandler bestShotHandler,
        std::chrono::milliseconds autoBestShotDelay,
        std::chrono::milliseconds trackExpirationDelay);

    virtual ~ObjectTrackBestShotProxy();

    void processObjectMetadataPacket(
        const nx::common::metadata::ObjectMetadataPacketPtr& objectMetadataPacket);

    void stop();

private:
    void assignBestShotFromPacket(
        const nx::common::metadata::ObjectMetadata& objectMetadata,
        const nx::common::metadata::ObjectMetadataPacketPtr& objectMetadataPacket);

    void pushBestShot(nx::common::metadata::ObjectMetadataPacketPtr objectMetadataPacket);

    void cleanUpOldTracks();
    void assignRaughBestShots();
    void scheduleTrackCleanup();

private:
    enum class BestShotType
    {
        none, /**< Best shot  is not generated yet. */
        rough, /**< Best shot is auto generated from the first frame. */
        precise, /**< Best shot is auto generated from the I-frame or explicitly provided. */
    };

    struct TrackContext
    {
        BestShotType bestShotType = BestShotType::none;
        nx::common::metadata::ObjectMetadataPacketPtr bestShot;
        nx::utils::ElapsedTimer timeoutSinceFirstFrame;
        nx::utils::ElapsedTimer timeoutSinceLastFrame;
    };

private:
    mutable nx::Mutex m_mutex;
    archive::AbstractIframeSearchHelper* const m_iframeSearchHelper = nullptr;
    BestShotHandler m_bestShotHandler;
    const std::chrono::milliseconds m_autoBestShotDelay{0};
    const std::chrono::milliseconds m_trackExpirationDelay{0};

    nx::network::aio::Timer m_timer;
    std::map<QnUuid, TrackContext> m_trackContexts;

    std::atomic<bool> m_stopped = false;
};

} // namespace nx::vms::server::analytics
