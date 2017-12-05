#include <memory>

#include <gtest/gtest.h>

#include <nx/utils/random.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/std/future.h>
#include <nx/utils/test_support/test_with_temporary_directory.h>
#include <nx/utils/thread/sync_queue.h>

#include <analytics/detected_objects_storage/analytics_events_storage.h>
#include <test_support/analytics/storage/analytics_storage_types.h>

namespace nx {
namespace analytics {
namespace storage {
namespace test {

static const auto kUsecInMs = 1000;

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

    void whenLookupObjects(const Filter& filter)
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
        whenLookupObjects(Filter());

        thenLookupSucceded();
        andLookupResultMatches(Filter(), m_analyticsDataPackets);
    }

    void andLookupResultMatches(
        const Filter& filter,
        const std::vector<common::metadata::ConstDetectionMetadataPacketPtr>& expected)
    {
        auto objects = toDetectedObjects(expected);

        std::sort(
            objects.begin(), objects.end(),
            [&filter](const DetectedObject& left, const DetectedObject& right)
            {
                if (filter.sortOrder == Qt::SortOrder::AscendingOrder)
                    return left.track.front().timestampUsec < right.track.front().timestampUsec;
                else
                    return left.track.front().timestampUsec > right.track.front().timestampUsec;
            });

        andLookupResultEquals(filterObjects(objects, filter));
    }

    void thenLookupSucceded()
    {
        m_prevLookupResult = m_lookupResultQueue.pop();
        ASSERT_EQ(ResultCode::ok, m_prevLookupResult->resultCode);
    }

    void andLookupResultEquals(
        std::vector<DetectedObject> expected)
    {
        ASSERT_EQ(expected, m_prevLookupResult->eventsFound);
    }

    std::vector<DetectedObject> toDetectedObjects(
        const std::vector<common::metadata::ConstDetectionMetadataPacketPtr>& analyticsDataPackets)
    {
        std::vector<DetectedObject> result;
        for (const auto& packet: analyticsDataPackets)
        {
            for (const auto& object: packet->objects)
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

    std::vector<DetectedObject> filterObjects(
        std::vector<DetectedObject> objects,
        const Filter& filter)
    {
        for (auto objectIter = objects.begin(); objectIter != objects.end(); )
        {
            for (auto objectPositionIter = objectIter->track.begin();
                objectPositionIter != objectIter->track.end();
                )
            {
                if (satisfiesFilter(filter, *objectPositionIter))
                    ++objectPositionIter;
                else
                    objectPositionIter = objectIter->track.erase(objectPositionIter);
            }

            if (objectIter->track.empty())
                objectIter = objects.erase(objectIter);
            else
                ++objectIter;
        }

        if (filter.maxObjectsToSelect > 0 && (int)objects.size() > filter.maxObjectsToSelect)
            objects.erase(objects.begin() + filter.maxObjectsToSelect, objects.end());

        return objects;
    }

    void assertEqual(
        const std::vector<common::metadata::ConstDetectionMetadataPacketPtr>& left,
        const std::vector<common::metadata::ConstDetectionMetadataPacketPtr>& right)
    {
        ASSERT_EQ(left.size(), right.size());
        // TODO
    }

    std::vector<common::metadata::ConstDetectionMetadataPacketPtr> filterPackets(
        const Filter& filter,
        std::vector<common::metadata::ConstDetectionMetadataPacketPtr> packets)
    {
        std::vector<common::metadata::ConstDetectionMetadataPacketPtr> filteredPackets;
        std::copy_if(
            packets.begin(),
            packets.end(),
            std::back_inserter(filteredPackets),
            [this, &filter](const common::metadata::ConstDetectionMetadataPacketPtr& packet)
            {
                return satisfiesFilter(filter, *packet);
            });

        return filteredPackets;
    }

    EventsStorage& eventsStorage()
    {
        return *m_eventsStorage;
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

    bool satisfiesFilter(
        const Filter& filter,
        const ObjectPosition& data)
    {
        if (!filter.boundingBox.isNull())
        {
            if (!filter.boundingBox.intersects(data.boundingBox))
                return false;
        }

        return satisfiesCommonConditions(filter, data);
    }

    bool satisfiesFilter(
        const Filter& filter,
        const common::metadata::DetectionMetadataPacket& data)
    {
        if (!filter.boundingBox.isNull())
        {
            bool intersects = false;
            for (const auto& object: data.objects)
                intersects |= filter.boundingBox.intersects(object.boundingBox);
            if (!intersects)
                return false;
        }

        return satisfiesCommonConditions(filter, data);
    }

    template<typename T>
    bool satisfiesCommonConditions(const Filter& filter, const T& data)
    {
        if (!filter.deviceId.isNull() && data.deviceId != filter.deviceId)
            return false;

        if (!filter.timePeriod.isNull() &&
            !filter.timePeriod.contains(data.timestampUsec / kUsecInMs))
        {
            return false;
        }

        return true;
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
        m_allowedTimeRange(
            std::chrono::system_clock::from_time_t(0),
            std::chrono::system_clock::from_time_t(0))
    {
    }

protected:
    virtual void SetUp() override
    {
        base_type::SetUp();

        generateVariousEvents();
    }

    void addRandomKnownDeviceIdToFilter()
    {
        ASSERT_FALSE(m_allowedDeviceIds.empty());
        m_filter.deviceId = nx::utils::random::choice(m_allowedDeviceIds);
    }

    void addRandomNonEmptyTimePeriodToFilter()
    {
        using namespace std::chrono;

        m_filter.timePeriod.startTimeMs = duration_cast<milliseconds>(
            (m_allowedTimeRange.first + (m_allowedTimeRange.second - m_allowedTimeRange.first) / 2)
            .time_since_epoch()).count();
        m_filter.timePeriod.durationMs = duration_cast<milliseconds>(
            (m_allowedTimeRange.second - m_allowedTimeRange.first) /
            nx::utils::random::number<int>(3, 10)).count();
    }

    void addEmptyTimePeriodToFilter()
    {
        using namespace std::chrono;

        m_filter.timePeriod.startTimeMs = duration_cast<milliseconds>(
            (m_allowedTimeRange.first - std::chrono::hours(2)).time_since_epoch()).count();
        m_filter.timePeriod.durationMs =
            duration_cast<milliseconds>(std::chrono::hours(1)).count();
    }

    void addLimitToFilter()
    {
        m_filter.maxObjectsToSelect = filterObjects(
            toDetectedObjects(m_analyticsDataPackets), m_filter).size() / 2;
    }

    void addRandomBoundingBoxToFilter()
    {
        m_filter.boundingBox = QRectF(
            nx::utils::random::number<float>(0, 1),
            nx::utils::random::number<float>(0, 1),
            nx::utils::random::number<float>(0, 1),
            nx::utils::random::number<float>(0, 1));
    }

    void givenEmptyFilter()
    {
        m_filter = Filter();
    }

    void givenRandomFilter()
    {
        addRandomKnownDeviceIdToFilter();
        addRandomNonEmptyTimePeriodToFilter();
        if (nx::utils::random::number<bool>())
            addLimitToFilter();
        if (nx::utils::random::number<bool>())
            addRandomBoundingBoxToFilter();
    }

    void setSortOrder(Qt::SortOrder sortOrder)
    {
        m_filter.sortOrder = sortOrder;
    }

    void whenLookupObjects()
    {
        base_type::whenLookupObjects(m_filter);
    }

    void whenLookupByEmptyFilter()
    {
        givenEmptyFilter();
        whenLookupObjects();
    }

    void whenLookupByRandomKnownDeviceId()
    {
        addRandomKnownDeviceIdToFilter();
        whenLookupObjects();
    }

    void whenLookupByRandomNonEmptyTimePeriod()
    {
        addRandomNonEmptyTimePeriodToFilter();
        whenLookupObjects();
    }

    void whenLookupByEmptyTimePeriod()
    {
        addEmptyTimePeriodToFilter();
        whenLookupObjects();
    }

    void whenLookupWithLimit()
    {
        addLimitToFilter();
        whenLookupObjects();
    }

    void whenLookupByRandomBoundingBox()
    {
        addRandomBoundingBoxToFilter();
        whenLookupObjects();
    }

    void thenResultMatchesExpectations()
    {
        thenLookupSucceded();
        andLookupResultMatches(m_filter, m_analyticsDataPackets);
    }

    const Filter& filter() const
    {
        return m_filter;
    }

    const std::vector<common::metadata::ConstDetectionMetadataPacketPtr>& analyticsDataPackets() const
    {
        return m_analyticsDataPackets;
    }

private:
    std::vector<QnUuid> m_allowedDeviceIds;
    std::pair<std::chrono::system_clock::time_point, std::chrono::system_clock::time_point>
        m_allowedTimeRange;
    std::vector<common::metadata::ConstDetectionMetadataPacketPtr> m_analyticsDataPackets;
    Filter m_filter;

    void generateVariousEvents()
    {
        std::vector<QnUuid> deviceIds;
        for (int i = 0; i < 3; ++i)
            deviceIds.push_back(QnUuid::createUuid());
        setAllowedDeviceIds(deviceIds);

        setAllowedTimeRange(
            std::chrono::system_clock::now() - std::chrono::hours(24),
            std::chrono::system_clock::now());

        generateEventsByCriteria();
    }

    void setAllowedDeviceIds(std::vector<QnUuid> deviceIds)
    {
        m_allowedDeviceIds = std::move(deviceIds);
    }

    void setAllowedTimeRange(
        std::chrono::system_clock::time_point start,
        std::chrono::system_clock::time_point end)
    {
        m_allowedTimeRange.first = start;
        m_allowedTimeRange.second = end;
    }

    void generateEventsByCriteria()
    {
        using namespace std::chrono;

        const int eventsPerDevice = 11;

        std::vector<common::metadata::ConstDetectionMetadataPacketPtr> analyticsDataPackets;
        for (const auto& deviceId: m_allowedDeviceIds)
        {
            for (int i = 0; i < eventsPerDevice; ++i)
            {
                auto packet = generateRandomPacket(1);
                packet->deviceId = deviceId;
                if (m_allowedTimeRange.second > m_allowedTimeRange.first)
                {
                    packet->timestampUsec = nx::utils::random::number<qint64>(
                        duration_cast<microseconds>(
                            m_allowedTimeRange.first.time_since_epoch()).count(),
                        duration_cast<microseconds>(
                            m_allowedTimeRange.second.time_since_epoch()).count());
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
};

TEST_F(AnalyticsEventsStorageLookup, empty_filter_matches_all_events)
{
    whenLookupByEmptyFilter();
    thenResultMatchesExpectations();
}

TEST_F(AnalyticsEventsStorageLookup, lookup_by_deviceId)
{
    whenLookupByRandomKnownDeviceId();
    thenResultMatchesExpectations();
}

TEST_F(AnalyticsEventsStorageLookup, lookup_by_non_empty_time_period)
{
    whenLookupByRandomNonEmptyTimePeriod();
    thenResultMatchesExpectations();
}

TEST_F(AnalyticsEventsStorageLookup, lookup_by_empty_time_period)
{
    whenLookupByEmptyTimePeriod();
    thenResultMatchesExpectations();
}

TEST_F(AnalyticsEventsStorageLookup, lookup_result_limit)
{
    whenLookupWithLimit();
    thenResultMatchesExpectations();
}

TEST_F(AnalyticsEventsStorageLookup, sort_lookup_result_by_timestamp_ascending)
{
    givenRandomFilter();
    setSortOrder(Qt::SortOrder::AscendingOrder);

    whenLookupObjects();

    thenResultMatchesExpectations();
}

TEST_F(AnalyticsEventsStorageLookup, sort_lookup_result_by_timestamp_descending)
{
    givenRandomFilter();
    setSortOrder(Qt::SortOrder::DescendingOrder);

    whenLookupObjects();

    thenResultMatchesExpectations();
}

// Advanced Lookup.

// TEST_F(AnalyticsEventsStorage, full_text_search)
// TEST_F(AnalyticsEventsStorage, lookup_by_attribute_value)

TEST_F(AnalyticsEventsStorageLookup, lookup_by_bounding_box)
{
    whenLookupByRandomBoundingBox();
    thenResultMatchesExpectations();
}

// TEST_F(AnalyticsEventsStorage, lookup_by_objectId)
// TEST_F(AnalyticsEventsStorage, lookup_by_objectTypeId)
// TEST_F(AnalyticsEventsStorage, lookup_stress_test)

//-------------------------------------------------------------------------------------------------
// Cursor.

class AnalyticsEventsStorageCursor:
    public AnalyticsEventsStorageLookup
{
    using base_type = AnalyticsEventsStorageLookup;

protected:
    void whenReadDataUsingCursor()
    {
        whenCreateCursor();
        thenCursorIsCreated();

        readAllDataFromCursor();
    }

    void whenCreateCursor()
    {
        using namespace std::placeholders;

        eventsStorage().createLookupCursor(
            filter(),
            std::bind(&AnalyticsEventsStorageCursor::saveCursor, this, _1, _2));
    }

    void thenCursorIsCreated()
    {
        m_cursor = m_createdCursorsQueue.pop();
        ASSERT_NE(nullptr, m_cursor);
    }

    void thenResultMatchesExpectations()
    {
        assertEqual(filterPackets(filter(), analyticsDataPackets()), m_packetsRead);
    }

private:
    std::vector<common::metadata::ConstDetectionMetadataPacketPtr> m_packetsRead;
    nx::utils::SyncQueue<std::unique_ptr<AbstractCursor>> m_createdCursorsQueue;
    std::unique_ptr<AbstractCursor> m_cursor;

    void readAllDataFromCursor()
    {
        while (auto packet = m_cursor->next())
            m_packetsRead.push_back(packet);
    }

    void saveCursor(
        ResultCode /*resultCode*/,
        std::unique_ptr<AbstractCursor> cursor)
    {
        m_createdCursorsQueue.push(std::move(cursor));
    }
};

TEST_F(AnalyticsEventsStorageCursor, cursor_provides_all_matched_data)
{
    givenRandomFilter();
    setSortOrder(Qt::SortOrder::DescendingOrder);

    whenReadDataUsingCursor();

    thenResultMatchesExpectations();
}

//TEST_F(AnalyticsEventsStorage, many_concurrent_cursors)

//-------------------------------------------------------------------------------------------------
// Deprecating data.

} // namespace test
} // namespace storage
} // namespace analytics
} // namespace nx
