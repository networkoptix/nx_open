#include <memory>

#include <gtest/gtest.h>

#include <nx/network/aio/basic_pollable.h>
#include <nx/utils/random.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/thread/sync_queue.h>

#include <nx/vms/server/analytics/db/object_metadata_streamer.h>
#include <utils/common/util.h>

#include "attribute_dictionary.h"

namespace nx::analytics::db::test {

namespace {

class EventsStorageStub;

class Cursor:
    public AbstractCursor
{
public:
    Cursor(
        EventsStorageStub* eventsStorageStub,
        Filter filter);

    virtual common::metadata::ConstObjectMetadataPacketPtr next() override;

    virtual void close() override;

    const Filter& filter() const;

    nx::utils::SyncQueue<common::metadata::ConstObjectMetadataPacketPtr>& packetsProvidedQueue();
private:
    std::size_t m_nextPacketPosition = 0;
    EventsStorageStub* m_eventsStorageStub = nullptr;
    Filter m_filter;
    nx::utils::SyncQueue<common::metadata::ConstObjectMetadataPacketPtr> m_packetsProvidedQueue;
};

//-------------------------------------------------------------------------------------------------

static const std::chrono::microseconds kInitialTimestamp(1000);
static const std::chrono::microseconds kTimestampStep(10);

class EventsStorageStub final:
    public AbstractEventsStorage
{
    friend class Cursor;

public:
    EventsStorageStub(nx::utils::SyncQueue<Cursor*>* createdCursorFilters):
        m_createdCursors(createdCursorFilters)
    {
    }

    virtual ~EventsStorageStub()
    {
        m_asyncCaller.pleaseStopSync();
    }

    virtual bool initialize(const Settings& /*settings*/) override
    {
        return true;
    }

    virtual void save(common::metadata::ConstObjectMetadataPacketPtr packet) override
    {
        auto it = std::upper_bound(
            m_objectMetadataPackets.begin(),
            m_objectMetadataPackets.end(),
            packet,
            [](const common::metadata::ConstObjectMetadataPacketPtr& one,
                const common::metadata::ConstObjectMetadataPacketPtr& two)
            {
                return one->timestampUs < two->timestampUs;
            });

        m_objectMetadataPackets.insert(it, std::move(packet));
    }

    virtual void createLookupCursor(
        Filter filter,
        CreateCursorCompletionHandler completionHandler) override
    {
        m_asyncCaller.post(
            [this, filter, completionHandler = std::move(completionHandler)]()
            {
                auto cursor = std::make_unique<Cursor>(this, filter);
                m_createdCursors->push(cursor.get());
                completionHandler(ResultCode::ok, std::move(cursor));
            });
    }

    virtual void lookup(
        Filter /*filter*/,
        LookupCompletionHandler /*completionHandler*/) override
    {
        FAIL();
    }

    virtual void lookupTimePeriods(
        Filter /*filter*/,
        TimePeriodsLookupOptions /*options*/,
        TimePeriodsLookupCompletionHandler /*completionHandler*/) override
    {
        FAIL();
    }

    virtual void markDataAsDeprecated(
        QnUuid /*deviceId*/,
        std::chrono::milliseconds /*oldestNeededDataTimestamp*/) override
    {
        FAIL();
    }

    virtual void flush(StoreCompletionHandler /*completionHandler*/) override
    {
        FAIL();
    }

    const std::vector<common::metadata::ConstObjectMetadataPacketPtr>& packets() const
    {
        return m_objectMetadataPackets;
    }

    std::vector<common::metadata::ConstObjectMetadataPacketPtr>& packets()
    {
        return m_objectMetadataPackets;
    }

    common::metadata::ConstObjectMetadataPacketPtr getPacketByTimestamp(
        std::chrono::microseconds timestamp) const
    {
        auto it = std::upper_bound(
            m_objectMetadataPackets.begin(),
            m_objectMetadataPackets.end(),
            timestamp,
            [](std::chrono::microseconds one,
                const common::metadata::ConstObjectMetadataPacketPtr& two)
            {
                return one.count() < two->timestampUs;
            });
        if (it == m_objectMetadataPackets.begin())
            return nullptr;

        return *(--it);
    }

    void generateRandomPackets()
    {
        const int packetCount = nx::utils::random::number<int>(10, 20);
        const auto deviceId = QnUuid::createUuid();

        for (int i = 0; i < packetCount; ++i)
        {
            auto packet = generateRandomPacket(1);
            packet->deviceId = deviceId;
            packet->timestampUs = (kInitialTimestamp + i * kTimestampStep).count();
            m_objectMetadataPackets.push_back(packet);
        }
    }
    virtual bool readMinimumEventTimestamp(std::chrono::milliseconds* /*outResult*/) override
    {
        return false;
    }

    virtual bool initialized() const override { return true; }

private:
    nx::utils::SyncQueue<Cursor*>* m_createdCursors;
    nx::network::aio::BasicPollable m_asyncCaller;
    /**
     * Sorted by increasing timestamp.
     */
    std::vector<common::metadata::ConstObjectMetadataPacketPtr> m_objectMetadataPackets;
};

//-------------------------------------------------------------------------------------------------

Cursor::Cursor(
    EventsStorageStub* eventsStorageStub,
    Filter filter)
    :
    m_eventsStorageStub(eventsStorageStub),
    m_filter(filter)
{
}

common::metadata::ConstObjectMetadataPacketPtr Cursor::next()
{
    if (m_nextPacketPosition >= m_eventsStorageStub->m_objectMetadataPackets.size())
        return nullptr;
    auto resultPacket = m_eventsStorageStub->m_objectMetadataPackets[m_nextPacketPosition++];
    m_packetsProvidedQueue.push(resultPacket);
    return resultPacket;
}

void Cursor::close()
{
}

const Filter& Cursor::filter() const
{
    return m_filter;
}

nx::utils::SyncQueue<common::metadata::ConstObjectMetadataPacketPtr>&
    Cursor::packetsProvidedQueue()
{
    return m_packetsProvidedQueue;
}

} // namespace

//-------------------------------------------------------------------------------------------------

class ObjectMetadataStreamer:
    public ::testing::Test
{
public:
    ObjectMetadataStreamer():
        m_deviceId(QnUuid::createUuid()),
        m_eventsStorageStub(&m_createdCursors)
    {
        m_streamer = std::make_unique<db::ObjectMetadataStreamer>(
            &m_eventsStorageStub,
            m_deviceId);

        m_eventsStorageStub.generateRandomPackets();
    }

protected:
    void setFirstRequestTimestamp(std::chrono::microseconds initialTimestamp)
    {
        m_initialRequestTimestamp = initialTimestamp;
    }

    void setRequestTimestampStep(std::chrono::microseconds timestampStep)
    {
        m_requestTimestampStep = timestampStep;
    }

    void whenRequestPacket()
    {
        readPacket(std::chrono::milliseconds(
            nx::utils::random::number<qint64>(1000, 2000)));
    }

    void whenRequestPacketUsingTimestampSmallerThenNextPacketAvailable()
    {
        readPacket(std::chrono::microseconds(
            m_eventsStorageStub.packets()[0]->timestampUs - 1));
    }

    void whenReadPacketsInALoopUsingNextPacketTimestamp()
    {
        for (const auto& nextExpectedPacket: m_eventsStorageStub.packets())
        {
            auto packet = m_streamer->getMotionData(nextExpectedPacket->timestampUs);
            if (!packet)
                break;
            m_packetsRead.push(packet);
        }
    }

    void whenRequestMultiplePacketsUsingTimestampsWithGap(
        std::chrono::milliseconds timestampGap)
    {
        std::chrono::microseconds timestamp(m_eventsStorageStub.packets()[0]->timestampUs);
        readPacket(timestamp);

        timestamp += timestampGap;
        readPacket(timestamp);
    }

    void whenRequestPacketUsingSomeTimestamp()
    {
        readPacket(std::chrono::microseconds(
            m_eventsStorageStub.packets()[m_eventsStorageStub.packets().size() / 2]->timestampUs));
    }

    void whenRequestPacketUsingMaxTimestamp()
    {
        readPacket(std::chrono::microseconds::max());
    }

    void whenReadPacketsInALoop()
    {
        for (auto timestamp = m_initialRequestTimestamp;; timestamp += m_requestTimestampStep)
        {
            auto expectedPacket =
                nx::common::metadata::toCompressedMetadataPacket(
                    *m_eventsStorageStub.getPacketByTimestamp(timestamp));
            if (m_expectedPackets.empty() ||
                expectedPacket->timestamp != m_expectedPackets.back()->timestamp)
            {
                m_expectedPackets.push_back(expectedPacket);
            }

            auto packet = m_streamer->getMotionData(timestamp.count());
            if (!packet)
                break;
            m_packetsRead.push(packet);
        }
    }

    void thenCursorIsCreated()
    {
        m_prevCursor = m_createdCursors.pop();
    }

    void andCursorFilterIsCorrect()
    {
        using namespace std::chrono;

        ASSERT_FALSE(m_prevCursor->filter().timePeriod.isNull());
        ASSERT_EQ(
            duration_cast<milliseconds>(m_lastRequestedTimestamp),
            m_prevCursor->filter().timePeriod.startTime());

        ASSERT_EQ(std::vector<QnUuid>{m_deviceId}, m_prevCursor->filter().deviceIds);
    }

    void thenPacketIsReadFromCursor()
    {
        if (!m_prevCursor)
            m_prevCursor = m_createdCursors.pop();
        ASSERT_NE(nullptr, m_prevCursor->packetsProvidedQueue().pop());
    }

    void thenEmptyPacketIsReturned()
    {
        m_prevPacketRead = m_packetsRead.pop();
        ASSERT_EQ(nullptr, m_prevPacketRead);
    }

    void thenAllPacketsAreRead()
    {
        std::vector<QnAbstractCompressedMetadataPtr> packetsRead;
        packetsRead.resize(m_packetsRead.size());
        std::generate(
            packetsRead.begin(), packetsRead.end(),
            [this]() { return m_packetsRead.pop(); });

        assertEqual(m_eventsStorageStub.packets(), packetsRead);
    }

    void thenCursorIsRecreatedUsingNewTimestamp()
    {
        m_createdCursors.pop(); //< Initial cursor.

        m_prevCursor = m_createdCursors.pop(); //< Re-created cursor
        andCursorFilterIsCorrect();
    }

    void thenOnlyNearestPacketIsProvided()
    {
        std::vector<QnAbstractCompressedMetadataPtr> packetsRead;
        while (!m_packetsRead.empty())
            packetsRead.push_back(m_packetsRead.pop());

        std::vector<QnAbstractCompressedMetadataPtr> packetsExpected;
        auto packet = m_eventsStorageStub.getPacketByTimestamp(m_lastRequestedTimestamp);
        if (packet)
            packetsExpected.push_back(nx::common::metadata::toCompressedMetadataPacket(*packet));

        assertEqual(packetsExpected, packetsRead);
    }

    void thenExpectedPacketsRead()
    {
        std::vector<QnAbstractCompressedMetadataPtr> packetsRead;
        while (!m_packetsRead.empty())
            packetsRead.push_back(m_packetsRead.pop());

        assertEqual(m_expectedPackets, packetsRead);
    }

private:
    const QnUuid m_deviceId;
    nx::utils::SyncQueue<Cursor*> m_createdCursors;
    EventsStorageStub m_eventsStorageStub;
    std::unique_ptr<db::ObjectMetadataStreamer> m_streamer;
    std::chrono::microseconds m_lastRequestedTimestamp;
    nx::utils::SyncQueue<QnAbstractCompressedMetadataPtr> m_packetsRead;
    QnAbstractCompressedMetadataPtr m_prevPacketRead;
    Cursor* m_prevCursor = nullptr;
    std::chrono::microseconds m_initialRequestTimestamp;
    std::chrono::microseconds m_requestTimestampStep;
    std::vector<QnAbstractCompressedMetadataPtr> m_expectedPackets;

    void readPacket(std::chrono::microseconds timestamp)
    {
        m_lastRequestedTimestamp = timestamp;
        m_packetsRead.push(m_streamer->getMotionData(timestamp.count()));
    }

    void assertEqual(
        const std::vector<common::metadata::ConstObjectMetadataPacketPtr>& expected,
        const std::vector<QnAbstractCompressedMetadataPtr>& actual)
    {
        ASSERT_EQ(expected.size(), actual.size());
        for (std::size_t i = 0; i < expected.size(); ++i)
            assertEqual(*expected[i], *actual[i]);
    }

    void assertEqual(
        const common::metadata::ObjectMetadataPacket& expected,
        const QnAbstractCompressedMetadata& actual)
    {
        ASSERT_EQ(expected.timestampUs, actual.timestamp);
        ASSERT_EQ(expected.durationUs, actual.m_duration);
    }

    void assertEqual(
        const std::vector<QnAbstractCompressedMetadataPtr>& expected,
        const std::vector<QnAbstractCompressedMetadataPtr>& actual)
    {
        ASSERT_EQ(expected.size(), actual.size());
        for (std::size_t i = 0; i < expected.size(); ++i)
        {
            ASSERT_EQ(expected[i]->timestamp, actual[i]->timestamp);
            ASSERT_EQ(expected[i]->m_duration, actual[i]->m_duration);
            // TODO: deserialize objects and compare.
        }
    }
};

//-------------------------------------------------------------------------------------------------

TEST_F(ObjectMetadataStreamer, creates_cursor_with_proper_time_period_filter)
{
    whenRequestPacket();

    thenCursorIsCreated();
    andCursorFilterIsCorrect();
}

TEST_F(ObjectMetadataStreamer, returns_null_if_next_packet_timestamp_exceeds_requested_one)
{
    whenRequestPacketUsingTimestampSmallerThenNextPacketAvailable();

    thenPacketIsReadFromCursor();
    thenEmptyPacketIsReturned();
}

TEST_F(ObjectMetadataStreamer, reads_all_available_packets)
{
    whenReadPacketsInALoopUsingNextPacketTimestamp();
    thenAllPacketsAreRead();
}

TEST_F(ObjectMetadataStreamer, acquires_new_cursor_in_case_of_a_gap_in_timestamp)
{
    whenRequestMultiplePacketsUsingTimestampsWithGap(
        MAX_FRAME_DURATION + std::chrono::milliseconds(1));
    thenCursorIsRecreatedUsingNewTimestamp();
}

TEST_F(ObjectMetadataStreamer, acquires_new_cursor_if_timestamp_jumps_backward)
{
    whenRequestMultiplePacketsUsingTimestampsWithGap(
        std::chrono::milliseconds(-100));
    thenCursorIsRecreatedUsingNewTimestamp();
}

TEST_F(ObjectMetadataStreamer, returns_closest_packet_to_the_requested_time_skipping_some_if_needed)
{
    whenRequestPacketUsingSomeTimestamp();
    thenOnlyNearestPacketIsProvided();
}

TEST_F(ObjectMetadataStreamer, returns_only_last_packet_if_needed)
{
    whenRequestPacketUsingMaxTimestamp();
    thenOnlyNearestPacketIsProvided();
}

TEST_F(ObjectMetadataStreamer, skips_packets_when_timestamp_skips_every_second_packet)
{
    setFirstRequestTimestamp(kInitialTimestamp);
    setRequestTimestampStep(kTimestampStep * 2);

    whenReadPacketsInALoop();
    thenExpectedPacketsRead();
}

//-------------------------------------------------------------------------------------------------
// Error handling.

// TEST_F(ObjectMetadataStreamer, removes_cursor_in_case_of_eof_or_error)
// TEST_F(ObjectMetadataStreamer, cursor_recreated_after_failure_not_too_often)

} // namespace nx::analytics::db::test
