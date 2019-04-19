#pragma once

#include <nx/utils/async_operation_guard.h>
#include <nx/utils/move_only_func.h>

#include <motion/abstract_motion_archive.h>

#include "analytics_events_storage.h"

namespace nx {
namespace analytics {
namespace storage {

class AbstractEventsStorage;

class DetectedObjectsStreamer:
    public QnAbstractMotionArchiveConnection
{
public:
    DetectedObjectsStreamer(
        AbstractEventsStorage* storage,
        const QnUuid& deviceId);
    ~DetectedObjectsStreamer();

    virtual QnAbstractCompressedMetadataPtr getMotionData(qint64 timeUsec) override;

private:
    AbstractEventsStorage* m_storage;
    const QnUuid m_deviceId;
    std::shared_ptr<AbstractCursor> m_cursor;
    nx::utils::AsyncOperationGuard m_asyncOperationGuard;
    // TODO: #ak Use cyclic array here.
    std::deque<common::metadata::ConstDetectionMetadataPacketPtr> m_packetCache;
    qint64 m_prevRequestedTimestamp = -1;

    void reinitializeCursorIfTimeDiscontinuityPresent(qint64 timeUsec);
    bool readPacketForTimestamp(qint64 timeUsec);
    ResultCode createCursor(std::chrono::milliseconds startTimestamp);
    void createCursorAsync(
        std::chrono::milliseconds startTimestamp,
        nx::utils::MoveOnlyFunc<void(ResultCode)> handler);
};

} // namespace storage
} // namespace analytics
} // namespace nx
