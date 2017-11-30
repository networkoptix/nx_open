#include "detected_objects_streamer.h"

namespace nx {
namespace analytics {
namespace storage {

DetectedObjectsStreamer::DetectedObjectsStreamer(
    AbstractEventsStorage* storage,
    const QnUuid& deviceId)
    :
    m_storage(storage),
    m_deviceId(deviceId)
{
}

QnAbstractCompressedMetadataPtr DetectedObjectsStreamer::getMotionData(qint64 /*timeUsec*/)
{
    // TODO
    return nullptr;
}

} // namespace storage
} // namespace analytics
} // namespace nx
