// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "object_tracker.h"

#include <algorithm>
#include <optional>

#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>
#include <utils/math/linear_combination.h>

namespace nx::vms::client::core::analytics {

using namespace std::chrono;
using namespace nx::common::metadata;

namespace {

milliseconds toMs(qint64 timestampUs)
{
    return duration_cast<milliseconds>(microseconds(timestampUs));
}

bool isValidRectangle(const QRectF& rect)
{
    // Rectangle can be used as a point, so zero width and height are OK.
    return rect.width() >= 0 && rect.height() >= 0;
}

/**
 * Path element inside Track. When constructing this structure make sure that the provided
 * ObjectMetadata always exists for the whole lifetime of the Track.
 */
struct TrackElement
{
    TrackElement(const ObjectMetadata* metadata, milliseconds timestamp):
        metadata(metadata),
        timestamp(timestamp)
    {
    }

    const ObjectMetadata* metadata;
    milliseconds timestamp{0};

    bool operator<(const TrackElement& other) const { return timestamp < other.timestamp; }

    bool operator<(const milliseconds& other) const { return timestamp < other; }
};

/**
 * Track describes behavior of the object sequence, united by the track id. Path is never empty!
 * Path is sorted by the timestamps. It is used to smooth rectangle movement through the video
 * frames.
 */
struct Track
{
    nx::Uuid analyticsEngineId;
    std::optional<milliseconds> minimalDuration;
    std::vector<TrackElement> path; //< Cannot be empty.

    milliseconds startTimestamp() const { return path.front().timestamp; }

    /**
     * The object is shown for the duration of the latest packet that has one, counted from the
     * start of the path (the current packet once the track is trimmed), and in any case for
     * kMinimalObjectDuration after the last packet. The end is exclusive.
     */
    milliseconds endTimestamp() const
    {
        const auto approximatedTimestamp = path.back().timestamp + kMinimalObjectDuration;
        return minimalDuration
            ? std::max(startTimestamp() + *minimalDuration, approximatedTimestamp)
            : approximatedTimestamp;
    }

    /**
     * Trim track by start time, leaving the only one path element, located before or on the
     * selected time.
     */
    void trimByStartTime(milliseconds startTime)
    {
        auto iter = std::lower_bound(path.begin(), path.end(), startTime);
        if (iter == path.end() || (iter != path.begin() && iter->timestamp > startTime))
            --iter;
        path.erase(path.begin(), iter);
        NX_ASSERT(!path.empty());
    }
};

/**
 * Each returned Track contains raw pointers inside the packets, so the packets must outlive the
 * tracks.
 */
std::vector<Track> fetchTracks(
    const std::list<ObjectMetadataPacketPtr>& objectMetadataPackets, milliseconds timestamp)
{
    // Left only objects which are to be displayed right now, store tracks for others.
    QHash<nx::Uuid, Track> objectTrackByTrackId;
    for (const auto& objectPacket: objectMetadataPackets)
    {
        const auto packetTimestamp = toMs(objectPacket->timestampUs);
        const bool packetHasDuration = objectPacket->durationUs > 0;

        // Each track will start from the actual object and be sorted by timestamps.
        for (const auto& objectMetadata: objectPacket->objectMetadataList)
        {
            Track& track = objectTrackByTrackId[objectMetadata.trackId];
            track.analyticsEngineId = objectPacket->analyticsEngineId;
            if (packetHasDuration)
                track.minimalDuration = toMs(objectPacket->durationUs);

            track.path.emplace_back(&objectMetadata, packetTimestamp);
            NX_ASSERT_HEAVY_CONDITION(std::is_sorted(track.path.cbegin(), track.path.cend()));
        }
    }

    std::vector<Track> result;
    for (auto track: objectTrackByTrackId)
    {
        // Skip tracks which end before or at the current frame.
        if (track.endTimestamp() <= timestamp)
            continue;

        track.trimByStartTime(timestamp);

        if (track.path.cbegin()->timestamp <= timestamp)
            result.push_back(track);
    }
    return result;
}

} // namespace

QRectF ObjectSnapshot::interpolatedRectangle(milliseconds timestamp) const
{
    const QRectF& rectangle = metadata.boundingBox;
    const bool hasFutureRectangle = futureRectangleTimestamp > startTimestamp;

    if (!isValidRectangle(rectangle))
        return hasFutureRectangle ? futureRectangle : rectangle;

    if (!hasFutureRectangle || !isValidRectangle(futureRectangle))
        return rectangle;

    const auto factor = (qreal) (timestamp - startTimestamp).count()
        / (qreal) (futureRectangleTimestamp - startTimestamp).count();

    return linearCombine(1 - factor, rectangle, factor, futureRectangle);
}

ObjectTracker::UpdateResult ObjectTracker::update(
    const std::list<ObjectMetadataPacketPtr>& packets, milliseconds timestamp)
{
    UpdateResult result;

    for (const auto& track: fetchTracks(packets, timestamp))
    {
        const ObjectMetadata& metadata = *track.path[0].metadata;

        auto& object = m_objects[metadata.trackId];
        object.trackId = metadata.trackId;
        object.analyticsEngineId = track.analyticsEngineId;
        object.metadata = metadata;
        object.startTimestamp = track.startTimestamp();
        object.endTimestamp = track.endTimestamp();

        // Make forecast for the rectangle movement to smooth it between analytics packets.
        if (track.path.size() > 1)
        {
            const auto& nextElement = track.path[1];
            object.futureRectangleTimestamp = nextElement.timestamp;
            object.futureRectangle = nextElement.metadata->boundingBox;
        }
        else
        {
            object.futureRectangleTimestamp = object.startTimestamp;
            object.futureRectangle = QRectF();
        }

        result.updated.push_back(object.trackId);
    }

    // Remove objects which are not supposed to be displayed on the current frame.
    for (auto it = m_objects.begin(); it != m_objects.end(); /*-*/)
    {
        if (timestamp < it->startTimestamp || timestamp >= it->endTimestamp)
        {
            NX_VERBOSE(this, "Object %1 does not fit into timestamp window", it->trackId);
            result.removed.push_back(it->trackId);
            it = m_objects.erase(it);
        }
        else
        {
            ++it;
        }
    }

    return result;
}

const QHash<nx::Uuid, ObjectSnapshot>& ObjectTracker::objects() const
{
    return m_objects;
}

void ObjectTracker::clear()
{
    m_objects.clear();
}

} // namespace nx::vms::client::core::analytics
