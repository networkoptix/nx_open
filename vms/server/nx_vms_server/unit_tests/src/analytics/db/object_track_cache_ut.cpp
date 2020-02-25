#include <vector>

#include <gtest/gtest.h>

#include <nx/utils/time.h>

#include <nx/vms/server/analytics/db/object_track_cache.h>

#include "attribute_dictionary.h"
#include <nx/vms/server/analytics/abstract_i_frame_search_helper.h>

class QnResourcePool;
class QnVideoCameraPool;

namespace nx::analytics::db::test {

static const qint64 kUserDefinedBestShotTime = 2;
static const qint64 kAutoAlignBestShotTime = 2;

class MocIFrameSearchHelper: public nx::vms::server::analytics::AbstractIFrameSearchHelper
{
public:
    MocIFrameSearchHelper(
        const QnResourcePool* resourcePool,
        const QnVideoCameraPool* cameraPool)
    {
    }

    virtual qint64 findAfter(
        const QnUuid& deviceId,
        nx::vms::api::StreamIndex streamIndex,
        qint64 timestampUs) const override
    {
        return kAutoAlignBestShotTime;
    }
};

class AnalyticsDbObjectTrackCache:
    public ::testing::Test
{
public:
    AnalyticsDbObjectTrackCache():
        m_aggregationPeriod(std::chrono::hours(1)),
        m_maxObjectLifeTime(std::chrono::hours(10)),
        m_iframeHelper(new MocIFrameSearchHelper(nullptr, nullptr)),
        m_objectTrackCache(m_aggregationPeriod, m_maxObjectLifeTime, m_iframeHelper.get()),
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
        m_objectsToInsert = m_objectTrackCache.getTracksToInsert();
    }

    void whenForceFetchingObjectToInsert()
    {
        auto track = m_objectTrackCache.getTrackToInsertForced(
            m_analyticsDataPackets.front()->objectMetadataList.front().trackId);

        if (track)
            m_objectsToInsert.push_back(std::move(*track));
    }

    void whenRemovingExpiredDataFromCache()
    {
        m_objectTrackCache.removeExpiredData();
    }

    void whenRequestObjectUpdates()
    {
        m_objectUpdates = m_objectTrackCache.getTracksToUpdate();
    }

    void whenMoreAnalyticsEventsAdded()
    {
        auto packet = generateRandomPacket(1);
        if (!m_analyticsDataPackets.empty())
        {
            packet->deviceId = m_analyticsDataPackets.back()->deviceId;
            packet->objectMetadataList.front().trackId =
                m_analyticsDataPackets.back()->objectMetadataList.front().trackId;
            packet->objectMetadataList.front().typeId =
                m_analyticsDataPackets.back()->objectMetadataList.front().typeId;
        }

        m_analyticsDataPackets.push_back(std::move(packet));
        m_objectTrackCache.add(m_analyticsDataPackets.back());
    }

    void givenSequentialAnalyticsData()
    {
        qint64 timestampUs = 0;
        for (int i = 0; i < 100; ++i)
        {
            auto packet = generateRandomPacket(1);
            if (!m_analyticsDataPackets.empty())
            {
                packet->deviceId = m_analyticsDataPackets.back()->deviceId;
                packet->objectMetadataList.front().trackId =
                    m_analyticsDataPackets.back()->objectMetadataList.front().trackId;
                packet->objectMetadataList.front().typeId =
                    m_analyticsDataPackets.back()->objectMetadataList.front().typeId;
            }

            packet->timestampUs = timestampUs++;
            m_analyticsDataPackets.push_back(std::move(packet));
            m_objectTrackCache.add(m_analyticsDataPackets.back());
        }
    }

    void givenUserDefinedBestShot()
    {
        auto packet = generateRandomPacket(1);
        if (!m_analyticsDataPackets.empty())
        {
            packet->deviceId = m_analyticsDataPackets.back()->deviceId;
            packet->objectMetadataList.front().trackId =
                m_analyticsDataPackets.back()->objectMetadataList.front().trackId;
            packet->objectMetadataList.front().typeId =
                m_analyticsDataPackets.back()->objectMetadataList.front().typeId;
        }

        packet->timestampUs = kUserDefinedBestShotTime;
        packet->objectMetadataList.at(0).bestShot = true;
        m_analyticsDataPackets.push_back(std::move(packet));
        m_objectTrackCache.add(m_analyticsDataPackets.back());
    }

    void thenBestShotAlignedToIFrame()
    {
        ASSERT_EQ(1, m_objectsToInsert.size());
        ASSERT_EQ(kAutoAlignBestShotTime, m_objectsToInsert[0].bestShot.timestampUs);
    }

    void thenBestShotMatchToUserDefinedValue()
    {
        ASSERT_EQ(1, m_objectsToInsert.size());
        ASSERT_EQ(kUserDefinedBestShotTime, m_objectsToInsert[0].bestShot.timestampUs);
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
        ASSERT_EQ(1, m_objectUpdates.front().appendedTrack.isSimpleRect());
    }

    void andNewAttributeListIsPresentInUpdate()
    {
        ASSERT_EQ(
            calcAttrDiff(
                m_analyticsDataPackets.front()->objectMetadataList.front().attributes,
                m_analyticsDataPackets.back()->objectMetadataList.front().attributes),
            m_objectUpdates.front().appendedAttributes);
    }

    void andFullAttributeListIsPresentInUpdate()
    {
        const auto expected =
            uniqueAttribites(
                m_analyticsDataPackets.front()->objectMetadataList.front().attributes,
                m_analyticsDataPackets.back()->objectMetadataList.front().attributes);
        const auto actual = m_objectUpdates.front().allAttributes;

        ASSERT_EQ(expected, actual);
    }

private:
    using Attributes = std::vector<common::metadata::Attribute>;

    const std::chrono::seconds m_aggregationPeriod;
    const std::chrono::seconds m_maxObjectLifeTime;
    std::unique_ptr<MocIFrameSearchHelper> m_iframeHelper;
    db::ObjectTrackCache m_objectTrackCache;
    nx::utils::test::ScopedTimeShift m_timeShift;
    std::vector<ObjectTrackUpdate> m_objectUpdates;

    std::vector<ObjectTrack> m_objectsToInsert;
    std::vector<common::metadata::ObjectMetadataPacketPtr> m_analyticsDataPackets;

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

TEST_F(AnalyticsDbObjectTrackCache, new_object_is_provided_for_insertion_after_aggregation_period)
{
    givenSomeAnalyticsData();

    whenWaitForAggregationPeriod();
    whenFetchObjectsToInsert();

    thenTheObjectInsertionIsProvided();
}

TEST_F(AnalyticsDbObjectTrackCache, object_insertion_is_reported_only_once)
{
    givenAlreadyUsedObjectInCache();

    whenWaitForAggregationPeriod();
    whenFetchObjectsToInsert();

    thenNoObjectAreProvided();
}

TEST_F(AnalyticsDbObjectTrackCache, new_object_is_not_provided_for_insertion_until_aggregation_period)
{
    givenSomeAnalyticsData();
    whenFetchObjectsToInsert();
    thenNoObjectAreProvided();
}

TEST_F(AnalyticsDbObjectTrackCache, new_object_can_be_taken_for_insertion_asap)
{
    givenSomeAnalyticsData();
    whenForceFetchingObjectToInsert();
    thenTheObjectInsertionIsProvided();
}

TEST_F(AnalyticsDbObjectTrackCache, object_update_is_reported_after_aggregation_period)
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

TEST_F(AnalyticsDbObjectTrackCache, object_update_is_not_reported_if_no_changes)
{
    givenAlreadyUsedObjectInCache();

    whenWaitForAggregationPeriod();
    whenRequestObjectUpdates();

    thenNoObjectUpdateIsReported();
}

TEST_F(AnalyticsDbObjectTrackCache, new_object_is_not_reported_as_updated)
{
    givenSomeAnalyticsData();

    whenWaitForAggregationPeriod();
    whenRequestObjectUpdates();

    thenNoObjectUpdateIsReported();
}

TEST_F(AnalyticsDbObjectTrackCache, object_update_not_is_provided_until_aggregation_period)
{
    givenAlreadyUsedObjectInCache();
    whenRequestObjectUpdates();
    thenNoObjectUpdateIsReported();
}

TEST_F(AnalyticsDbObjectTrackCache, the_object_is_removed_after_maxObjectLifetime)
{
    givenAlreadyUsedObjectInCache();

    whenWaitForMaxObjectLifetime();
    whenRemovingExpiredDataFromCache();
    whenRequestObjectUpdates();

    thenNoObjectUpdateIsReported();
}

TEST_F(AnalyticsDbObjectTrackCache, auto_assign_best_shot)
{
    givenSequentialAnalyticsData();

    whenWaitForAggregationPeriod();
    whenFetchObjectsToInsert();

    thenBestShotAlignedToIFrame();
}

TEST_F(AnalyticsDbObjectTrackCache, custom_best_shot_time)
{
    givenUserDefinedBestShot();
    givenSequentialAnalyticsData();

    whenWaitForAggregationPeriod();
    whenFetchObjectsToInsert();

    thenBestShotMatchToUserDefinedValue();
}

} // namespace nx::analytics::db::test
