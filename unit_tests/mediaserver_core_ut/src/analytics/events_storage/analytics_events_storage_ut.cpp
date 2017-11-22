#include <memory>

#include <gtest/gtest.h>

#include <nx/utils/random.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/std/future.h>
#include <nx/utils/test_support/test_with_temporary_directory.h>
#include <nx/utils/thread/sync_queue.h>

#include <analytics/events_storage/analytics_events_storage.h>

namespace nx {
namespace mediaserver {
namespace analytics {
namespace storage {
namespace test {

class AnalyticsEventsStorage:
    public nx::utils::test::TestWithTemporaryDirectory,
    public ::testing::Test
{
public:
    AnalyticsEventsStorage():
        nx::utils::test::TestWithTemporaryDirectory("analytics_events_storage", QString())
    {
        m_settings.dbConnectionOptions.dbName =
            nx::utils::test::TestWithTemporaryDirectory::testDataDir() + "/events.sqlite";
        m_settings.dbConnectionOptions.driverType = nx::utils::db::RdbmsDriverType::sqlite;
        m_settings.dbConnectionOptions.maxConnectionCount = 17;
    }

protected:
    void whenSaveEvent()
    {
        m_analyticsDataPackets.push_back(generateRandomPacket(1));
        whenIssueSavePacket(m_analyticsDataPackets[0]);
        thenSaveSucceeded();
    }

    void whenSaveMultipleEventsConcurrently()
    {
        const int packetCount = 101;
        for (int i = 0; i < packetCount; ++i)
        {
            m_analyticsDataPackets.push_back(generateRandomPacket(1));
            whenIssueSavePacket(m_analyticsDataPackets.back());
        }

        for (int i = 0; i < packetCount; ++i)
            thenSaveSucceeded();
    }

    void whenIssueSavePacket(common::metadata::ConstDetectionMetadataPacketPtr packet)
    {
        m_eventsStorage->save(
            packet,
            [this](ResultCode resultCode) { m_savePacketResultQueue.push(resultCode); });
    }

    void whenRestartStorage()
    {
        m_eventsStorage.reset();
        ASSERT_TRUE(initializeStorage());
    }

    void whenLookupEvents(const Filter& filter)
    {
        m_eventsStorage->lookup(
            filter,
            [this](
                ResultCode resultCode,
                std::vector<DetectionEvent> eventsFound)
            {
                m_lookupResultQueue.push(LookupResult{resultCode, std::move(eventsFound)});
            });
    }

    void thenSaveSucceeded()
    {
        ASSERT_EQ(ResultCode::ok, m_savePacketResultQueue.pop());
    }

    void thenAllEventsCanBeRead()
    {
        whenLookupEvents(Filter());
        thenLookupSucceded();
        andLookupResultEquals(toDetectionEvents(m_analyticsDataPackets));
    }

    void thenLookupSucceded()
    {
        m_prevLookupResult = m_lookupResultQueue.pop();
        ASSERT_EQ(ResultCode::ok, m_prevLookupResult->resultCode);
    }

    void andLookupResultEquals(
        std::vector<DetectionEvent> expected)
    {
        auto sortCondition =
            [](const DetectionEvent& left, const DetectionEvent& right)
            {
                return std::tie(left.deviceId, left.timestampUsec) <
                    std::tie(right.deviceId, right.timestampUsec);
            };

        std::sort(expected.begin(), expected.end(), sortCondition);
        std::sort(
            m_prevLookupResult->eventsFound.begin(),
            m_prevLookupResult->eventsFound.end(),
            sortCondition);

        ASSERT_EQ(expected, m_prevLookupResult->eventsFound);
    }

private:
    struct LookupResult
    {
        ResultCode resultCode;
        std::vector<DetectionEvent> eventsFound;
    };

    std::unique_ptr<EventsStorage> m_eventsStorage;
    Settings m_settings;
    std::vector<common::metadata::ConstDetectionMetadataPacketPtr> m_analyticsDataPackets;
    nx::utils::SyncQueue<ResultCode> m_savePacketResultQueue;
    nx::utils::SyncQueue<LookupResult> m_lookupResultQueue;
    boost::optional<LookupResult> m_prevLookupResult;

    virtual void SetUp() override
    {
        ASSERT_TRUE(initializeStorage());
    }

    bool initializeStorage()
    {
        m_eventsStorage = std::make_unique<EventsStorage>(m_settings);
        return m_eventsStorage->initialize();
    }

    common::metadata::ConstDetectionMetadataPacketPtr generateRandomPacket(int eventCount)
    {
        auto packet = std::make_shared<common::metadata::DetectionMetadataPacket>();
        packet->deviceId = QnUuid::createUuid();
        packet->timestampUsec = nx::utils::random::number<qint64>();
        packet->durationUsec = nx::utils::random::number<qint64>(0, 30000);

        for (int i = 0; i < eventCount; ++i)
        {
            common::metadata::DetectedObject detectedObject;
            detectedObject.objectTypeId = QnUuid::createUuid();
            detectedObject.objectId = QnUuid::createUuid();
            detectedObject.boundingBox = QRectF(0, 0, 100, 100);
            detectedObject.labels.push_back(common::metadata::Attribute{
                nx::utils::random::generateName(7),
                nx::utils::random::generateName(7)});
            packet->objects.push_back(std::move(detectedObject));
        }

        return packet;
    }

    std::vector<DetectionEvent> toDetectionEvents(
        const std::vector<common::metadata::ConstDetectionMetadataPacketPtr>& analyticsDataPackets)
    {
        std::vector<DetectionEvent> result;
        for (const auto& packet: analyticsDataPackets)
        {
            for (const auto& object: packet->objects)
            {
                DetectionEvent event;
                event.deviceId = packet->deviceId;
                event.timestampUsec = packet->timestampUsec;
                event.durationUsec = packet->durationUsec;
                event.object = object;
                result.push_back(std::move(event));
            }
        }

        return result;
    }
};

TEST_F(AnalyticsEventsStorage, event_saved_can_be_read_later)
{
    whenSaveEvent();
    whenRestartStorage();

    thenAllEventsCanBeRead();
}

TEST_F(AnalyticsEventsStorage, storing_multiple_events_concurrently)
{
    whenSaveMultipleEventsConcurrently();
    thenAllEventsCanBeRead();
}

//-------------------------------------------------------------------------------------------------
// Lookup.

// TEST_F(AnalyticsEventsStorage, empty_filter_matches_all_events)
// TEST_F(AnalyticsEventsStorage, full_text_search)
// TEST_F(AnalyticsEventsStorage, matching_events_by_attribute_value)
// TEST_F(AnalyticsEventsStorage, matching_events_by_bounding_box)
// TEST_F(AnalyticsEventsStorage, matching_events_by_rectangle)
// TEST_F(AnalyticsEventsStorage, matching_events_by_objectId)
// TEST_F(AnalyticsEventsStorage, matching_events_by_objectTypeId)
// TEST_F(AnalyticsEventsStorage, lookup_stress_test)

//-------------------------------------------------------------------------------------------------
// Cursor.

// TEST_F(AnalyticsEventsStorage, cursor_provides_all_matched_data)
// TEST_F(AnalyticsEventsStorage, many_concurrent_cursors)

//-------------------------------------------------------------------------------------------------
// Deprecating data.

} // namespace test
} // namespace storage
} // namespace analytics
} // namespace mediaserver
} // namespace nx
