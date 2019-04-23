#pragma once

#include <nx/sql/sql_cursor.h>

#include "abstract_cursor.h"
#include "analytics_events_storage_types.h"

namespace nx::analytics::storage {

/**
 * Selects track from the new DB scheme.
 */
class Cursor:
    public AbstractCursor
{
public:
    Cursor(std::unique_ptr<nx::sql::Cursor<DetectedObject>> sqlCursor);

    virtual common::metadata::ConstDetectionMetadataPacketPtr next() override;
    virtual void close() override;

private:
    std::unique_ptr<nx::sql::Cursor<DetectedObject>> m_sqlCursor;
    bool m_eof = false;
    std::vector<std::pair<DetectedObject, std::size_t /*current track position*/>> m_currentObjects;
    common::metadata::DetectionMetadataPacketPtr m_packet;

    std::tuple<const DetectedObject*, std::size_t /*track position*/> readNextTrackPosition();

    void loadNextObject();

    common::metadata::DetectionMetadataPacketPtr createMetaDataPacket(
        const DetectedObject& object,
        int trackPositionIndex);

    nx::common::metadata::DetectedObject toMetadataObject(
        const DetectedObject& object,
        int trackPositionIndex);

    bool canAddToTheCurrentPacket(
        const DetectedObject& object,
        int trackPositionIndex) const;

    void addToCurrentPacket(
        const DetectedObject& object,
        int trackPositionIndex);
};

} // namespace nx::analytics::storage
