// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <list>
#include <vector>

#include <QtCore/QHash>
#include <QtCore/QRectF>

#include <analytics/common/object_metadata.h>
#include <nx/utils/uuid.h>

namespace nx::vms::client::core::analytics {

/** State of an analytics object track at the current playback position. */
struct NX_VMS_CLIENT_CORE_API ObjectSnapshot
{
    nx::Uuid trackId;
    nx::Uuid analyticsEngineId;

    /** Object data from the latest track packet at or before the current position. */
    nx::common::metadata::ObjectMetadata metadata;

    std::chrono::milliseconds startTimestamp{0};
    std::chrono::milliseconds endTimestamp{0};

    /**
     * Bounding box from the next track packet, used to smooth movement between packets. If there
     * is no next packet, the timestamp equals startTimestamp.
     */
    QRectF futureRectangle;
    std::chrono::milliseconds futureRectangleTimestamp{0};

    /**
     * Bounding box linearly interpolated between the current and the next track packets. It is
     * invalid if the current one is invalid and there is no next packet.
     */
    QRectF interpolatedRectangle(std::chrono::milliseconds timestamp) const;
};

/**
 * Determines which analytics objects are visible at the current playback position and where,
 * given the object metadata packets around that position. Objects stay visible at least
 * nx::common::metadata::kMinimalObjectDuration after their last packet.
 */
class NX_VMS_CLIENT_CORE_API ObjectTracker
{
public:
    /** Metadata should be requested this far into the past and the future of the position. */
    static constexpr std::chrono::milliseconds kMetadataWindowSize = std::chrono::seconds(2);

    struct UpdateResult
    {
        std::vector<nx::Uuid> updated;
        std::vector<nx::Uuid> removed;
    };

    /**
     * Updates the objects for the given position.
     * @param packets Packets within kMetadataWindowSize of the timestamp, sorted by time.
     */
    UpdateResult update(const std::list<nx::common::metadata::ObjectMetadataPacketPtr>& packets,
        std::chrono::milliseconds timestamp);

    const QHash<nx::Uuid, ObjectSnapshot>& objects() const;

    void clear();

private:
    QHash<nx::Uuid, ObjectSnapshot> m_objects;
};

} // namespace nx::vms::client::core::analytics
