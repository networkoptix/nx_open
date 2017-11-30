#pragma once

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

    virtual QnAbstractCompressedMetadataPtr getMotionData(qint64 timeUsec) override;

private:
    AbstractEventsStorage* m_storage;
    const QnUuid m_deviceId;
};

} // namespace storage
} // namespace analytics
} // namespace nx
