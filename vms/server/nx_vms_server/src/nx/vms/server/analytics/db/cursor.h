#pragma once

#include <nx/sql/sql_cursor.h>
#include <nx/utils/thread/mutex.h>

#include <analytics/db/abstract_cursor.h>
#include <analytics/db/analytics_db_types.h>

namespace nx::analytics::db {

/**
 * Selects track from the new DB scheme.
 */
class Cursor:
    public AbstractCursor
{
public:
    Cursor(std::unique_ptr<nx::sql::Cursor<DetectedObject>> sqlCursor);
    virtual ~Cursor();

    virtual common::metadata::ConstDetectionMetadataPacketPtr next() override;
    virtual void close() override;

    void setOnBeforeCursorDestroyed(
        nx::utils::MoveOnlyFunc<void(Cursor*)> handler);

private:
    using Objects =
        std::vector<std::pair<DetectedObject, std::size_t /*current track position*/>>;

    std::unique_ptr<nx::sql::Cursor<DetectedObject>> m_sqlCursor;
    bool m_eof = false;
    Objects m_currentObjects;
    common::metadata::DetectionMetadataPacketPtr m_packet;
    QnMutex m_mutex;
    nx::utils::MoveOnlyFunc<void(Cursor*)> m_onBeforeCursorDestroyedHandler;

    void loadObjectsIfNecessary();
    void loadNextObject();

    qint64 nextTrackPositionTimestamp();
    Objects::iterator findTrackPosition();
    qint64 maxObjectTrackStartTimestamp();

    std::tuple<const DetectedObject*, std::size_t /*track position*/> readNextTrackPosition();

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

} // namespace nx::analytics::db
