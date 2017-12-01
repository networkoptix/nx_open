#include "detected_objects_streamer.h"

#include <nx/fusion/model_functions.h>
#include <nx/utils/log/log.h>
#include <nx/utils/sync_call.h>

#include <utils/common/util.h>

namespace nx {
namespace analytics {
namespace storage {

static const auto kMaxStreamingDuration = std::chrono::hours(24) * 30 * 12 * 7;

DetectedObjectsStreamer::DetectedObjectsStreamer(
    AbstractEventsStorage* storage,
    const QnUuid& deviceId)
    :
    m_storage(storage),
    m_deviceId(deviceId)
{
}

DetectedObjectsStreamer::~DetectedObjectsStreamer()
{
    m_asyncOperationGuard->terminate();
}

QnAbstractCompressedMetadataPtr DetectedObjectsStreamer::getMotionData(qint64 timeUsec)
{
    using namespace std::chrono;

    if (m_prevPacketTimestamp != -1 &&
        timeUsec - m_prevPacketTimestamp > MAX_FRAME_DURATION)
    {
        NX_VERBOSE(this, lm("Detected %1 usec gap in timestamps requested. Re-creating cursor")
            .args(timeUsec - m_prevPacketTimestamp));
        m_lastPacketRead.reset();
        m_cursor.reset();
    }

    if (!m_lastPacketRead)
    {
        if (!m_cursor)
            createCursor(duration_cast<milliseconds>(microseconds(timeUsec)));

        if (!m_cursor)
            return nullptr;

        auto detectionPacket = m_cursor->next();
        if (!detectionPacket)
        {
            NX_VERBOSE(this, lm("Closing cursor. Device %1, timestamp %2")
                .args(m_deviceId, timeUsec));
            m_cursor.reset();
            return nullptr;
        }

        m_lastPacketRead.swap(detectionPacket);
    }

    if (m_lastPacketRead->timestampUsec > timeUsec)
        return nullptr;

    m_prevPacketTimestamp = m_lastPacketRead->timestampUsec;
    decltype(m_lastPacketRead) resultPacket;
    m_lastPacketRead.swap(resultPacket);
    return nx::common::metadata::toMetadataPacket(*resultPacket);
}

ResultCode DetectedObjectsStreamer::createCursor(
    std::chrono::milliseconds startTimestamp)
{
    using namespace std::placeholders;

    auto result = makeSyncCall<ResultCode>(
        std::bind(&DetectedObjectsStreamer::createCursorAsync, this, startTimestamp, _1));
    return std::get<0>(result);
}

void DetectedObjectsStreamer::createCursorAsync(
    std::chrono::milliseconds startTimestamp,
    nx::utils::MoveOnlyFunc<void(ResultCode)> handler)
{
    NX_VERBOSE(this, lm("Creating cursor. Device %1, startTimestamp %2")
        .args(m_deviceId, startTimestamp));

    Filter filter;
    filter.deviceId = m_deviceId;
    filter.sortOrder = Qt::SortOrder::AscendingOrder;
    filter.timePeriod.setStartTime(startTimestamp);
    filter.timePeriod.setDuration(kMaxStreamingDuration);
    m_storage->createLookupCursor(
        std::move(filter),
        [this, sharedGuard = m_asyncOperationGuard.sharedGuard(), handler = std::move(handler)](
            ResultCode resultCode,
            std::unique_ptr<AbstractCursor> cursor)
        {
            const auto lock = sharedGuard->lock();
            if (!lock)
                return;

            NX_VERBOSE(this, lm("Created cursor. Device %1, resultCode %2")
                .args(m_deviceId, QnLexical::serialized(resultCode)));
            m_cursor = std::move(cursor);
            handler(resultCode);
        });
}

} // namespace storage
} // namespace analytics
} // namespace nx
