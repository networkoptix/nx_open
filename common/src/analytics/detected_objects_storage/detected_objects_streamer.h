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
    std::unique_ptr<AbstractCursor> m_cursor;
    nx::utils::AsyncOperationGuard m_asyncOperationGuard;
    common::metadata::ConstDetectionMetadataPacketPtr m_lastPacketRead;
    qint64 m_prevPacketTimestamp = -1;

    ResultCode createCursor(std::chrono::milliseconds startTimestamp);
    void createCursorAsync(
        std::chrono::milliseconds startTimestamp,
        nx::utils::MoveOnlyFunc<void(ResultCode)> handler);
};

} // namespace storage
} // namespace analytics
} // namespace nx
