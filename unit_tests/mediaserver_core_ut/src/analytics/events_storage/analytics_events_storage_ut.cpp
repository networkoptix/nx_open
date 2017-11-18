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
        m_analyticsDataPacket = generateRandomPacket(1);
        whenIssueSavePacket(m_analyticsDataPacket);
        thenSaveSucceeded();
    }

    void whenIssueSavePacket(common::metadata::ConstDetectionMetadataPacketPtr packet)
    {
        m_eventsStorage->save(
            m_analyticsDataPacket,
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
                std::vector<common::metadata::DetectionMetadataPacket> eventsFound)
            {
                m_lookupResultQueue.push(LookupResult{resultCode, std::move(eventsFound)});
            });
    }

    void thenSaveSucceeded()
    {
        ASSERT_EQ(ResultCode::ok, m_savePacketResultQueue.pop());
    }

    void thenEventCanBeRead()
    {
        whenLookupEvents(Filter());
        thenLookupSucceded();
        andLookupResultEquals(*m_analyticsDataPacket);
    }

    void thenLookupSucceded()
    {
        m_prevLookupResult = m_lookupResultQueue.pop();
        ASSERT_EQ(ResultCode::ok, m_prevLookupResult->resultCode);
    }

    void andLookupResultEquals(
        const common::metadata::DetectionMetadataPacket& expected)
    {
        ASSERT_EQ(1U, m_prevLookupResult->eventsFound.size());
        ASSERT_EQ(expected, m_prevLookupResult->eventsFound[0]);
    }

private:
    struct LookupResult
    {
        ResultCode resultCode;
        std::vector<common::metadata::DetectionMetadataPacket> eventsFound;

        LookupResult() = delete;
    };

    std::unique_ptr<EventsStorage> m_eventsStorage;
    Settings m_settings;
    common::metadata::ConstDetectionMetadataPacketPtr m_analyticsDataPacket;
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
};

TEST_F(AnalyticsEventsStorage, event_saved_can_be_read_later)
{
    whenSaveEvent();
    whenRestartStorage();

    thenEventCanBeRead();
}

// TEST_F(AnalyticsEventsStorage, storing_multiple_events_concurrently)

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
