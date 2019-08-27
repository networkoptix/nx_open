#include <memory>
#include <optional>

#include <gtest/gtest.h>

#include <nx/utils/random.h>
#include <nx/utils/std/algorithm.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/std/future.h>
#include <nx/utils/test_support/test_with_temporary_directory.h>
#include <nx/utils/thread/sync_queue.h>

#include <analytics/db/config.h>

#include <nx/vms/server/analytics/db/analytics_db.h>
#include <nx/vms/server/analytics/db/object_track_searcher.h>
#include <nx/vms/server/analytics/db/serializers.h>

#include "attribute_dictionary.h"

namespace nx::analytics::db::test {

class AnalyticsDb:
    public nx::utils::test::TestWithTemporaryDirectory,
    public ::testing::Test
{
public:
    AnalyticsDb():
        nx::utils::test::TestWithTemporaryDirectory("analytics_db", QString()),
        m_allowedTimeRange(
            std::chrono::system_clock::from_time_t(0),
            std::chrono::system_clock::from_time_t(0))
    {
        m_settings.path = nx::utils::test::TestWithTemporaryDirectory::testDataDir();
        m_settings.dbConnectionOptions.dbName = "objects.sqlite";
        m_settings.dbConnectionOptions.driverType = nx::sql::RdbmsDriverType::sqlite;
        m_settings.dbConnectionOptions.maxConnectionCount = 17;

        m_attributeDictionary.initialize(5, 2);

        for (int i = 0; i < 3; ++i)
            m_allowedDeviceIds.push_back(QnUuid::createUuid());
    }

protected:
    virtual void SetUp() override
    {
        ASSERT_TRUE(initializeStorage());
    }

    common::metadata::ObjectMetadataPacketPtr whenSaveEvent()
    {
        auto packet = generateRandomPackets(1).front();
        whenIssueSavePacket(packet);
        flushData();
        return packet;
    }

    void whenSaveMultipleEventsConcurrently()
    {
        constexpr int packetCount = 101;

        generateRandomPackets(packetCount);

        for (const auto& packet: m_analyticsDataPackets)
            whenIssueSavePacket(packet);

        flushData();
    }

    void whenSaveObjectTrackContainingBestShot()
    {
        std::vector<nx::common::metadata::ObjectMetadataPacketPtr> packets;

        packets.push_back(generateRandomPacket());
        packets.push_back(std::make_shared<nx::common::metadata::ObjectMetadataPacket>(
            *packets.back()));

        packets.back()->objectMetadataList.front().bestShot = true;

        saveAnalyticsDataPackets(std::move(packets));
    }

    void whenIssueSavePacket(common::metadata::ConstObjectMetadataPacketPtr packet)
    {
        m_eventsStorage->save(packet);
    }

    void whenRestartStorage()
    {
        m_eventsStorage.reset();
        ASSERT_TRUE(initializeStorage());
    }

    void whenLookupObjectTracks(const Filter& filter)
    {
        m_eventsStorage->lookup(
            filter,
            [this](
                ResultCode resultCode,
                std::vector<ObjectTrack> tracksFound)
            {
                m_lookupResultQueue.push(LookupResult{resultCode, std::move(tracksFound)});
            });
    }

    void thenAllEventsCanBeRead()
    {
        auto filter = buildEmptyFilter();

        whenLookupObjectTracks(filter);

        thenLookupSucceded();
        andLookupResultMatches(filter, m_analyticsDataPackets);
    }

    void andLookupResultMatches(
        const Filter& filter,
        const std::vector<common::metadata::ObjectMetadataPacketPtr>& expected)
    {
        andLookupResultEquals(calculateExpectedResult(filter, expected));
    }

    bool isLookupResultEquals(
        const Filter& filter,
        const std::vector<common::metadata::ObjectMetadataPacketPtr>& expected)
    {
        return calculateExpectedResult(filter, expected) == m_prevLookupResult->tracksFound;
    }

    void thenLookupSucceded()
    {
        m_prevLookupResult = m_lookupResultQueue.pop();
        ASSERT_EQ(ResultCode::ok, m_prevLookupResult->resultCode);
    }

    void andLookupResultEquals(
        const std::vector<ObjectTrack>& expected)
    {
        ASSERT_EQ(expected, m_prevLookupResult->tracksFound);
    }

    std::vector<ObjectTrack> toObjectMetadataPackets(
        const std::vector<common::metadata::ObjectMetadataPacketPtr>& objectMetadataPackets) const
    {
        std::vector<ObjectTrack> objectTracks;

        convertPacketsToObjectTracks(objectMetadataPackets, &objectTracks);
        groupTracks(&objectTracks);
        removeDuplicateAttributes(&objectTracks);

        return objectTracks;
    }

    std::vector<ObjectTrack> filterObjectTracks(
        std::vector<ObjectTrack> tracks,
        const Filter& filter)
    {
        for (auto trackIter = tracks.begin(); trackIter != tracks.end(); )
        {
            if (!ObjectTrackSearcher::satisfiesFilter(filter, *trackIter))
            {
                trackIter = tracks.erase(trackIter);
                continue;
            }

            for (auto objectPositionIter = trackIter->objectPositionSequence.begin();
                objectPositionIter != trackIter->objectPositionSequence.end();
                )
            {
                // NOTE: If object position does not satisfy the region filter -
                // it is still returned with the matched object.
                // But, if position does not specify time range - it is dropped.
                // That's a functional requirement.
                if (satisfiesCommonConditions(filter, *objectPositionIter))
                    ++objectPositionIter;
                else
                    objectPositionIter = trackIter->objectPositionSequence.erase(objectPositionIter);
            }

            if (trackIter->objectPositionSequence.empty())
                trackIter = tracks.erase(trackIter);
            else
                ++trackIter;
        }

        if (filter.maxObjectTracksToSelect > 0 && (int)tracks.size() > filter.maxObjectTracksToSelect)
            tracks.erase(tracks.begin() + filter.maxObjectTracksToSelect, tracks.end());

        if (filter.maxObjectTrackSize > 0)
        {
            for (auto& track: tracks)
            {
                if ((int) track.objectPositionSequence.size() > filter.maxObjectTrackSize)
                {
                    track.objectPositionSequence.erase(
                        track.objectPositionSequence.begin() + filter.maxObjectTrackSize,
                        track.objectPositionSequence.end());
                }
            }
        }

        return tracks;
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

    std::vector<common::metadata::ObjectMetadataPacketPtr> filterPackets(
        const Filter& filter,
        std::vector<common::metadata::ObjectMetadataPacketPtr> packets)
    {
        std::vector<common::metadata::ObjectMetadataPacketPtr> filteredPackets;
        std::copy_if(
            packets.begin(),
            packets.end(),
            std::back_inserter(filteredPackets),
            [this, &filter](const common::metadata::ConstObjectMetadataPacketPtr& packet)
            {
                return satisfiesFilter(filter, *packet);
            });

        return filteredPackets;
    }

    std::vector<common::metadata::ObjectMetadataPacketPtr> sortPacketsByTimestamp(
        std::vector<common::metadata::ObjectMetadataPacketPtr> packets,
        Qt::SortOrder sortOrder)
    {
        using namespace common::metadata;

        std::sort(
            packets.begin(), packets.end(),
            [sortOrder](
                const ConstObjectMetadataPacketPtr& leftPacket,
                const ConstObjectMetadataPacketPtr& rightPacket)
            {
                const auto& leftValue =
                    std::tie(leftPacket->timestampUs, leftPacket->durationUs);
                const auto& rightValue =
                    std::tie(rightPacket->timestampUs, rightPacket->durationUs);

                return sortOrder == Qt::AscendingOrder
                    ? leftValue < rightValue
                    : leftValue > rightValue;
            });

        // Sorting object metadata lists in packets by trackId.
        for (auto& packet: packets)
        {
            std::sort(
                packet->objectMetadataList.begin(), packet->objectMetadataList.end(),
                [sortOrder](
                    const nx::common::metadata::ObjectMetadata& left,
                    const nx::common::metadata::ObjectMetadata& right)
                {
                    return sortOrder == Qt::AscendingOrder
                        ? left.trackId < right.trackId
                        : left.trackId > right.trackId;
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

    std::vector<common::metadata::ObjectMetadataPacketPtr> generateEventsByCriteria()
    {
        using namespace std::chrono;

        const int eventsPerDevice = 11;

        std::vector<common::metadata::ObjectMetadataPacketPtr> analyticsDataPackets;
        for (const auto& deviceId: m_allowedDeviceIds)
        {
            for (int i = 0; i < eventsPerDevice; ++i)
            {
                auto packet = generateRandomPacket(&m_attributeDictionary);
                packet->deviceId = deviceId;
                if (m_allowedTimeRange.second > m_allowedTimeRange.first)
                {
                    packet->timestampUs = nx::utils::random::number<qint64>(
                        duration_cast<milliseconds>(
                            m_allowedTimeRange.first.time_since_epoch()).count(),
                        duration_cast<milliseconds>(
                            m_allowedTimeRange.second.time_since_epoch()).count())
                        * 1000;
                }

                analyticsDataPackets.push_back(packet);
            }
        }

        // Fallback, if conditions resulted in no packets.
        if (analyticsDataPackets.empty())
        {
            for (int i = 0; i < 101; ++i)
                analyticsDataPackets.push_back(generateRandomPacket());
        }

        std::sort(
            analyticsDataPackets.begin(), analyticsDataPackets.end(),
            [](const auto& left, const auto& right)
            {
                return left->timestampUs < right->timestampUs;
            });

        return analyticsDataPackets;
    }

    void saveAnalyticsDataPackets(
        std::vector<common::metadata::ObjectMetadataPacketPtr> analyticsDataPackets)
    {
        std::copy(
            analyticsDataPackets.begin(), analyticsDataPackets.end(),
            std::back_inserter(m_analyticsDataPackets));

        std::for_each(analyticsDataPackets.begin(), analyticsDataPackets.end(),
            [this](const auto& packet) { whenIssueSavePacket(packet); });

        flushData();
    }

    AbstractEventsStorage& eventsStorage()
    {
        return *m_eventsStorage;
    }

    const std::vector<common::metadata::ObjectMetadataPacketPtr>& analyticsDataPackets() const
    {
        return m_analyticsDataPackets;
    }

    void aggregateAnalyticsDataPacketsByTimestamp()
    {
        if (m_analyticsDataPackets.empty())
            return;

        m_analyticsDataPackets = sortPacketsByTimestamp(
            std::exchange(m_analyticsDataPackets, {}),
            Qt::SortOrder::AscendingOrder);

        auto prevPacketIter = m_analyticsDataPackets.begin();
        for (auto it = std::next(prevPacketIter);
            it != m_analyticsDataPackets.end();
            )
        {
            if ((*it)->timestampUs == (*prevPacketIter)->timestampUs &&
                (*it)->deviceId == (*prevPacketIter)->deviceId)
            {
                std::move(
                    (*it)->objectMetadataList.begin(), (*it)->objectMetadataList.end(),
                    std::back_inserter((*prevPacketIter)->objectMetadataList));

                (*prevPacketIter)->durationUs =
                    std::max((*it)->durationUs, (*prevPacketIter)->durationUs);
                it = m_analyticsDataPackets.erase(it);
            }
            else
            {
                prevPacketIter = it;
                ++it;
            }
        }
    }

    common::metadata::ObjectMetadataPacketPtr generateRandomPacket(
        AttributeDictionary* attributeDictionary = nullptr)
    {
        auto packet = test::generateRandomPacket(1, attributeDictionary);

        if (m_fixedObjectId)
        {
            for (auto& object: packet->objectMetadataList)
            {
                object.trackId = *m_fixedObjectId;
                object.typeId = m_fixedObjectId->toSimpleString();
            }
        }

        if (m_lastTimestamp)
        {
            packet->timestampUs =
                *m_lastTimestamp + nx::utils::random::number<qint64>(1, 60000000);
        }

        NX_ASSERT(!m_allowedDeviceIds.empty());
        packet->deviceId = nx::utils::random::choice(m_allowedDeviceIds);

        m_lastTimestamp = packet->timestampUs;

        return packet;
    }

    std::vector<common::metadata::ObjectMetadataPacketPtr>
        generateRandomPackets(int count)
    {
        std::vector<common::metadata::ObjectMetadataPacketPtr> packets;

        for (int i = 0; i < count; ++i)
            packets.push_back(generateRandomPacket());

        std::copy(packets.begin(), packets.end(), std::back_inserter(m_analyticsDataPackets));

        std::sort(
            m_analyticsDataPackets.begin(), m_analyticsDataPackets.end(),
            [](const auto& left, const auto& right)
            {
                return left->timestampUs < right->timestampUs;
            });

        return packets;
    }

    Filter buildEmptyFilter()
    {
        return Filter();
    }

    const Settings& settings() const
    {
        return m_settings;
    }

    Settings& settings()
    {
        return m_settings;
    }

    void setAllowedObjectId(const QnUuid& trackId)
    {
        m_fixedObjectId = trackId;
    }

private:
    struct LookupResult
    {
        ResultCode resultCode;
        std::vector<ObjectTrack> tracksFound;
    };

    std::unique_ptr<AbstractEventsStorage> m_eventsStorage;
    Settings m_settings;
    std::vector<common::metadata::ObjectMetadataPacketPtr> m_analyticsDataPackets;
    nx::utils::SyncQueue<LookupResult> m_lookupResultQueue;
    std::optional<LookupResult> m_prevLookupResult;
    std::optional<qint64> m_lastTimestamp;
    std::optional<QnUuid> m_fixedObjectId;

    std::vector<QnUuid> m_allowedDeviceIds;
    std::pair<std::chrono::system_clock::time_point, std::chrono::system_clock::time_point>
        m_allowedTimeRange;
    AttributeDictionary m_attributeDictionary;

    bool initializeStorage()
    {
        m_eventsStorage = EventsStorageFactory::instance().create(nullptr);
        return m_eventsStorage->initialize(m_settings);
    }

    void flushData()
    {
        std::promise<ResultCode> done;
        m_eventsStorage->flush([&done](ResultCode result) { done.set_value(result); });
        ASSERT_EQ(ResultCode::ok, done.get_future().get());
    }

    std::vector<nx::analytics::db::ObjectTrack> calculateExpectedResult(
        const Filter& filter,
        const std::vector<common::metadata::ObjectMetadataPacketPtr>& packets)
    {
        auto objectMetadataPackets = toObjectMetadataPackets(packets);
        objectMetadataPackets = filterObjectTracksAndApplySortOrder(filter, std::move(objectMetadataPackets));

        // NOTE: Object bbox is stored in the DB with limited precision.
        // So, lowering precision to that in the DB.
        objectMetadataPackets = applyTrackBoxPrecision(std::move(objectMetadataPackets));

        return objectMetadataPackets;
    }

    std::vector<nx::analytics::db::ObjectTrack> filterObjectTracksAndApplySortOrder(
        const Filter& filter,
        std::vector<nx::analytics::db::ObjectTrack> objectTracks)
    {
        // First, sorting tracks in descending order, because we always filtering the most recent
        // tracks and filter.sortOrder is applied AFTER filtering.
        std::sort(
            objectTracks.begin(), objectTracks.end(),
            [&filter](const ObjectTrack& left, const ObjectTrack& right)
            {
                return left.objectPositionSequence.front().timestampUs
                    > right.objectPositionSequence.front().timestampUs;
            });

        auto filteredObjectTracks = filterObjectTracks(objectTracks, filter);

        std::sort(
            filteredObjectTracks.begin(), filteredObjectTracks.end(),
            [&filter](const ObjectTrack& left, const ObjectTrack& right)
            {
                if (filter.sortOrder == Qt::AscendingOrder)
                {
                    return left.objectPositionSequence.front().timestampUs
                        < right.objectPositionSequence.front().timestampUs;
                }
                else
                {
                    return left.objectPositionSequence.front().timestampUs
                        > right.objectPositionSequence.front().timestampUs;
                }
            });

        return filteredObjectTracks;
    }

    std::vector<nx::analytics::db::ObjectTrack> applyTrackBoxPrecision(
        std::vector<nx::analytics::db::ObjectTrack> tracks)
    {
        static const QSize kResolution(kCoordinatesPrecision, kCoordinatesPrecision);

        for (auto& track: tracks)
        {
            for (auto& position: track.objectPositionSequence)
            {
                position.boundingBox =
                    translate(
                        translate(position.boundingBox, kResolution),
                        kResolution);
            }
        }

        return tracks;
    }

    bool satisfiesFilter(
        const Filter& filter,
        const ObjectPosition& data)
    {
        if (!satisfiesCommonConditions(filter, data))
            return false;

        if (!filter.boundingBox)
            return true;

        return rectsIntersectToSearchPrecision(*filter.boundingBox, data.boundingBox);
    }

    bool satisfiesFilter(
        const Filter& filter,
        const common::metadata::ObjectMetadataPacket& data)
    {
        if (!filter.objectTrackId.isNull())
        {
            bool hasRequiredObject = false;
            for (const auto& objectMetadata: data.objectMetadataList)
                hasRequiredObject |= objectMetadata.trackId == filter.objectTrackId;
            if (!hasRequiredObject)
                return false;
        }

        if (filter.boundingBox)
        {
            bool intersects = false;
            for (const auto& objectMetadata: data.objectMetadataList)
            {
                intersects |= rectsIntersectToSearchPrecision(
                    *filter.boundingBox,
                    objectMetadata.boundingBox);
            }
            if (!intersects)
                return false;
        }

        if (!filter.objectTypeId.empty())
        {
            bool hasProperType = false;
            for (const auto& objectMetadata: data.objectMetadataList)
            {
                hasProperType |= std::find(
                    filter.objectTypeId.begin(), filter.objectTypeId.end(),
                    objectMetadata.typeId) != filter.objectTypeId.end();
            }
            if (!hasProperType)
                return false;
        }

        if (!filter.freeText.isEmpty())
        {
            bool isFilterSatisfied = false;
            for (const auto& objectMetadata: data.objectMetadataList)
            {
                isFilterSatisfied |=
                    ObjectTrackSearcher::matchAttributes(
                        objectMetadata.attributes, filter.freeText);
            }
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

        if (!filter.timePeriod.contains(data.timestampUs / kUsecInMs))
            return false;

        return true;
    }

    static void convertPacketsToObjectTracks(
        const std::vector<common::metadata::ObjectMetadataPacketPtr>& analyticsDataPackets,
        std::vector<ObjectTrack>* tracks)
    {
        for (const auto& packet: analyticsDataPackets)
        {
            for (const auto& objectMetadata: packet->objectMetadataList)
            {
                ObjectTrack track;
                track.id = objectMetadata.trackId;
                track.objectTypeId = objectMetadata.typeId;
                track.attributes = objectMetadata.attributes;
                track.deviceId = packet->deviceId;
                track.objectPositionSequence.push_back(ObjectPosition());
                ObjectPosition& objectPosition = track.objectPositionSequence.back();
                objectPosition.boundingBox = objectMetadata.boundingBox;
                objectPosition.timestampUs = packet->timestampUs;
                objectPosition.durationUs = packet->durationUs;
                objectPosition.deviceId = packet->deviceId;

                if (objectMetadata.bestShot)
                {
                    track.bestShot.timestampUs = packet->timestampUs;
                    track.bestShot.rect = objectMetadata.boundingBox;
                }

                tracks->push_back(std::move(track));
            }
        }
    }

    static void groupTracks(std::vector<ObjectTrack>* tracks)
    {
        std::sort(
            tracks->begin(), tracks->end(),
            [](const ObjectTrack& left, const ObjectTrack& right)
            {
                return left.id < right.id;
            });

        for (auto it = tracks->begin(); it != tracks->end();)
        {
            auto nextIter = std::next(it);
            if (nextIter == tracks->end())
                break;

            if (it->id == nextIter->id)
            {
                // Merging.
                it->firstAppearanceTimeUs =
                    std::min(it->firstAppearanceTimeUs, nextIter->firstAppearanceTimeUs);
                it->lastAppearanceTimeUs =
                    std::max(it->lastAppearanceTimeUs, nextIter->lastAppearanceTimeUs);
                std::move(
                    nextIter->objectPositionSequence.begin(), nextIter->objectPositionSequence.end(),
                    std::back_inserter(it->objectPositionSequence));
                std::move(
                    nextIter->attributes.begin(), nextIter->attributes.end(),
                    std::back_inserter(it->attributes));

                if (nextIter->bestShot.initialized())
                    it->bestShot = nextIter->bestShot;

                tracks->erase(nextIter);
            }
            else
            {
                ++it;
            }
        }

        for (auto& track: *tracks)
        {
            std::sort(
                track.objectPositionSequence.begin(), track.objectPositionSequence.end(),
                [](const ObjectPosition& left, const ObjectPosition& right)
                { return left.timestampUs < right.timestampUs; });

            if (!track.objectPositionSequence.empty())
            {
                track.firstAppearanceTimeUs =
                    (track.objectPositionSequence.begin()->timestampUs / kUsecInMs) * kUsecInMs;
                track.lastAppearanceTimeUs =
                    (track.objectPositionSequence.rbegin()->timestampUs / kUsecInMs) * kUsecInMs;
            }
        }
    }

    static void removeDuplicateAttributes(std::vector<ObjectTrack>* tracks)
    {
        for (auto& track: *tracks)
        {
            std::map<QString, QString> uniqueAttributes;
            for (const auto& attribute: track.attributes)
                uniqueAttributes[attribute.name] = attribute.value;
            track.attributes.clear();

            for (const auto& [name, value]: uniqueAttributes)
                track.attributes.push_back({name, value});
        }
    }
};

TEST_F(AnalyticsDb, event_saved_can_be_read_later)
{
    whenSaveEvent();
    whenRestartStorage();

    thenAllEventsCanBeRead();
}

TEST_F(AnalyticsDb, storing_multiple_events_concurrently)
{
    whenSaveMultipleEventsConcurrently();
    thenAllEventsCanBeRead();
}

TEST_F(AnalyticsDb, tracks_best_shot_is_saved_and_reported)
{
    whenSaveObjectTrackContainingBestShot();

    thenAllEventsCanBeRead();
}

//-------------------------------------------------------------------------------------------------

class AnalyticsDbMovingToNewPath:
    public AnalyticsDb
{
public:
    ~AnalyticsDbMovingToNewPath()
    {
        m_terminated = true;

        if (m_dbLoadThread.joinable())
            m_dbLoadThread.join();
    }

protected:
    void startFillingDbConcurrently()
    {
        m_dbLoadThread = std::thread(
            [this]()
            {
                while (!m_terminated)
                    eventsStorage().save(generateRandomPacket());
            });
    }

    void startConcurrentLookups()
    {
        static constexpr int kRequestBatchSize = 100;

        m_dbLoadThread = std::thread(
            [this]()
            {
                nx::utils::SyncQueue<int /*dummy*/> done;

                while (!m_terminated)
                {
                    for (int i = 0; i < kRequestBatchSize; ++i)
                        eventsStorage().lookup(Filter(), [&done](auto&&...) { done.push(0); });
                    for (int i = 0; i < kRequestBatchSize; ++i)
                        done.pop();
                }
            });
    }

    void whenChangeDbPath()
    {
        auto newSettings = settings();
        const auto newPath = nx::utils::test::TestWithTemporaryDirectory::testDataDir() + "/2/";
        ASSERT_TRUE(QDir().mkdir(newPath));
        newSettings.path = newPath;

        eventsStorage().initialize(newSettings);
    }

    void thenDbIsOperational()
    {
        whenLookupObjectTracks(buildEmptyFilter());
        thenLookupSucceded();
    }

private:
    std::thread m_dbLoadThread;
    bool m_terminated = false;
};

TEST_F(AnalyticsDbMovingToNewPath, moving_during_concurrent_load)
{
    startFillingDbConcurrently();
    whenChangeDbPath();
    thenDbIsOperational();
}

TEST_F(AnalyticsDbMovingToNewPath, moving_during_concurrent_lookups)
{
    startConcurrentLookups();
    whenChangeDbPath();
    thenDbIsOperational();
}

//-------------------------------------------------------------------------------------------------
// Basic Lookup condition.

class AnalyticsDbLookup:
    public AnalyticsDb
{
    using base_type = AnalyticsDb;

protected:
    virtual void SetUp() override
    {
        base_type::SetUp();

        if (HasFatalFailure())
            return;

        generateVariousEvents();

        m_filter = buildEmptyFilter();
    }

    void addRandomKnownDeviceIdToFilter()
    {
        ASSERT_FALSE(allowedDeviceIds().empty());

        m_filter.deviceIds.clear();
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

    void addRandomTrackIdToFilter()
    {
        const auto& randomPacket = nx::utils::random::choice(analyticsDataPackets());
        const auto& randomObject = nx::utils::random::choice(randomPacket->objectMetadataList);
        m_filter.objectTrackId = randomObject.trackId;
    }

    void addRandomObjectTypeIdToFilter()
    {
        const auto& randomPacket = nx::utils::random::choice(analyticsDataPackets());
        const auto& randomObject = nx::utils::random::choice(randomPacket->objectMetadataList);
        m_filter.objectTypeId.push_back(randomObject.typeId);
    }

    void addMaxObjectTracksLimitToFilter()
    {
        const auto filteredObjectCount = (int) filterObjectTracks(
            toObjectMetadataPackets(analyticsDataPackets()), m_filter).size();

        if (filteredObjectCount > 0)
        {
            m_filter.maxObjectTracksToSelect =
                nx::utils::random::number<int>(0, filteredObjectCount + 1);
        }
    }

    void addMaxTrackLengthLimitToFilter()
    {
        // TODO: #ak Currently, only 1 track element can be returned.
        m_filter.maxObjectTrackSize = 1;
    }

    void addRandomTextFoundInDataToFilter()
    {
        m_filter.freeText = attributeDictionary().getRandomText();
    }

    void addRandomTextPrefixFoundInDataToFilter()
    {
        QString text;
        for (int i = 0; i < 10; ++i)
        {
            text = attributeDictionary().getRandomText();
            if (text.size() >= 2)
                break;
        }

        m_filter.freeText = text.mid(0, text.size() / 2);
    }

    void addRandomUnknownText()
    {
        m_filter.freeText = QnUuid::createUuid().toSimpleString();
    }

    void invertTextFilterCase()
    {
        if (!m_effectiveLookupFilter)
            m_effectiveLookupFilter = m_filter;

        if (m_effectiveLookupFilter->freeText.front().isLower())
            m_effectiveLookupFilter->freeText = m_effectiveLookupFilter->freeText.toUpper();
        else
            m_effectiveLookupFilter->freeText = m_effectiveLookupFilter->freeText.toLower();
    }

    void addRandomBoundingBoxToFilter()
    {
        m_filter.boundingBox = QRectF(
            nx::utils::random::number<float>(0, 1),
            nx::utils::random::number<float>(0, 1),
            nx::utils::random::number<float>(0, 1),
            nx::utils::random::number<float>(0, 1));

        auto bottomRight = m_filter.boundingBox->bottomRight();
        if (bottomRight.x() > 1.0)
            bottomRight.setX(1.0);
        if (bottomRight.y() > 1.0)
            bottomRight.setY(1.0);

        m_filter.boundingBox->setBottomRight(bottomRight);
    }

    void givenEmptyFilter()
    {
        m_filter = buildEmptyFilter();
    }

    void givenRandomFilter()
    {
        const auto& randomPacket = nx::utils::random::choice(analyticsDataPackets());

        m_filter.deviceIds.push_back(randomPacket->deviceId);

        if (nx::utils::random::number<bool>())
        {
            addRandomNonEmptyTimePeriodToFilter();
            if (!m_filter.timePeriod.contains(randomPacket->timestampUs / kUsecInMs))
            {
                m_filter.timePeriod.addPeriod(
                    QnTimePeriod(randomPacket->timestampUs / kUsecInMs, 1));
            }
        }

        if (nx::utils::random::number<bool>())
            m_filter.objectTrackId = randomPacket->objectMetadataList.front().trackId;

        if (nx::utils::random::number<bool>())
        {
            addRandomObjectTypeIdToFilter();
            if (!nx::utils::contains(
                m_filter.objectTypeId, randomPacket->objectMetadataList.front().typeId))
            {
                m_filter.objectTypeId.push_back(randomPacket->objectMetadataList.front().typeId);
            }
        }

        if (nx::utils::random::number<bool>())
            addMaxObjectTracksLimitToFilter();

        if (nx::utils::random::number<bool>())
        {
            addRandomBoundingBoxToFilter();
            if (!m_filter.boundingBox->intersects(randomPacket->objectMetadataList.front().boundingBox))
            {
                m_filter.boundingBox =
                    m_filter.boundingBox->united(randomPacket->objectMetadataList.front().boundingBox);
            }
        }

        if (nx::utils::random::number<bool>())
        {
            addRandomTextFoundInDataToFilter();
        }
    }

    void givenRandomFilterWithMultipleDeviceIds()
    {
        givenRandomFilter();

        m_filter.deviceIds.clear();
        for (const auto& deviceId: allowedDeviceIds())
            m_filter.deviceIds.push_back(deviceId);

        m_filter.objectTrackId = QnUuid();
    }

    void givenObjectWithLongTrack()
    {
        using namespace std::chrono;

        const auto startTime = system_clock::from_time_t(
            analyticsDataPackets().back()->timestampUs / 1000000) + hours(1);
        setAllowedTimeRange(startTime, startTime + hours(24));

        m_specificObjectTrackId = QnUuid::createUuid();
        const auto deviceId = QnUuid::createUuid();
        auto analyticsDataPackets = generateEventsByCriteria();

        qint64 objectTrackStartTime = std::numeric_limits<qint64>::max();
        qint64 objectTrackEndTime = std::numeric_limits<qint64>::min();

        for (auto& packet: analyticsDataPackets)
        {
            objectTrackStartTime = std::min(objectTrackStartTime, packet->timestampUs);
            objectTrackEndTime =
                std::max(objectTrackEndTime, packet->timestampUs + packet->durationUs);
            packet->deviceId = deviceId;

            for (auto& objectMetadata: packet->objectMetadataList)
            {
                objectMetadata.trackId = m_specificObjectTrackId;
                objectMetadata.typeId = m_specificObjectTrackId.toString();
            }
        }

        m_specificObjectTrackTimePeriod.setStartTime(
            duration_cast<milliseconds>(microseconds(objectTrackStartTime)));
        m_specificObjectTrackTimePeriod.setDuration(
            duration_cast<milliseconds>(microseconds(objectTrackEndTime - objectTrackStartTime)));

        saveAnalyticsDataPackets(analyticsDataPackets);
    }

    void setSortOrder(Qt::SortOrder sortOrder)
    {
        m_filter.sortOrder = sortOrder;
    }

    void whenLookupObjectTracks()
    {
        if (!m_effectiveLookupFilter)
            m_effectiveLookupFilter = m_filter;

        base_type::whenLookupObjectTracks(*m_effectiveLookupFilter);
    }

    void whenLookupByEmptyFilter()
    {
        givenEmptyFilter();
        whenLookupObjectTracks();
    }

    void whenLookupByRandomKnownDeviceId()
    {
        addRandomKnownDeviceIdToFilter();
        whenLookupObjectTracks();
    }

    void whenLookupByRandomNonEmptyTimePeriod()
    {
        addRandomNonEmptyTimePeriodToFilter();
        whenLookupObjectTracks();
    }

    void whenLookupByEmptyTimePeriod()
    {
        addEmptyTimePeriodToFilter();
        whenLookupObjectTracks();
    }

    void whenLookupByRandomTrackId()
    {
        addRandomTrackIdToFilter();
        whenLookupObjectTracks();
    }

    void whenLookupByRandomObjectTypeId()
    {
        addRandomObjectTypeIdToFilter();
        whenLookupObjectTracks();
    }

    void whenLookupWithMaxObjectTracksLimit()
    {
        addMaxObjectTracksLimitToFilter();
        whenLookupObjectTracks();
    }

    void whenLookupWithMaxTrackLengthLimit()
    {
        m_filter.objectTrackId = m_specificObjectTrackId;
        addMaxTrackLengthLimitToFilter();
        //m_filter.maxTrackSize = 1;

        whenLookupObjectTracks();
    }

    void whenLookupByRandomTextFoundInData()
    {
        addRandomTextFoundInDataToFilter();
        whenLookupObjectTracks();
    }

    void whenLookupByRandomBoundingBox()
    {
        addRandomBoundingBoxToFilter();
        whenLookupObjectTracks();
    }

    void whenLookupByRandomNonEmptyTimePeriodCoveringPartOfTrack()
    {
        m_filter.objectTrackId = m_specificObjectTrackId;
        m_filter.timePeriod.setStartTime(
            m_specificObjectTrackTimePeriod.startTime() + m_specificObjectTrackTimePeriod.duration() / 3);
        m_filter.timePeriod.setDuration(m_specificObjectTrackTimePeriod.duration() / 3);

        whenLookupObjectTracks();
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
    std::optional<Filter> m_effectiveLookupFilter;
    QnUuid m_specificObjectTrackId;
    QnTimePeriod m_specificObjectTrackTimePeriod;

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

TEST_F(AnalyticsDbLookup, empty_filter_matches_all_tracks)
{
    whenLookupByEmptyFilter();
    thenResultMatchesExpectations();
}

TEST_F(AnalyticsDbLookup, lookup_by_deviceId)
{
    whenLookupByRandomKnownDeviceId();
    thenResultMatchesExpectations();
}

TEST_F(AnalyticsDbLookup, lookup_by_non_empty_time_period)
{
    whenLookupByRandomNonEmptyTimePeriod();
    thenResultMatchesExpectations();
}

TEST_F(AnalyticsDbLookup, lookup_by_empty_time_period)
{
    whenLookupByEmptyTimePeriod();
    thenResultMatchesExpectations();
}

TEST_F(AnalyticsDbLookup, filtering_part_of_track_by_time_period)
{
    givenObjectWithLongTrack();
    whenLookupByRandomNonEmptyTimePeriodCoveringPartOfTrack();
    thenResultMatchesExpectations();
}

TEST_F(AnalyticsDbLookup, max_object_tracks_limit)
{
    whenLookupWithMaxObjectTracksLimit();
    thenResultMatchesExpectations();
}

TEST_F(AnalyticsDbLookup, max_track_length_limit)
{
    givenObjectWithLongTrack();
    whenLookupWithMaxTrackLengthLimit();
    thenResultMatchesExpectations();
}

TEST_F(AnalyticsDbLookup, sort_lookup_result_by_timestamp_ascending)
{
    givenRandomFilter();
    setSortOrder(Qt::SortOrder::AscendingOrder);

    whenLookupObjectTracks();

    thenResultMatchesExpectations();
}

TEST_F(AnalyticsDbLookup, sort_lookup_result_by_timestamp_descending)
{
    givenRandomFilter();
    setSortOrder(Qt::SortOrder::DescendingOrder);

    whenLookupObjectTracks();

    thenResultMatchesExpectations();
}

TEST_F(AnalyticsDbLookup, full_text_search)
{
    whenLookupByRandomTextFoundInData();
    thenResultMatchesExpectations();
}

TEST_F(AnalyticsDbLookup, full_text_search_by_prefix)
{
    addRandomTextPrefixFoundInDataToFilter();
    whenLookupObjectTracks();

    thenResultMatchesExpectations();
}

TEST_F(AnalyticsDbLookup, lookup_by_unknown_text_produces_no_objects)
{
    addRandomUnknownText();
    whenLookupObjectTracks();
    thenResultMatchesExpectations();
}

TEST_F(AnalyticsDbLookup, full_text_search_case_insensitive)
{
    addRandomTextFoundInDataToFilter();
    invertTextFilterCase();
    whenLookupObjectTracks();

    thenResultMatchesExpectations();
}

TEST_F(AnalyticsDbLookup, lookup_by_bounding_box)
{
    whenLookupByRandomBoundingBox();
    thenResultMatchesExpectations();
}

TEST_F(AnalyticsDbLookup, lookup_by_objectTrackId)
{
    whenLookupByRandomTrackId();
    thenResultMatchesExpectations();
}

TEST_F(AnalyticsDbLookup, lookup_by_objectTypeId)
{
    whenLookupByRandomObjectTypeId();
    thenResultMatchesExpectations();
}

// TEST_F(AnalyticsDbLookup, lookup_by_multiple_objectTypeId)

TEST_F(AnalyticsDbLookup, lookup_stress_test)
{
    givenRandomFilter();
    setSortOrder(
        nx::utils::random::number<bool>()
        ? Qt::SortOrder::AscendingOrder
        : Qt::SortOrder::DescendingOrder);

    whenLookupObjectTracks();

    thenResultMatchesExpectations();
}

TEST_F(AnalyticsDbLookup, quering_data_from_multiple_cameras)
{
    givenRandomFilterWithMultipleDeviceIds();
    whenLookupObjectTracks();
    thenResultMatchesExpectations();
}

//-------------------------------------------------------------------------------------------------

class AnalyticsDbLookupTrackAddedWithLargeTimeGap:
    public AnalyticsDbLookup
{
    using base_type = AnalyticsDbLookup;

protected:
    virtual void SetUp() override
    {
        settings().maxCachedObjectLifeTime = std::chrono::milliseconds(1);

        base_type::SetUp();
    }
};

TEST_F(
    AnalyticsDbLookupTrackAddedWithLargeTimeGap,
    data_saved_with_a_large_delay_is_still_found)
{
    const auto deviceId = QnUuid::createUuid();
    setAllowedDeviceIds({deviceId});
    setAllowedObjectId(QnUuid::createUuid());

    whenSaveEvent();
    std::this_thread::sleep_for(settings().maxCachedObjectLifeTime * 10);
    const auto packet = whenSaveEvent();

    filter().boundingBox = packet->objectMetadataList.front().boundingBox;
    filter().deviceIds = {deviceId};
    filter().maxObjectTrackSize = 0;

    whenLookupObjectTracks();
    thenResultMatchesExpectations();
}

//-------------------------------------------------------------------------------------------------
// Cursor.

class AnalyticsDbCursor:
    public AnalyticsDbLookup
{
    using base_type = AnalyticsDbLookup;

public:
    AnalyticsDbCursor()
    {
        filter().sortOrder = Qt::AscendingOrder;
        filter().maxObjectTrackSize = 0;
    }

protected:
    void givenObjectMetadataPacketsWithSameTimestamp()
    {
        using namespace std::chrono;

        const auto startTime = system_clock::from_time_t(
            analyticsDataPackets().back()->timestampUs / 1000000) + hours(1);
        setAllowedTimeRange(startTime, startTime + hours(24));

        auto newPackets = generateEventsByCriteria();

        std::for_each(
            std::next(newPackets.begin()), newPackets.end(),
            [existingPacket = newPackets.front()](auto& packet)
            {
                packet->deviceId = existingPacket->deviceId;
                packet->timestampUs = existingPacket->timestampUs;
            });

        saveAnalyticsDataPackets(std::move(newPackets));
        aggregateAnalyticsDataPacketsByTimestamp();
    }

    void givenObjectMetadataPacketsWithInterleavedTracks()
    {
        using namespace std::chrono;

        constexpr int objectCount = 3;

        const auto startTime = system_clock::from_time_t(
            analyticsDataPackets().back()->timestampUs / 1000000) + hours(1);
        setAllowedTimeRange(startTime, startTime + hours(24));

        auto newPackets = generateEventsByCriteria();

        const auto deviceId = QnUuid::createUuid();
        std::vector<QnUuid> trackIds(objectCount, QnUuid());
        std::generate(trackIds.begin(), trackIds.end(), &QnUuid::createUuid);

        for (auto& packet: newPackets)
        {
            packet->deviceId = deviceId;

            while (packet->objectMetadataList.size() < trackIds.size())
                packet->objectMetadataList.push_back(packet->objectMetadataList.front());

            for (std::size_t i = 0; i < packet->objectMetadataList.size(); ++i)
            {
                packet->objectMetadataList[i].trackId = trackIds[i];
                packet->objectMetadataList[i].typeId = trackIds[i].toSimpleString();
            }
        }

        saveAnalyticsDataPackets(std::move(newPackets));

        filter().deviceIds = {deviceId};
    }

    void givenRandomCursorFilter()
    {
        givenRandomFilter();

        if (filter().deviceIds.size() > 1)
            filter().deviceIds.erase(std::next(filter().deviceIds.begin()), filter().deviceIds.end());

        filter().objectTypeId.clear();
        filter().objectTrackId = QnUuid();
        filter().boundingBox = std::nullopt;
        filter().freeText.clear();
    }

    void whenReadDataUsingCursor()
    {
        m_packetsRead.clear();

        // NOTE: Currently, the cursor is forward only.
        setSortOrder(Qt::AscendingOrder);

        // NOTE: Limiting object's track when using cursor does not make any sense.
        // It will just skip data.
        filter().maxObjectTrackSize = 0;

        if (!m_cursor)
            createCursor();

        readAllDataFromCursor();
    }

    void whenCreateCursor()
    {
        eventsStorage().createLookupCursor(
            filter(),
            [this](auto&&... args) { saveCursor(std::forward<decltype(args)>(args)...); });
    }

    void thenCursorIsCreated()
    {
        m_cursor = m_createdCursorsQueue.pop();
        ASSERT_NE(nullptr, m_cursor);
    }

    void thenResultMatchesExpectations()
    {
        auto expected = filterPackets(filter(), analyticsDataPackets());
        expected = sortPacketsByTimestamp(
            std::move(expected),
            filter().sortOrder);

        if (filter().maxObjectTracksToSelect > 0 && (int)expected.size() > filter().maxObjectTracksToSelect)
            expected.erase(expected.begin() + filter().maxObjectTracksToSelect, expected.end());

        sortObjectMetadataById(&expected);

        // TODO: Currently, attribute change history is not preserved.
        removeLabels(&expected);

        std::vector<common::metadata::ObjectMetadataPacketPtr> actual;
        std::transform(
            m_packetsRead.begin(), m_packetsRead.end(),
            std::back_inserter(actual),
            [](const auto& constPacket)
            {
                return std::make_shared<common::metadata::ObjectMetadataPacket>(*constPacket);
            });
        sortObjectMetadataById(&actual);
        removeLabels(&actual);

        assertEqual(expected, actual);
    }

    void andObjectMetadataWithSameTimestampAreDeliveredInSinglePacket()
    {
        // TODO
    }

    void createCursor()
    {
        whenCreateCursor();
        thenCursorIsCreated();
    }

    void addMoreData()
    {
        saveAnalyticsDataPackets(generateEventsByCriteria());
    }

private:
    std::vector<common::metadata::ConstObjectMetadataPacketPtr> m_packetsRead;
    nx::utils::SyncQueue<std::shared_ptr<AbstractCursor>> m_createdCursorsQueue;
    std::shared_ptr<AbstractCursor> m_cursor;

    void readAllDataFromCursor()
    {
        while (auto packet = m_cursor->next())
            m_packetsRead.push_back(packet);
    }

    void saveCursor(
        ResultCode /*resultCode*/,
        std::shared_ptr<AbstractCursor> cursor)
    {
        m_createdCursorsQueue.push(cursor);
    }

    void sortObjectMetadataById(
        std::vector<common::metadata::ObjectMetadataPacketPtr>* packets)
    {
        for (auto& packet: *packets)
        {
            std::sort(
                packet->objectMetadataList.begin(), packet->objectMetadataList.end(),
                [](const auto& left, const auto& right) { return left.trackId < right.trackId; });
        }
    }

    void removeLabels(
        std::vector<common::metadata::ObjectMetadataPacketPtr>* packets)
    {
        for (auto& packet: *packets)
        {
            for (auto& objectMetadata: packet->objectMetadataList)
                objectMetadata.attributes.clear();
        }
    }
};

TEST_F(AnalyticsDbCursor, reads_all_available_data)
{
    whenReadDataUsingCursor();
    thenResultMatchesExpectations();
}

TEST_F(AnalyticsDbCursor, reads_all_matched_data)
{
    givenRandomCursorFilter();

    whenReadDataUsingCursor();
    thenResultMatchesExpectations();
}

TEST_F(AnalyticsDbCursor, aggregates_object_metadata_with_same_timestamp)
{
    givenObjectMetadataPacketsWithSameTimestamp();

    whenReadDataUsingCursor();

    thenResultMatchesExpectations();
    andObjectMetadataWithSameTimestampAreDeliveredInSinglePacket();
}

TEST_F(AnalyticsDbCursor, interleaved_object_tracks_are_reported_interleaved)
{
    givenObjectMetadataPacketsWithInterleavedTracks();
    whenReadDataUsingCursor();
    thenResultMatchesExpectations();
}

TEST_F(AnalyticsDbCursor, recreated_cursor_is_able_to_access_new_data)
{
    createCursor();

    addMoreData();
    whenReadDataUsingCursor();

    addMoreData();
    createCursor();

    whenReadDataUsingCursor();
    thenResultMatchesExpectations();
}

// TEST_F(AnalyticsDbCursor, attribute_change_history_is_preserved)

//-------------------------------------------------------------------------------------------------
// Time periods lookup.

class AnalyticsDbTimePeriodsLookup:
    public AnalyticsDbLookup
{
    using base_type = AnalyticsDbLookup;

protected:
    void givenAggregationPeriodEqualToTheWholeArchivePeriod()
    {
        const auto allTimePeriods = expectedLookupResult();
        if (!allTimePeriods.isEmpty())
        {
            const auto archiveLength =
                allTimePeriods.back().endTime() - allTimePeriods.front().startTime();

            m_lookupOptions.detailLevel = archiveLength;
        }
    }

    void givenFilterWithTimePeriod()
    {
        const auto allTimePeriods = expectedLookupResult();
        if (!allTimePeriods.isEmpty())
        {
            const auto archiveLength =
                allTimePeriods.back().endTime() - allTimePeriods.front().startTime();

            filter().timePeriod.setStartTime(allTimePeriods.front().startTime() + archiveLength / 3);
            filter().timePeriod.setDuration(archiveLength * 2 / 3);
        }
    }

    void givenRandomLookupOptions()
    {
        m_lookupOptions.detailLevel = std::chrono::milliseconds(
            nx::utils::random::number<int>(1, 1000000));
    }

    void whenLookupTimePeriods()
    {
        filter().sortOrder = Qt::AscendingOrder;

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
        using namespace std::chrono;

        auto expected = expectedLookupResult();
        auto actualTimePeriods = std::get<1>(m_prevLookupResult);

        // NOTE: Time period fetch precision is the following:
        // start time - a second
        // duration - kTrackAggregationPeriod.

        roundTimePeriods(&expected);
        expected = QnTimePeriodList::aggregateTimePeriodsUnconstrained(
            std::move(expected),
            std::max<milliseconds>(m_lookupOptions.detailLevel, kMinTimePeriodAggregationPeriod));

        roundTimePeriods(&actualTimePeriods);

        assertTimePeriodsMatchUpToAggregationPeriod(expected, actualTimePeriods);
    }

    void assertTimePeriodsMatchUpToAggregationPeriod(
        const QnTimePeriodList& left, const QnTimePeriodList& right)
    {
        ASSERT_EQ(left.size(), right.size());

        for (int i = 0; i < left.size(); ++i)
        {
            ASSERT_LE(
                std::chrono::abs(left[i].startTime() - right[i].startTime()),
                kTrackAggregationPeriod);
        }
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
        auto periods = filterTimePeriods(
            filter(),
            toTimePeriods(
                filterPackets(filter(), analyticsDataPackets()),
                m_lookupOptions));

        if (filter().sortOrder == Qt::SortOrder::DescendingOrder)
        {
            std::sort(
                periods.begin(), periods.end(),
                [](const auto& left, const auto& right) { return left.startTime() > right.startTime(); });
        }

        if (filter().maxObjectTracksToSelect > 0 && periods.size() > filter().maxObjectTracksToSelect)
            periods.erase(periods.begin() + filter().maxObjectTracksToSelect, periods.end());

        return periods;
    }

    void roundTimePeriods(QnTimePeriodList* periods)
    {
        using namespace std::chrono;

        // Rounding to the aggregation period.
        for (auto& timePeriod: *periods)
        {
            timePeriod.setStartTime(duration_cast<seconds>(timePeriod.startTime()));
            if (timePeriod.duration() < kTrackAggregationPeriod)
                timePeriod.setDuration(kTrackAggregationPeriod);
        }

        *periods = QnTimePeriodList::aggregateTimePeriodsUnconstrained(
            std::move(*periods), kTrackAggregationPeriod);
    }

    QnTimePeriodList filterTimePeriods(
        const Filter& filter,
        const QnTimePeriodList& timePeriods)
    {
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
        const std::vector<common::metadata::ObjectMetadataPacketPtr>& packets,
        TimePeriodsLookupOptions lookupOptions)
    {
        using namespace std::chrono;

        QnTimePeriodList result;
        for (const auto& packet: packets)
        {
            QnTimePeriod timePeriod(
                duration_cast<milliseconds>(microseconds(packet->timestampUs)),
                duration_cast<milliseconds>(microseconds(packet->durationUs)));

            auto it = std::upper_bound(result.begin(), result.end(), timePeriod);
            result.insert(it, timePeriod);
        }

        return QnTimePeriodList::aggregateTimePeriodsUnconstrained(
            result,
            std::max<milliseconds>(lookupOptions.detailLevel, kMinTimePeriodAggregationPeriod));
    }
};

TEST_F(AnalyticsDbTimePeriodsLookup, selecting_all_periods)
{
    whenLookupTimePeriods();
    thenResultMatchesExpectations();
}

TEST_F(AnalyticsDbTimePeriodsLookup, selecting_all_periods_by_device)
{
    givenEmptyFilter();
    addRandomKnownDeviceIdToFilter();

    whenLookupTimePeriods();
    thenResultMatchesExpectations();
}

TEST_F(AnalyticsDbTimePeriodsLookup, selecting_all_periods_aggregated_to_one)
{
    givenAggregationPeriodEqualToTheWholeArchivePeriod();
    whenLookupTimePeriods();
    thenResultMatchesExpectations();
}

TEST_F(
    AnalyticsDbTimePeriodsLookup,
    selecting_periods_with_timestamp_filter_only)
{
    givenFilterWithTimePeriod();
    whenLookupTimePeriods();
    thenResultMatchesExpectations();
}

TEST_F(AnalyticsDbTimePeriodsLookup, with_aggregation_period)
{
    addRandomKnownDeviceIdToFilter();
    givenRandomLookupOptions();

    whenLookupTimePeriods();

    thenResultMatchesExpectations();
}

TEST_F(AnalyticsDbTimePeriodsLookup, with_region)
{
    givenEmptyFilter();
    addRandomBoundingBoxToFilter();

    whenLookupTimePeriods();

    thenResultMatchesExpectations();
}

TEST_F(AnalyticsDbTimePeriodsLookup, with_random_filter)
{
    givenRandomFilter();
    whenLookupTimePeriods();
    thenResultMatchesExpectations();
}

TEST_F(AnalyticsDbTimePeriodsLookup, lookup_by_unknown_text_produces_no_objects)
{
    addRandomUnknownText();
    whenLookupTimePeriods();
    thenResultMatchesExpectations();
}

// TODO: with_random_filter_and_aggregation

//-------------------------------------------------------------------------------------------------
// Deprecating data.

/**
 * Initial condition for every test: there is some random data in the DB.
 */
class AnalyticsDbCleanup:
    public AnalyticsDb
{
    using base_type = AnalyticsDb;

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

        const auto filter = buildEmptyFilter();

        whenLookupObjectTracks(filter);
        thenLookupSucceded();
        andLookupResultMatches(filter, analyticsDataPackets());
    }

    void thenDataIsExpected()
    {
        auto filter = buildEmptyFilter();
        filter.timePeriod.setStartTime(m_oldestAvailableDataTimestamp);
        filter.timePeriod.durationMs = QnTimePeriod::kInfiniteDuration;

        for (;;)
        {
            whenLookupObjectTracks(buildEmptyFilter());
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
        const std::vector<common::metadata::ObjectMetadataPacketPtr>&
            packets = analyticsDataPackets();
        auto maxElement = std::max_element(
            packets.begin(), packets.end(),
            [](const common::metadata::ObjectMetadataPacketPtr& left,
                const common::metadata::ObjectMetadataPacketPtr& right)
            {
                return left->timestampUs < right->timestampUs;
            });

        return (*maxElement)->timestampUs;
    }

    qint64 getMinTimestamp() const
    {
        const std::vector<common::metadata::ObjectMetadataPacketPtr>&
            packets = analyticsDataPackets();
        auto maxElement = std::min_element(
            packets.begin(), packets.end(),
            [](const common::metadata::ObjectMetadataPacketPtr& left,
                const common::metadata::ObjectMetadataPacketPtr& right)
            {
                return left->timestampUs < right->timestampUs;
            });

        return (*maxElement)->timestampUs;
    }
};

TEST_F(AnalyticsDbCleanup, removing_all_data)
{
    whenRemoveEventsUpToLatestEventTimestamp();
    thenStorageIsEmpty();
}

TEST_F(AnalyticsDbCleanup, removing_data_up_to_a_random_available_timestamp)
{
    whenRemoveEventsUpToARandomAvailableTimestamp();
    thenDataIsExpected();
}

TEST_F(AnalyticsDbCleanup, removing_data_up_to_minimal_available_timestamp)
{
    whenRemoveEventsUpToEarlisestEventTimestamp();
    thenDataIsNotChanged();
}

} // namespace nx::analytics::db::test
