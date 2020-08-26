#include "object_track_best_shot_proxy.h"

#include <nx/utils/std/algorithm.h>
#include <nx/utils/time/abstract_time_provider.h>
#include <nx/vms/server/archive/abstract_iframe_search_helper.h>
#include <utils/math/math.h>

using namespace std::chrono;
using namespace nx::common::metadata;

static const seconds kTimerInterval{2};

static ObjectMetadataPacketPtr makeBestShotPacket(
    ObjectMetadata objectMetadata,
    const ObjectMetadataPacketPtr& objectMetadataPacket)
{
    if (objectMetadata.objectMetadataType == ObjectMetadataType::regular)
        objectMetadata.objectMetadataType = ObjectMetadataType::bestShot;

    ObjectMetadataPacketPtr bestShot = std::make_shared<ObjectMetadataPacket>();
    bestShot->deviceId = objectMetadataPacket->deviceId;
    bestShot->durationUs = objectMetadataPacket->durationUs;
    bestShot->timestampUs = objectMetadataPacket->timestampUs;
    bestShot->streamIndex = objectMetadataPacket->streamIndex;
    bestShot->objectMetadataList.push_back(std::move(objectMetadata));

    return bestShot;
}

namespace nx::vms::server::analytics {

ObjectTrackBestShotProxy::ObjectTrackBestShotProxy(
    archive::AbstractIframeSearchHelper* iframeSearchHelper,
    BestShotHandler bestShotHandler,
    milliseconds autoBestShotDelay,
    milliseconds trackExpirationDelay)
    :
    m_iframeSearchHelper(iframeSearchHelper),
    m_bestShotHandler(std::move(bestShotHandler)),
    m_autoBestShotDelay(autoBestShotDelay),
    m_trackExpirationDelay(trackExpirationDelay)
{
    scheduleTrackCleanup();
}

ObjectTrackBestShotProxy::~ObjectTrackBestShotProxy()
{
    stop();
}

void ObjectTrackBestShotProxy::scheduleTrackCleanup()
{
    m_timer.start(kTimerInterval,
        [this]()
        {
            NX_MUTEX_LOCKER lock(&m_mutex);
            if (m_stopped)
                return;

            assignRaughBestShots();
            cleanUpOldTracks();
            scheduleTrackCleanup();
        });
}

void ObjectTrackBestShotProxy::processObjectMetadataPacket(
    const ObjectMetadataPacketPtr& objectMetadataPacket)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    if (m_stopped)
        return;

    for (const ObjectMetadata& objectMetadata: objectMetadataPacket->objectMetadataList)
        assignBestShotFromPacket(objectMetadata, objectMetadataPacket);
}

void ObjectTrackBestShotProxy::stop()
{
    m_stopped = true;
    std::promise<void> promise;
    m_timer.post(
        [this, &promise]()
        {
            m_timer.pleaseStopSync();
            promise.set_value();
        });
    promise.get_future().wait();
    NX_MUTEX_LOCKER lock(&m_mutex);
    cleanUpOldTracks();
}

void ObjectTrackBestShotProxy::assignBestShotFromPacket(
    const ObjectMetadata& objectMetadata,
    const ObjectMetadataPacketPtr& packet)
{
    auto& track = m_trackContexts[objectMetadata.trackId];

    track.timeoutSinceLastFrame.restart();

    if (!track.bestShot)
    {
        track.bestShot = makeBestShotPacket(objectMetadata, packet);
        track.timeoutSinceFirstFrame.restart();
    }

    if (objectMetadata.isBestShot())
    {
        track.bestShotType = BestShotType::precise;
        pushBestShot(packet);
    }
    else if (track.bestShotType != BestShotType::precise)
    {
        const qint64 iframeTimeUs = m_iframeSearchHelper->findAfter(
            packet->deviceId,
            packet->streamIndex,
            packet->timestampUs);

        if (qBetween(track.bestShot->timestampUs, iframeTimeUs, packet->timestampUs + 1))
        {
            track.bestShotType = BestShotType::precise;
            track.bestShot->timestampUs = iframeTimeUs;
            pushBestShot(track.bestShot);
        }
    }
}

void ObjectTrackBestShotProxy::pushBestShot(ObjectMetadataPacketPtr objectMetadataPacket)
{
    m_bestShotHandler(objectMetadataPacket);
}

void ObjectTrackBestShotProxy::assignRaughBestShots()
{
    for (auto it = m_trackContexts.begin(); it != m_trackContexts.end(); ++it)
    {
        TrackContext& trackContext = it->second;
        const bool hasExpired =
            trackContext.timeoutSinceFirstFrame.hasExpired(m_autoBestShotDelay);

        if (hasExpired && trackContext.bestShotType == BestShotType::none)
        {
            trackContext.bestShotType = BestShotType::rough;
            pushBestShot(trackContext.bestShot);
        }
    }
}

void ObjectTrackBestShotProxy::cleanUpOldTracks()
{
    for (auto it = m_trackContexts.begin(); it != m_trackContexts.end();)
    {
        TrackContext& trackContext = it->second;
        const bool hasExpired =
            trackContext.timeoutSinceLastFrame.hasExpired(m_trackExpirationDelay);

        if (hasExpired)
            it = m_trackContexts.erase(it);
        else
            ++it;
    }
}

} // namespace nx::vms::server::analytics
