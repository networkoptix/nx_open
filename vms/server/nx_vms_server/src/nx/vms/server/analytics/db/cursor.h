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
    Cursor(std::unique_ptr<nx::sql::Cursor<ObjectTrack>> sqlCursor);
    virtual ~Cursor();

    virtual common::metadata::ConstObjectMetadataPacketPtr next() override;
    virtual void close() override;

    void setOnBeforeCursorDestroyed(
        nx::utils::MoveOnlyFunc<void(Cursor*)> handler);

private:
    using Tracks =
        std::vector<std::pair<ObjectTrack, std::size_t /*current track position*/>>;

    std::unique_ptr<nx::sql::Cursor<ObjectTrack>> m_sqlCursor;
    bool m_eof = false;
    Tracks m_currentTracks;
    common::metadata::ObjectMetadataPacketPtr m_packet;
    QnMutex m_mutex;
    nx::utils::MoveOnlyFunc<void(Cursor*)> m_onBeforeCursorDestroyedHandler;

    void loadObjectsIfNecessary();
    void loadNextTrack();

    qint64 nextTrackPositionTimestamp();
    std::tuple<Tracks::iterator /*track*/, qint64 /*track position timestamp*/> findTrackPosition();
    qint64 maxObjectTrackStartTimestamp();

    std::tuple<const ObjectTrack*, std::size_t /*track position*/> readNextTrackPosition();

    common::metadata::ObjectMetadataPacketPtr createMetaDataPacket(
        const ObjectTrack& track,
        int trackPositionIndex);

    nx::common::metadata::ObjectMetadata toMetadataObject(
        const ObjectTrack& track,
        int trackPositionIndex);

    bool canAddToTheCurrentPacket(
        const ObjectTrack& track,
        int trackPositionIndex) const;

    void addToCurrentPacket(
        const ObjectTrack& track,
        int trackPositionIndex);
};

} // namespace nx::analytics::db
