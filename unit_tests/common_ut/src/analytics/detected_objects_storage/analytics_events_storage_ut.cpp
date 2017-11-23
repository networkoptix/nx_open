#include <memory>

#include <gtest/gtest.h>

#include <nx/utils/random.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/std/future.h>
#include <nx/utils/test_support/test_with_temporary_directory.h>
#include <nx/utils/thread/sync_queue.h>

#include <analytics/detected_objects_storage/analytics_events_storage.h>

namespace nx {
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
    virtual void SetUp() override
    {
        ASSERT_TRUE(initializeStorage());
    }

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
                std::vector<DetectedObject> eventsFound)
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
        andLookupResultEquals(toDetectedObjects(m_analyticsDataPackets));
    }

    void thenLookupSucceded()
    {
        m_prevLookupResult = m_lookupResultQueue.pop();
        ASSERT_EQ(ResultCode::ok, m_prevLookupResult->resultCode);
    }

    void andLookupResultEquals(
        std::vector<DetectedObject> expected)
    {
        auto sortCondition =
            [](const DetectedObject& left, const DetectedObject& right)
            {
                return left.objectId < right.objectId;
            };

        std::sort(expected.begin(), expected.end(), sortCondition);
        std::sort(
            m_prevLookupResult->eventsFound.begin(),
            m_prevLookupResult->eventsFound.end(),
            sortCondition);

        ASSERT_EQ(expected, m_prevLookupResult->eventsFound);
    }

    common::metadata::DetectionMetadataPacketPtr generateRandomPacket(int eventCount)
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

    std::vector<DetectedObject> toDetectedObjects(
        const std::vector<common::metadata::ConstDetectionMetadataPacketPtr>& analyticsDataPackets)
    {
        std::vector<DetectedObject> result;
        for (const auto& packet : analyticsDataPackets)
        {
            for (const auto& object : packet->objects)
            {
                DetectedObject detectedObject;
                detectedObject.objectId = object.objectId;
                detectedObject.objectTypeId = object.objectTypeId;
                detectedObject.attributes = object.labels;
                detectedObject.track.push_back(ObjectPosition());
                ObjectPosition& objectPosition = detectedObject.track.back();
                objectPosition.boundingBox = object.boundingBox;
                objectPosition.timestampUsec = packet->timestampUsec;
                objectPosition.durationUsec = packet->durationUsec;
                objectPosition.deviceId = packet->deviceId;

                result.push_back(std::move(detectedObject));
            }
        }

        return result;
    }

private:
    struct LookupResult
    {
        ResultCode resultCode;
        std::vector<DetectedObject> eventsFound;
    };

    std::unique_ptr<EventsStorage> m_eventsStorage;
    Settings m_settings;
    std::vector<common::metadata::ConstDetectionMetadataPacketPtr> m_analyticsDataPackets;
    nx::utils::SyncQueue<ResultCode> m_savePacketResultQueue;
    nx::utils::SyncQueue<LookupResult> m_lookupResultQueue;
    boost::optional<LookupResult> m_prevLookupResult;

    bool initializeStorage()
    {
        m_eventsStorage = std::make_unique<EventsStorage>(m_settings);
        return m_eventsStorage->initialize();
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
// Basic Lookup condition.

class AnalyticsEventsStorageLookup:
    public AnalyticsEventsStorage
{
    using base_type = AnalyticsEventsStorage;

public:
    AnalyticsEventsStorageLookup():
        m_timeRange(0, 0)
    {
    }

protected:
    virtual void SetUp() override
    {
        base_type::SetUp();

        generateVariousEvents();
    }

    void whenLookupByEmptyFilter()
    {
        m_filter = Filter();
        whenLookupEvents(m_filter);
    }

    void whenLookupByAnyDeviceId()
    {
        ASSERT_FALSE(m_allowedDeviceIds.empty());
        m_filter.deviceId = nx::utils::random::choice(m_allowedDeviceIds);
        whenLookupEvents(m_filter);
    }

    void thenResultMatchesExpectations()
    {
        thenLookupSucceded();
        andLookupResultEquals(toDetectedObjects(filterPackets(m_filter)));
    }

private:
    std::vector<QnUuid> m_allowedDeviceIds;
    std::pair<qint64, qint64> m_timeRange;
    std::vector<common::metadata::ConstDetectionMetadataPacketPtr> m_analyticsDataPackets;
    Filter m_filter;

    void generateVariousEvents()
    {
        std::vector<QnUuid> deviceIds;
        for (int i = 0; i < 3; ++i)
            deviceIds.push_back(QnUuid::createUuid());
        setAllowedDeviceIds(deviceIds);

        setAllowedTimeRange(100, 100000);

        generateEventsByCriteria();
    }

    void setAllowedDeviceIds(std::vector<QnUuid> deviceIds)
    {
        m_allowedDeviceIds = std::move(deviceIds);
    }

    void setAllowedTimeRange(qint64 start, qint64 end)
    {
        m_timeRange.first = start;
        m_timeRange.second = end;
    }

    void generateEventsByCriteria()
    {
        const int eventsPerDevice = 11;

        std::vector<common::metadata::ConstDetectionMetadataPacketPtr> analyticsDataPackets;
        for (const auto& deviceId: m_allowedDeviceIds)
        {
            for (int i = 0; i < eventsPerDevice; ++i)
            {
                auto packet = generateRandomPacket(1);
                packet->deviceId = deviceId;
                if (m_timeRange.first != 0 || m_timeRange.second != 0)
                {
                    packet->timestampUsec = nx::utils::random::number<qint64>(
                        m_timeRange.first, m_timeRange.second);
                }

                analyticsDataPackets.push_back(packet);
            }
        }

        // Fallback, if conditions resulted in no packets.
        if (analyticsDataPackets.empty())
        {
            for (int i = 0; i < 101; ++i)
                analyticsDataPackets.push_back(generateRandomPacket(1));
        }

        m_analyticsDataPackets = analyticsDataPackets;
        saveAnalyticsDataPackets(std::move(analyticsDataPackets));
    }

    void saveAnalyticsDataPackets(
        std::vector<common::metadata::ConstDetectionMetadataPacketPtr> analyticsDataPackets)
    {
        for (const auto& packet: analyticsDataPackets)
            whenIssueSavePacket(packet);

        for (const auto& packet: analyticsDataPackets)
            thenSaveSucceeded();
    }

    std::vector<common::metadata::ConstDetectionMetadataPacketPtr> filterPackets(
        const Filter& filter)
    {
        std::vector<common::metadata::ConstDetectionMetadataPacketPtr> result;
        for (const auto& packet: m_analyticsDataPackets)
        {
            if (satisfiesFilter(filter, packet))
                result.push_back(packet);
        }

        return result;
    }

    bool satisfiesFilter(
        const Filter& filter,
        common::metadata::ConstDetectionMetadataPacketPtr packet)
    {
        if (!filter.deviceId.isNull() && packet->deviceId != filter.deviceId)
            return false;

        return true;
    }
};

TEST_F(AnalyticsEventsStorageLookup, empty_filter_matches_all_events)
{
    whenLookupByEmptyFilter();
    thenResultMatchesExpectations();
}

TEST_F(AnalyticsEventsStorageLookup, lookup_by_deviceId)
{
    whenLookupByAnyDeviceId();
    thenResultMatchesExpectations();
}

// TEST_F(AnalyticsEventsStorageLookup, lookup_by_time_period)

// Advanced Lookup.

// TEST_F(AnalyticsEventsStorage, full_text_search)
// TEST_F(AnalyticsEventsStorage, lookup_by_attribute_value)
// TEST_F(AnalyticsEventsStorage, lookup_by_bounding_box)
// TEST_F(AnalyticsEventsStorage, lookup_by_rectangle)
// TEST_F(AnalyticsEventsStorage, lookup_by_objectId)
// TEST_F(AnalyticsEventsStorage, lookup_by_objectTypeId)
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
} // namespace nx
