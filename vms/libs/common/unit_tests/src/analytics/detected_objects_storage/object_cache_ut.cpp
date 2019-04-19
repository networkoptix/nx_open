#include <vector>

#include <gtest/gtest.h>

#include <nx/utils/time.h>

#include <analytics/detected_objects_storage/object_cache.h>
#include <test_support/analytics/storage/analytics_storage_types.h>

namespace nx::analytics::storage::test {

class AnalyticsDbObjectCache:
    public ::testing::Test
{
public:
    AnalyticsDbObjectCache():
        m_aggregationPeriod(std::chrono::hours(1)),
        m_maxObjectLifeTime(std::chrono::hours(10)),
        m_objectCache(m_aggregationPeriod, m_maxObjectLifeTime),
        m_timeShift(nx::utils::test::ClockType::steady)
    {
    }

protected:
    void givenSomeAnalyticsData()
    {
        whenMoreAnalyticsEventsAdded();
    }

    void givenAlreadyUsedObjectInCache()
    {
        givenSomeAnalyticsData();

        whenWaitForAggregationPeriod();
        whenFetchObjectsToInsert();

        thenTheObjectInsertionIsProvided();
    }

    void whenWaitForAggregationPeriod()
    {
        m_timeShift.applyRelativeShift(m_aggregationPeriod);
    }

    void whenWaitForMaxObjectLifetime()
    {
        m_timeShift.applyRelativeShift(m_maxObjectLifeTime);
    }

    void whenFetchObjectsToInsert()
    {
        m_objectsToInsert = m_objectCache.getObjectsToInsert();
    }

    void whenForceFetchingObjectToInsert()
    {
        auto object = m_objectCache.getObjectToInsertForced(
            m_analyticsDataPackets.front()->objects.front().objectId);

        if (object)
            m_objectsToInsert.push_back(std::move(*object));
    }

    void whenRemovingExpiredDataFromCache()
    {
        m_objectCache.removeExpiredData();
    }

    void whenRequestObjectUpdates()
    {
        m_objectUpdates = m_objectCache.getObjectsToUpdate();
    }

    void whenMoreAnalyticsEventsAdded()
    {
        auto packet = generateRandomPacket(1);
        if (!m_analyticsDataPackets.empty())
        {
            packet->deviceId = m_analyticsDataPackets.back()->deviceId;
            packet->objects.front().objectId =
                m_analyticsDataPackets.back()->objects.front().objectId;
            packet->objects.front().objectTypeId =
                m_analyticsDataPackets.back()->objects.front().objectTypeId;
        }

        m_analyticsDataPackets.push_back(std::move(packet));
        m_objectCache.add(m_analyticsDataPackets.back());
    }

    void thenTheObjectInsertionIsProvided()
    {
        ASSERT_GT(m_objectsToInsert.size(), 0);
    }

    void thenNoObjectAreProvided()
    {
        ASSERT_EQ(0, m_objectsToInsert.size());
    }

    void thenTheObjectUpdateIsReported()
    {
        ASSERT_GT(m_objectUpdates.size(), 0);
    }

    void thenNoObjectUpdateIsReported()
    {
        ASSERT_TRUE(m_objectUpdates.empty());
    }

    void andOnlyNewTrackIsPresentInUpdate()
    {
        ASSERT_EQ(1, m_objectUpdates.front().appendedTrack.size());
    }

    void andNewAttributeListIsPresentInUpdate()
    {
        ASSERT_EQ(
            calcAttrDiff(
                m_analyticsDataPackets.front()->objects.front().labels,
                m_analyticsDataPackets.back()->objects.front().labels),
            m_objectUpdates.front().appendedAttributes);
    }

    void andFullAttributeListIsPresentInUpdate()
    {
        const auto expected =
            uniqueAttribites(
                m_analyticsDataPackets.front()->objects.front().labels,
                m_analyticsDataPackets.back()->objects.front().labels);
        const auto actual = m_objectUpdates.front().allAttributes;

        ASSERT_EQ(expected, actual);
    }

private:
    using Attributes = std::vector<common::metadata::Attribute>;

    const std::chrono::seconds m_aggregationPeriod;
    const std::chrono::seconds m_maxObjectLifeTime;
    storage::ObjectCache m_objectCache;
    nx::utils::test::ScopedTimeShift m_timeShift;
    std::vector<ObjectUpdate> m_objectUpdates;

    std::vector<DetectedObject> m_objectsToInsert;
    std::vector<common::metadata::DetectionMetadataPacketPtr> m_analyticsDataPackets;

    Attributes calcAttrDiff(const Attributes& from, const Attributes& to)
    {
        Attributes result;

        for (const auto& attr: to)
        {
            if (std::find(from.begin(), from.end(), attr) == from.end())
                result.push_back(attr);
        }

        return result;
    }

    Attributes uniqueAttribites(const Attributes& from, const Attributes& to)
    {
        Attributes result = from;
        for (const auto& attr: to)
        {
            auto it = std::find_if(result.begin(), result.end(),
                [&attr](const auto& val) { return val.name == attr.name; });
            if (it == result.end())
                result.push_back(attr);
            else if (it->value != attr.value)
                it->value = attr.value;
        }

        return result;
    }
};

TEST_F(AnalyticsDbObjectCache, new_object_is_provided_for_insertion_after_aggregation_period)
{
    givenSomeAnalyticsData();

    whenWaitForAggregationPeriod();
    whenFetchObjectsToInsert();

    thenTheObjectInsertionIsProvided();
}

TEST_F(AnalyticsDbObjectCache, object_insertion_is_reported_only_once)
{
    givenAlreadyUsedObjectInCache();

    whenWaitForAggregationPeriod();
    whenFetchObjectsToInsert();

    thenNoObjectAreProvided();
}

TEST_F(AnalyticsDbObjectCache, new_object_is_not_provided_for_insertion_until_aggregation_period)
{
    givenSomeAnalyticsData();
    whenFetchObjectsToInsert();
    thenNoObjectAreProvided();
}

TEST_F(AnalyticsDbObjectCache, new_object_can_be_taken_for_insertion_asap)
{
    givenSomeAnalyticsData();
    whenForceFetchingObjectToInsert();
    thenTheObjectInsertionIsProvided();
}

TEST_F(AnalyticsDbObjectCache, object_update_is_reported_after_aggregation_period)
{
    givenAlreadyUsedObjectInCache();

    whenMoreAnalyticsEventsAdded();
    whenWaitForAggregationPeriod();
    whenRequestObjectUpdates();

    thenTheObjectUpdateIsReported();
    andOnlyNewTrackIsPresentInUpdate();
    andNewAttributeListIsPresentInUpdate();
    andFullAttributeListIsPresentInUpdate();
}

TEST_F(AnalyticsDbObjectCache, object_update_is_not_reported_if_no_changes)
{
    givenAlreadyUsedObjectInCache();

    whenWaitForAggregationPeriod();
    whenRequestObjectUpdates();

    thenNoObjectUpdateIsReported();
}

TEST_F(AnalyticsDbObjectCache, new_object_is_not_reported_as_updated)
{
    givenSomeAnalyticsData();

    whenWaitForAggregationPeriod();
    whenRequestObjectUpdates();

    thenNoObjectUpdateIsReported();
}

TEST_F(AnalyticsDbObjectCache, object_update_not_is_provided_until_aggregation_period)
{
    givenAlreadyUsedObjectInCache();
    whenRequestObjectUpdates();
    thenNoObjectUpdateIsReported();
}

TEST_F(AnalyticsDbObjectCache, the_object_is_removed_after_maxObjectLifetime)
{
    givenAlreadyUsedObjectInCache();

    whenWaitForMaxObjectLifetime();
    whenRemovingExpiredDataFromCache();
    whenRequestObjectUpdates();

    thenNoObjectUpdateIsReported();
}

} // namespace nx::analytics::storage::test
