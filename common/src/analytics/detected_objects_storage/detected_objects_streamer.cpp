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
    NX_VERBOSE(this, lm("Requested timestamp %1").args(timeUsec / 1000));

    reinitializeCursorIfTimeDiscontinuityPresent(timeUsec);

    if (!readPacketForTimestamp(timeUsec))
        return nullptr;

    if (m_packetCache.front()->timestampUsec > timeUsec)
    {
        NX_VERBOSE(this, lm("Delaying packet with timestamp %1 vs requested %2")
            .args(m_packetCache.front()->timestampUsec / 1000, timeUsec / 1000));
        return nullptr;
    }

    NX_VERBOSE(this, lm("Returning packet with timestamp %1 vs %2 requested. Difference %3 ms")
        .args(m_packetCache.front()->timestampUsec / 1000, timeUsec / 1000,
            (timeUsec - m_packetCache.front()->timestampUsec) / 1000));

    auto resultPacket = std::move(m_packetCache.front());
    m_packetCache.pop_front();
    return nx::common::metadata::toMetadataPacket(*resultPacket);
}

void DetectedObjectsStreamer::reinitializeCursorIfTimeDiscontinuityPresent(qint64 timeUsec)
{
    using namespace std::chrono;

    if (m_prevRequestedTimestamp != -1)
    {
        const bool timeJumpedForward =
            timeUsec > m_prevRequestedTimestamp &&
            microseconds(timeUsec - m_prevRequestedTimestamp) > MAX_FRAME_DURATION;
        const bool timeJumpedBackward = timeUsec < m_prevRequestedTimestamp;

        if (timeJumpedForward || timeJumpedBackward)
        {
            NX_VERBOSE(this, lm("Detected %1 usec gap in timestamps requested. Re-creating cursor")
                .args(timeUsec - m_prevRequestedTimestamp));
            m_packetCache.clear();
            m_cursor.reset();
        }
    }

    m_prevRequestedTimestamp = timeUsec;
}

bool DetectedObjectsStreamer::readPacketForTimestamp(qint64 timeUsec)
{
    using namespace std::chrono;

    if (!m_cursor)
        createCursor(duration_cast<milliseconds>(microseconds(timeUsec)));
    if (!m_cursor)
        return false;

    for (;;)
    {
        if (!m_packetCache.empty() && m_packetCache.back()->timestampUsec > timeUsec)
            break;

        auto detectionPacket = m_cursor->next();
        if (detectionPacket)
        {
            NX_VERBOSE(this, lm("Read packet with timestamp %1")
                .args(detectionPacket->timestampUsec / 1000));
            m_packetCache.push_back(detectionPacket);
        }

        if (m_packetCache.empty())
        {
            NX_VERBOSE(this, lm("Closing cursor. Device %1, timestamp %2")
                .args(m_deviceId, timeUsec / 1000));
            m_cursor.reset();
            return false;
        }

        for (auto it = m_packetCache.begin(); it != m_packetCache.end();)
        {
            auto nextIt = std::next(it);
            if (nextIt == m_packetCache.end())
                break;
            if ((*nextIt)->timestampUsec > timeUsec)
                break;
            NX_VERBOSE(this, lm("Skipping packet with timestamp %1")
                .args((*it)->timestampUsec / 1000));
            it = m_packetCache.erase(it);
        }

        // Treating NULL packet as the one with the largest timestamp.
        if (detectionPacket == nullptr)
            break;
    }

    return true;
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
