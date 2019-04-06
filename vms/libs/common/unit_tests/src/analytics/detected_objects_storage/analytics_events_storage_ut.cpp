#include <memory>

#include <gtest/gtest.h>

#include <nx/utils/random.h>
#include <nx/utils/std/algorithm.h>
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
        nx::utils::test::TestWithTemporaryDirectory("analytics_events_storage", QString()),
        m_allowedTimeRange(
            std::chrono::system_clock::from_time_t(0),
            std::chrono::system_clock::from_time_t(0))
    {
        m_settings.dbConnectionOptions.dbName =
            nx::utils::test::TestWithTemporaryDirectory::testDataDir() + "/events.sqlite";
        m_settings.dbConnectionOptions.driverType = nx::sql::RdbmsDriverType::sqlite;
        m_settings.dbConnectionOptions.maxConnectionCount = 17;

        m_attributeDictionary.initialize(5, 2);
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
        const std::vector<common::metadata::DetectionMetadataPacketPtr>& expected)
    {
        auto objects = toDetectedObjects(expected);
        const auto filteredObjects =
            filterObjectsAndApplySortOrder(filter, std::move(objects));
        andLookupResultEquals(filteredObjects);
    }

    std::vector<nx::analytics::storage::DetectedObject> filterObjectsAndApplySortOrder(
        const Filter& filter,
        std::vector<nx::analytics::storage::DetectedObject> objects)
    {
        // First, sorting objects in descending order, because we always filtering the most recent objects
        // and filter.sortOrder is applied AFTER fitering.
        std::sort(
            objects.begin(), objects.end(),
            [&filter](const DetectedObject& left, const DetectedObject& right)
            {
                return left.track.front().timestampUsec > right.track.front().timestampUsec;
            });

        auto filteredObjects = filterObjects(objects, filter);

        std::sort(
            filteredObjects.begin(), filteredObjects.end(),
            [&filter](const DetectedObject& left, const DetectedObject& right)
            {
                if (filter.sortOrder == Qt::AscendingOrder)
                    return left.track.front().timestampUsec < right.track.front().timestampUsec;
                else
                    return left.track.front().timestampUsec > right.track.front().timestampUsec;
            });

        return filteredObjects;
    }

    bool isLookupResultEquals(
        const Filter& filter,
        const std::vector<common::metadata::DetectionMetadataPacketPtr>& expected)
    {
        auto objects = toDetectedObjects(expected);
        const auto filteredObjects =
            filterObjectsAndApplySortOrder(filter, std::move(objects));
        return filteredObjects == m_prevLookupResult->eventsFound;
    }

    void thenLookupSucceded()
    {
        m_prevLookupResult = m_lookupResultQueue.pop();
        ASSERT_EQ(ResultCode::ok, m_prevLookupResult->resultCode);
    }

    void andLookupResultEquals(
        const std::vector<DetectedObject>& expected)
    {
        ASSERT_EQ(expected, m_prevLookupResult->eventsFound);
    }

    std::vector<DetectedObject> toDetectedObjects(
        const std::vector<common::metadata::DetectionMetadataPacketPtr>& analyticsDataPackets) const
    {
        std::vector<DetectedObject> result;
        for (const auto& packet: analyticsDataPackets)
        {
            for (const auto& object: packet->objects)
            {
                DetectedObject detectedObject;
                detectedObject.objectAppearanceId = object.objectId;
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

        // Groupping object track.
        std::sort(
            result.begin(), result.end(),
            [](const DetectedObject& left, const DetectedObject& right)
            {
                return left.objectAppearanceId < right.objectAppearanceId;
            });

        for (auto it = result.begin(); it != result.end();)
        {
            auto nextIter = std::next(it);
            if (nextIter == result.end())
                break;

            if (it->objectAppearanceId == nextIter->objectAppearanceId)
            {
                // Merging.
                it->firstAppearanceTimeUsec =
                    std::min(it->firstAppearanceTimeUsec, nextIter->firstAppearanceTimeUsec);
                it->lastAppearanceTimeUsec =
                    std::max(it->lastAppearanceTimeUsec, nextIter->lastAppearanceTimeUsec);
                std::move(
                    nextIter->track.begin(), nextIter->track.end(),
                    std::back_inserter(it->track));
                std::move(
                    nextIter->attributes.begin(), nextIter->attributes.end(),
                    std::back_inserter(it->attributes));
                result.erase(nextIter);
            }
            else
            {
                ++it;
            }
        }

        for (auto& detectedObject: result)
        {
            std::sort(
                detectedObject.track.begin(), detectedObject.track.end(),
                [](const ObjectPosition& left, const ObjectPosition& right)
                    { return left.timestampUsec < right.timestampUsec; });

            if (!detectedObject.track.empty())
            {
                detectedObject.firstAppearanceTimeUsec =
                    detectedObject.track.begin()->timestampUsec;
                detectedObject.lastAppearanceTimeUsec =
                    detectedObject.track.rbegin()->timestampUsec;
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
            if (!satisfiesFilter(filter, *objectIter))
            {
                objectIter = objects.erase(objectIter);
                continue;
            }

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

        if (filter.maxTrackSize > 0)
        {
            for (auto& object: objects)
            {
                if ((int) object.track.size() > filter.maxTrackSize)
                {
                    object.track.erase(
                        object.track.begin() + filter.maxTrackSize,
                        object.track.end());
                }
            }
        }

        return objects;
    }

    template<typename ContainerOne, typename ContainerTwo>
    void assertEqual(const ContainerOne& left, const ContainerTwo& right)
    {
        ASSERT_EQ(left.size(), right.size());
        for (std::size_t i = 0; i < left.size(); ++i)
        {
            ASSERT_EQ(*left[i], *right[i]);
        }
    }

    std::vector<common::metadata::DetectionMetadataPacketPtr> filterPackets(
        const Filter& filter,
        std::vector<common::metadata::DetectionMetadataPacketPtr> packets)
    {
        std::vector<common::metadata::DetectionMetadataPacketPtr> filteredPackets;
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

    std::vector<common::metadata::DetectionMetadataPacketPtr> sortPacketsByTimestamp(
        std::vector<common::metadata::DetectionMetadataPacketPtr> packets,
        Qt::SortOrder sortOrder)
    {
        using namespace common::metadata;

        std::sort(
            packets.begin(), packets.end(),
            [sortOrder](
                const ConstDetectionMetadataPacketPtr& leftPacket,
                const ConstDetectionMetadataPacketPtr& rightPacket)
            {
                const auto& leftValue =
                    std::tie(leftPacket->timestampUsec, leftPacket->durationUsec);
                const auto& rightValue =
                    std::tie(rightPacket->timestampUsec, rightPacket->durationUsec);

                return sortOrder == Qt::AscendingOrder
                    ? leftValue < rightValue
                    : leftValue > rightValue;
            });

        // Sorting objects in packets by objectAppearanceId.
        for (auto& packet: packets)
        {
            std::sort(
                packet->objects.begin(), packet->objects.end(),
                [sortOrder](
                    const nx::common::metadata::DetectedObject& left,
                    const nx::common::metadata::DetectedObject& right)
                {
                    return sortOrder == Qt::AscendingOrder
                        ? left.objectId < right.objectId
                        : left.objectId > right.objectId;
                });
        }

        return packets;
    }

    void setAllowedDeviceIds(std::vector<QnUuid> deviceIds)
    {
        m_allowedDeviceIds = std::move(deviceIds);
    }

    const std::vector<QnUuid>& allowedDeviceIds() const
    {
        return m_allowedDeviceIds;
    }

    void setAllowedTimeRange(
        std::chrono::system_clock::time_point start,
        std::chrono::system_clock::time_point end)
    {
        m_allowedTimeRange.first = start;
        m_allowedTimeRange.second = end;
    }

    std::pair<std::chrono::system_clock::time_point, std::chrono::system_clock::time_point>
        allowedTimeRange() const
    {
        return m_allowedTimeRange;
    }

    const AttributeDictionary& attributeDictionary() const
    {
        return m_attributeDictionary;
    }

    std::vector<common::metadata::DetectionMetadataPacketPtr> generateEventsByCriteria()
    {
        using namespace std::chrono;

        const int eventsPerDevice = 11;

        std::vector<common::metadata::DetectionMetadataPacketPtr> analyticsDataPackets;
        for (const auto& deviceId : m_allowedDeviceIds)
        {
            for (int i = 0; i < eventsPerDevice; ++i)
            {
                auto packet = generateRandomPacket(1, &m_attributeDictionary);
                packet->deviceId = deviceId;
                if (m_allowedTimeRange.second > m_allowedTimeRange.first)
                {
                    packet->timestampUsec = nx::utils::random::number<qint64>(
                        duration_cast<milliseconds>(m_allowedTimeRange.first.time_since_epoch()).count(),
                        duration_cast<milliseconds>(m_allowedTimeRange.second.time_since_epoch()).count())
                        * 1000;
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

        return analyticsDataPackets;
    }

    void saveAnalyticsDataPackets(
        std::vector<common::metadata::DetectionMetadataPacketPtr> analyticsDataPackets)
    {
        std::copy(
            analyticsDataPackets.begin(), analyticsDataPackets.end(),
            std::back_inserter(m_analyticsDataPackets));

        std::for_each(analyticsDataPackets.begin(), analyticsDataPackets.end(),
            [this](const auto& packet) { whenIssueSavePacket(packet); });

        std::for_each(analyticsDataPackets.begin(), analyticsDataPackets.end(),
            [this](const auto&) { thenSaveSucceeded(); });
    }

    EventsStorage& eventsStorage()
    {
        return *m_eventsStorage;
    }

    const std::vector<common::metadata::DetectionMetadataPacketPtr>& analyticsDataPackets() const
    {
        return m_analyticsDataPackets;
    }

    void aggregateAnalyticsDataPacketsByTimestamp()
    {
        if (m_analyticsDataPackets.empty())
            return;

        m_analyticsDataPackets =
            sortPacketsByTimestamp(m_analyticsDataPackets, Qt::SortOrder::AscendingOrder);

        auto prevPacketIter = m_analyticsDataPackets.begin();
        for (auto it = std::next(prevPacketIter);
            it != m_analyticsDataPackets.end();
            )
        {
            if ((*it)->timestampUsec == (*prevPacketIter)->timestampUsec &&
                (*it)->deviceId == (*prevPacketIter)->deviceId)
            {
                std::move(
                    (*it)->objects.begin(), (*it)->objects.end(),
                    std::back_inserter((*prevPacketIter)->objects));
                (*prevPacketIter)->durationUsec =
                    std::max((*it)->durationUsec, (*prevPacketIter)->durationUsec);
                it = m_analyticsDataPackets.erase(it);
            }
            else
            {
                prevPacketIter = it;
                ++it;
            }
        }
    }

private:
    struct LookupResult
    {
        ResultCode resultCode;
        std::vector<DetectedObject> eventsFound;
    };

    std::unique_ptr<EventsStorage> m_eventsStorage;
    Settings m_settings;
    std::vector<common::metadata::DetectionMetadataPacketPtr> m_analyticsDataPackets;
    nx::utils::SyncQueue<ResultCode> m_savePacketResultQueue;
    nx::utils::SyncQueue<LookupResult> m_lookupResultQueue;
    boost::optional<LookupResult> m_prevLookupResult;

    std::vector<QnUuid> m_allowedDeviceIds;
    std::pair<std::chrono::system_clock::time_point, std::chrono::system_clock::time_point>
        m_allowedTimeRange;
    AttributeDictionary m_attributeDictionary;

    bool initializeStorage()
    {
        m_eventsStorage = std::make_unique<EventsStorage>(m_settings);
        return m_eventsStorage->initialize();
    }

    bool satisfiesFilter(
        const Filter& filter, const DetectedObject& detectedObject)
    {
        if (!filter.objectAppearanceId.isNull() && detectedObject.objectAppearanceId != filter.objectAppearanceId)
            return false;

        if (!filter.objectTypeId.empty() &&
            std::find(
                filter.objectTypeId.begin(), filter.objectTypeId.end(),
                detectedObject.objectTypeId) == filter.objectTypeId.end())
        {
            return false;
        }

        if (!filter.freeText.isEmpty() &&
            !matchAttributes(detectedObject.attributes, filter.freeText))
        {
            return false;
        }

        return true;
    }

    bool satisfiesFilter(
        const Filter& filter,
        const ObjectPosition& data)
    {
        if (!satisfiesCommonConditions(filter, data))
            return false;

        if (!filter.boundingBox.isNull() && !filter.boundingBox.intersects(data.boundingBox))
            return false;

        return true;
    }

    bool satisfiesFilter(
        const Filter& filter,
        const common::metadata::DetectionMetadataPacket& data)
    {
        if (!filter.objectAppearanceId.isNull())
        {
            bool hasRequiredObject = false;
            for (const auto& object: data.objects)
                hasRequiredObject |= object.objectId == filter.objectAppearanceId;
            if (!hasRequiredObject)
                return false;
        }

        if (!filter.boundingBox.isNull())
        {
            bool intersects = false;
            for (const auto& object: data.objects)
                intersects |= filter.boundingBox.intersects(object.boundingBox);
            if (!intersects)
                return false;
        }

        if (!filter.objectTypeId.empty())
        {
            bool hasProperType = false;
            for (const auto& object: data.objects)
            {
                hasProperType |= std::find(
                    filter.objectTypeId.begin(), filter.objectTypeId.end(),
                    object.objectTypeId) != filter.objectTypeId.end();
            }
            if (!hasProperType)
                return false;
        }

        if (!filter.freeText.isEmpty())
        {
            bool isFilterSatisfied = false;
            for (const auto& object: data.objects)
                isFilterSatisfied |= matchAttributes(object.labels, filter.freeText);
            if (!isFilterSatisfied)
                return false;
        }

        return satisfiesCommonConditions(filter, data);
    }

    template<typename T>
    bool satisfiesCommonConditions(const Filter& filter, const T& data)
    {
        if (!filter.deviceIds.empty() && !nx::utils::contains(filter.deviceIds, data.deviceId))
            return false;

        if (!filter.timePeriod.isNull() &&
            !filter.timePeriod.contains(data.timestampUsec / kUsecInMs))
        {
            return false;
        }

        return true;
    }

    bool matchAttributes(
        const std::vector<nx::common::metadata::Attribute>& attributes,
        const QString& filter)
    {
        const auto filterWords = filter.split(L' ');
        // Attributes have to contain all words.
        for (const auto& filterWord: filterWords)
        {
            bool found = false;
            for (const auto& attribute: attributes)
            {
                if (attribute.name.indexOf(filterWord) != -1 ||
                    attribute.value.indexOf(filterWord) != -1)
                {
                    found = true;
                    break;
                }
            }

            if (!found)
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

protected:
    virtual void SetUp() override
    {
        base_type::SetUp();

        if (HasFatalFailure())
            return;

        generateVariousEvents();
    }

    void addRandomKnownDeviceIdToFilter()
    {
        ASSERT_FALSE(allowedDeviceIds().empty());
        m_filter.deviceIds.push_back(
            nx::utils::random::choice(allowedDeviceIds()));
    }

    void addRandomNonEmptyTimePeriodToFilter()
    {
        using namespace std::chrono;

        m_filter.timePeriod.startTimeMs = duration_cast<milliseconds>(
            (allowedTimeRange().first + (allowedTimeRange().second - allowedTimeRange().first) / 2)
            .time_since_epoch()).count();
        m_filter.timePeriod.durationMs = duration_cast<milliseconds>(
            (allowedTimeRange().second - allowedTimeRange().first) /
            nx::utils::random::number<int>(3, 10)).count();
    }

    void addEmptyTimePeriodToFilter()
    {
        using namespace std::chrono;

        m_filter.timePeriod.startTimeMs = duration_cast<milliseconds>(
            (allowedTimeRange().first - std::chrono::hours(2)).time_since_epoch()).count();
        m_filter.timePeriod.durationMs =
            duration_cast<milliseconds>(std::chrono::hours(1)).count();
    }

    void addRandomObjectIdToFilter()
    {
        const auto& randomPacket = nx::utils::random::choice(analyticsDataPackets());
        const auto& randomObject = nx::utils::random::choice(randomPacket->objects);
        m_filter.objectAppearanceId = randomObject.objectId;
    }

    void addRandomObjectTypeIdToFilter()
    {
        const auto& randomPacket = nx::utils::random::choice(analyticsDataPackets());
        const auto& randomObject = nx::utils::random::choice(randomPacket->objects);
        m_filter.objectTypeId.push_back(randomObject.objectTypeId);
    }

    void addMaxObjectsLimitToFilter()
    {
        m_filter.maxObjectsToSelect = (int) filterObjects(
            toDetectedObjects(analyticsDataPackets()), m_filter).size() / 2;
    }

    void addMaxTrackLengthLimitToFilter()
    {
        // TODO: #ak Currently, only 1 track element can be returned.
        m_filter.maxTrackSize = 1;
    }

    void addRandomTextFoundInDataToFilter()
    {
        m_filter.freeText = attributeDictionary().getRandomText();
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

        if (nx::utils::random::number<bool>())
            addRandomNonEmptyTimePeriodToFilter();

        if (nx::utils::random::number<bool>())
            addRandomObjectIdToFilter();

        if (nx::utils::random::number<bool>())
            addRandomObjectTypeIdToFilter();

        if (nx::utils::random::number<bool>())
            addMaxObjectsLimitToFilter();

        if (nx::utils::random::number<bool>())
            addRandomBoundingBoxToFilter();

        if (nx::utils::random::number<bool>())
            addRandomTextFoundInDataToFilter();
    }

    void givenRandomFilterWithMultipleDeviceIds()
    {
        givenRandomFilter();

        m_filter.deviceIds.clear();
        for (const auto& deviceId: allowedDeviceIds())
            m_filter.deviceIds.push_back(deviceId);

        m_filter.objectAppearanceId = QnUuid();
    }

    void givenObjectWithLongTrack()
    {
        using namespace std::chrono;

        m_specificObjectAppearanceId = QnUuid::createUuid();
        auto analyticsDataPackets = generateEventsByCriteria();

        qint64 objectTrackStartTime = std::numeric_limits<qint64>::max();
        qint64 objectTrackEndTime = std::numeric_limits<qint64>::min();

        for (auto& packet: analyticsDataPackets)
        {
            objectTrackStartTime = std::min(objectTrackStartTime, packet->timestampUsec);
            objectTrackEndTime =
                std::max(objectTrackEndTime, packet->timestampUsec + packet->durationUsec);

            for (auto& object: packet->objects)
            {
                object.objectId = m_specificObjectAppearanceId;
                object.objectTypeId = m_specificObjectAppearanceId.toString();
            }
        }

        m_specificObjectTimePeriod.setStartTime(
            duration_cast<milliseconds>(microseconds(objectTrackStartTime)));
        m_specificObjectTimePeriod.setDuration(
            duration_cast<milliseconds>(microseconds(objectTrackEndTime - objectTrackStartTime)));

        saveAnalyticsDataPackets(analyticsDataPackets);
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

    void whenLookupByRandomObjectId()
    {
        addRandomObjectIdToFilter();
        whenLookupObjects();
    }

    void whenLookupByRandomObjectTypeId()
    {
        addRandomObjectTypeIdToFilter();
        whenLookupObjects();
    }

    void whenLookupWithMaxObjectsLimit()
    {
        addMaxObjectsLimitToFilter();
        whenLookupObjects();
    }

    void whenLookupWithMaxTrackLengthLimit()
    {
        m_filter.objectAppearanceId = m_specificObjectAppearanceId;
        addMaxTrackLengthLimitToFilter();
        //m_filter.maxTrackSize = 1;

        whenLookupObjects();
    }

    void whenLookupByRandomTextFoundInData()
    {
        addRandomTextFoundInDataToFilter();
        whenLookupObjects();
    }

    void whenLookupByRandomBoundingBox()
    {
        addRandomBoundingBoxToFilter();
        whenLookupObjects();
    }

    void whenLookupByRandomNonEmptyTimePeriodCoveringPartOfTrack()
    {
        m_filter.objectAppearanceId = m_specificObjectAppearanceId;
        m_filter.timePeriod.setStartTime(
            m_specificObjectTimePeriod.startTime() + m_specificObjectTimePeriod.duration() / 3);
        m_filter.timePeriod.setDuration(m_specificObjectTimePeriod.duration() / 3);

        whenLookupObjects();
    }

    void thenResultMatchesExpectations()
    {
        thenLookupSucceded();
        andLookupResultMatches(m_filter, analyticsDataPackets());
    }

    const Filter& filter() const
    {
        return m_filter;
    }

    Filter& filter()
    {
        return m_filter;
    }

private:
    Filter m_filter;
    QnUuid m_specificObjectAppearanceId;
    QnTimePeriod m_specificObjectTimePeriod;

    void generateVariousEvents()
    {
        std::vector<QnUuid> deviceIds;
        for (int i = 0; i < 3; ++i)
            deviceIds.push_back(QnUuid::createUuid());
        setAllowedDeviceIds(deviceIds);

        setAllowedTimeRange(
            std::chrono::system_clock::now() - std::chrono::hours(24),
            std::chrono::system_clock::now());

        saveAnalyticsDataPackets(generateEventsByCriteria());
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

TEST_F(AnalyticsEventsStorageLookup, full_track_timestamps)
{
    givenObjectWithLongTrack();
    whenLookupByRandomNonEmptyTimePeriodCoveringPartOfTrack();
    thenResultMatchesExpectations();
}

TEST_F(AnalyticsEventsStorageLookup, max_objects_limit)
{
    whenLookupWithMaxObjectsLimit();
    thenResultMatchesExpectations();
}

TEST_F(AnalyticsEventsStorageLookup, max_track_length_limit)
{
    givenObjectWithLongTrack();
    whenLookupWithMaxTrackLengthLimit();
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

TEST_F(AnalyticsEventsStorageLookup, full_text_search)
{
    whenLookupByRandomTextFoundInData();
    thenResultMatchesExpectations();
}

// TEST_F(AnalyticsEventsStorage, lookup_by_attribute_value)

TEST_F(AnalyticsEventsStorageLookup, lookup_by_bounding_box)
{
    whenLookupByRandomBoundingBox();
    thenResultMatchesExpectations();
}

TEST_F(AnalyticsEventsStorageLookup, lookup_by_objectId)
{
    whenLookupByRandomObjectId();
    thenResultMatchesExpectations();
}

TEST_F(AnalyticsEventsStorageLookup, lookup_by_objectTypeId)
{
    whenLookupByRandomObjectTypeId();
    thenResultMatchesExpectations();
}

// TEST_F(AnalyticsEventsStorageLookup, lookup_by_multiple_objectTypeId)

TEST_F(AnalyticsEventsStorageLookup, lookup_stress_test)
{
    givenRandomFilter();
    setSortOrder(
        nx::utils::random::number<bool>()
        ? Qt::SortOrder::AscendingOrder
        : Qt::SortOrder::DescendingOrder);

    whenLookupObjects();

    thenResultMatchesExpectations();
}

TEST_F(AnalyticsEventsStorageLookup, quering_data_from_multiple_cameras)
{
    givenRandomFilterWithMultipleDeviceIds();
    whenLookupObjects();
    thenResultMatchesExpectations();
}

//-------------------------------------------------------------------------------------------------
// Cursor.

class AnalyticsEventsStorageCursor:
    public AnalyticsEventsStorageLookup
{
    using base_type = AnalyticsEventsStorageLookup;

protected:
    void givenDetectedObjectsWithSameTimestamp()
    {
        auto newPackets = generateEventsByCriteria();
        newPackets.erase(std::next(newPackets.begin()), newPackets.end());

        const auto existingPackets = analyticsDataPackets();
        const auto& existingPacket = nx::utils::random::choice(existingPackets);

        newPackets.front()->deviceId = existingPacket->deviceId;
        newPackets.front()->timestampUsec = existingPacket->timestampUsec;

        saveAnalyticsDataPackets(std::move(newPackets));
        aggregateAnalyticsDataPacketsByTimestamp();
    }

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
        assertEqual(
            sortPacketsByTimestamp(
                filterPackets(filter(), analyticsDataPackets()),
                filter().sortOrder),
            m_packetsRead);
    }

    void andObjectsWithSameTimestampAreDeiveredInSinglePacket()
    {
        // TODO
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
    setSortOrder(nx::utils::random::number<bool>() ? Qt::AscendingOrder : Qt::DescendingOrder);

    whenReadDataUsingCursor();
    thenResultMatchesExpectations();
}

TEST_F(AnalyticsEventsStorageCursor, aggregates_objects_with_same_timestamp)
{
    givenDetectedObjectsWithSameTimestamp();

    whenReadDataUsingCursor();

    thenResultMatchesExpectations();
    andObjectsWithSameTimestampAreDeiveredInSinglePacket();
}

//-------------------------------------------------------------------------------------------------
// Time periods lookup.

class AnalyticsEventsStorageTimePeriodsLookup:
    public AnalyticsEventsStorageLookup
{
    using base_type = AnalyticsEventsStorageLookup;

protected:
    void givenAggregationPeriodEqualToTheWholeArchivePeriod()
    {
        const auto allTimePeriods = expectedLookupResult();
        const auto archiveLength =
            allTimePeriods.back().endTime() - allTimePeriods.front().startTime();

        m_lookupOptions.detailLevel = archiveLength;
    }

    void givenFilterWithTimePeriod()
    {
        const auto allTimePeriods = expectedLookupResult();
        const auto archiveLength =
            allTimePeriods.back().endTime() - allTimePeriods.front().startTime();

        filter().timePeriod.setStartTime(allTimePeriods.front().startTime() + archiveLength / 3);
        filter().timePeriod.setDuration(archiveLength * 2 / 3);
    }

    void givenRandomLookupOptions()
    {
        m_lookupOptions.detailLevel = std::chrono::milliseconds(
            nx::utils::random::number<int>(1, 1000000));
    }

    void whenLookupTimePeriods()
    {
        using namespace std::placeholders;

        eventsStorage().lookupTimePeriods(
            filter(),
            m_lookupOptions,
            [this](auto&&... args) { saveLookupResult(std::forward<decltype(args)>(args)...); });
    }

    void thenResultMatchesExpectations()
    {
        thenLookupSucceded();
        andResultMatchesExpectations();
    }

    void thenLookupSucceded()
    {
        m_prevLookupResult = m_lookupResultQueue.pop();
        ASSERT_EQ(ResultCode::ok, std::get<0>(m_prevLookupResult));
    }

    void andResultMatchesExpectations()
    {
        const auto expected = expectedLookupResult();
        ASSERT_EQ(expected, std::get<1>(m_prevLookupResult));
    }

private:
    std::tuple<ResultCode, QnTimePeriodList> m_prevLookupResult;
    nx::utils::SyncQueue<std::tuple<ResultCode, QnTimePeriodList>> m_lookupResultQueue;
    TimePeriodsLookupOptions m_lookupOptions;

    void saveLookupResult(ResultCode resultCode, QnTimePeriodList timePeriods)
    {
        m_lookupResultQueue.push(std::make_tuple(resultCode, std::move(timePeriods)));
    }

    QnTimePeriodList expectedLookupResult()
    {
        return filterTimePeriods(
            filter(),
            toTimePeriods(
                filterPackets(filter(), analyticsDataPackets()),
                m_lookupOptions));
    }

    QnTimePeriodList filterTimePeriods(
        const Filter& filter,
        const QnTimePeriodList& timePeriods)
    {
        if (filter.timePeriod.isEmpty())
            return timePeriods;

        QnTimePeriodList result;
        for (const auto& timePeriod: timePeriods)
        {
            auto resultingTimePeriod = timePeriod;
            if (resultingTimePeriod.startTime() < filter.timePeriod.startTime())
                resultingTimePeriod.setStartTime(filter.timePeriod.startTime());

            if (filter.timePeriod.durationMs != QnTimePeriod::kInfiniteDuration &&
                resultingTimePeriod.endTime() > filter.timePeriod.endTime())
            {
                resultingTimePeriod.setEndTime(filter.timePeriod.endTime());
            }

            result.push_back(resultingTimePeriod);
        }

        return result;
    }

    QnTimePeriodList toTimePeriods(
        const std::vector<common::metadata::DetectionMetadataPacketPtr>& packets,
        TimePeriodsLookupOptions lookupOptions)
    {
        using namespace std::chrono;

        QnTimePeriodList result;
        for (const auto& packet: packets)
        {
            QnTimePeriod timePeriod(
                duration_cast<milliseconds>(microseconds(packet->timestampUsec)),
                duration_cast<milliseconds>(microseconds(packet->durationUsec)));

            auto it = std::upper_bound(result.begin(), result.end(), timePeriod);
            result.insert(it, timePeriod);
        }

        return QnTimePeriodList::aggregateTimePeriodsUnconstrained(
            result,
            std::max<milliseconds>(lookupOptions.detailLevel, kMinTimePeriodAggregationPeriod));
    }
};

TEST_F(AnalyticsEventsStorageTimePeriodsLookup, selecting_all_periods)
{
    whenLookupTimePeriods();
    thenResultMatchesExpectations();
}

TEST_F(AnalyticsEventsStorageTimePeriodsLookup, selecting_all_periods_aggregated_to_one)
{
    givenAggregationPeriodEqualToTheWholeArchivePeriod();
    whenLookupTimePeriods();
    thenResultMatchesExpectations();
}

TEST_F(
    AnalyticsEventsStorageTimePeriodsLookup,
    selecting_periods_with_timestamp_filter_only)
{
    givenFilterWithTimePeriod();
    whenLookupTimePeriods();
    thenResultMatchesExpectations();
}

TEST_F(AnalyticsEventsStorageTimePeriodsLookup, with_aggregation_period)
{
    givenRandomLookupOptions();
    whenLookupTimePeriods();
    thenResultMatchesExpectations();
}

TEST_F(AnalyticsEventsStorageTimePeriodsLookup, with_random_filter)
{
    givenRandomFilter();
    whenLookupTimePeriods();
    thenResultMatchesExpectations();
}

// TODO: with_random_filter_and_aggregation

//-------------------------------------------------------------------------------------------------
// Deprecating data.

/**
 * Initial condition for every test: there is some random data in the DB.
 */
class AnalyticsEventsStorageCleanup:
    public AnalyticsEventsStorage
{
    using base_type = AnalyticsEventsStorage;

protected:
    void whenRemoveEventsUpToLatestEventTimestamp()
    {
        using namespace std::chrono;

        m_oldestAvailableDataTimestamp =
            duration_cast<milliseconds>(microseconds(getMaxTimestamp())) +
            std::chrono::milliseconds(1);
        eventsStorage().markDataAsDeprecated(m_deviceId, m_oldestAvailableDataTimestamp);
    }

    void whenRemoveEventsUpToEarlisestEventTimestamp()
    {
        using namespace std::chrono;

        m_oldestAvailableDataTimestamp =
            duration_cast<milliseconds>(microseconds(getMinTimestamp()));
        eventsStorage().markDataAsDeprecated(m_deviceId, m_oldestAvailableDataTimestamp);
    }

    void whenRemoveEventsUpToARandomAvailableTimestamp()
    {
        using namespace std::chrono;

        const auto minTimestamp = getMinTimestamp();
        const auto maxTimestamp = getMaxTimestamp();

        m_oldestAvailableDataTimestamp =
            duration_cast<milliseconds>(microseconds(
                minTimestamp + (maxTimestamp - minTimestamp) / 2));

        eventsStorage().markDataAsDeprecated(
            m_deviceId,
            m_oldestAvailableDataTimestamp);
    }

    void thenStorageIsEmpty()
    {
        thenDataIsExpected();
    }

    void thenDataIsNotChanged()
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        whenLookupObjects(Filter());
        thenLookupSucceded();
        andLookupResultMatches(Filter(), analyticsDataPackets());
    }

    void thenDataIsExpected()
    {
        Filter filter;
        filter.timePeriod.setStartTime(m_oldestAvailableDataTimestamp);
        filter.timePeriod.durationMs = QnTimePeriod::kInfiniteDuration;

        for (;;)
        {
            whenLookupObjects(Filter());
            thenLookupSucceded();
            if (isLookupResultEquals(filter, analyticsDataPackets()))
                return;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

private:
    QnUuid m_deviceId;
    std::chrono::milliseconds m_oldestAvailableDataTimestamp;

    void SetUp() override
    {
        base_type::SetUp();

        if (HasFatalFailure())
            return;

        m_deviceId = QnUuid::createUuid();
        setAllowedDeviceIds({{ m_deviceId }});

        setAllowedTimeRange(
            std::chrono::system_clock::now() - std::chrono::hours(24),
            std::chrono::system_clock::now());

        auto analyticsPackets = generateEventsByCriteria();
        saveAnalyticsDataPackets(std::move(analyticsPackets));
    }

    qint64 getMaxTimestamp() const
    {
        const std::vector<common::metadata::DetectionMetadataPacketPtr>&
            packets = analyticsDataPackets();
        auto maxElement = std::max_element(
            packets.begin(), packets.end(),
            [](const common::metadata::DetectionMetadataPacketPtr& left,
                const common::metadata::DetectionMetadataPacketPtr& right)
            {
                return left->timestampUsec < right->timestampUsec;
            });

        return (*maxElement)->timestampUsec;
    }

    qint64 getMinTimestamp() const
    {
        const std::vector<common::metadata::DetectionMetadataPacketPtr>&
            packets = analyticsDataPackets();
        auto maxElement = std::min_element(
            packets.begin(), packets.end(),
            [](const common::metadata::DetectionMetadataPacketPtr& left,
                const common::metadata::DetectionMetadataPacketPtr& right)
            {
                return left->timestampUsec < right->timestampUsec;
            });

        return (*maxElement)->timestampUsec;
    }
};

TEST_F(AnalyticsEventsStorageCleanup, removing_all_data)
{
    whenRemoveEventsUpToLatestEventTimestamp();
    thenStorageIsEmpty();
}

TEST_F(AnalyticsEventsStorageCleanup, removing_data_up_to_a_random_available_timestamp)
{
    whenRemoveEventsUpToARandomAvailableTimestamp();
    thenDataIsExpected();
}

TEST_F(AnalyticsEventsStorageCleanup, removing_data_up_to_minimal_available_timestamp)
{
    whenRemoveEventsUpToEarlisestEventTimestamp();
    thenDataIsNotChanged();
}

} // namespace test
} // namespace storage
} // namespace analytics
} // namespace nx
