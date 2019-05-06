#include <gtest/gtest.h>

#include <analytics/detected_objects_storage/config.h>
#include <nx/vms/server/analytics/detected_objects_storage/object_track_aggregator.h>

#include "analytics_storage_types.h"

namespace nx::analytics::storage::test {

class AnalyticsDbObjectTrackAggregator:
    public ::testing::Test
{
public:
    AnalyticsDbObjectTrackAggregator():
        m_aggregator(
            kTrackSearchResolutionX,
            kTrackSearchResolutionY,
            kTrackAggregationPeriod)
    {
    }

protected:
    void whenAddObjectsWithLargeTimestampDifference()
    {
        using namespace std::chrono;

        for (int i = 0; i < 3; ++i)
        {
            auto packet = generateRandomPacket(1);
            if (!m_packets.empty())
            {
                packet->timestampUsec =
                    m_packets.back()->timestampUsec + microseconds(hours(1)).count();
            }

            m_packets.push_back(packet);
        }

        for (auto& packet: m_packets)
        {
            for (auto& object: packet->objects)
            {
                const auto timestamp = milliseconds(packet->timestampUsec / 1000);
                m_aggregator.add(
                    object.objectId,
                    timestamp,
                    object.boundingBox);
                m_objectToTimestamp[object.objectId] = timestamp;
            }
        }
    }

    void whenRequestAggregatedData()
    {
        m_aggregatedData = m_aggregator.getAggregatedData(false);
    }

    void thenAggregatedDataIsProvided()
    {
        ASSERT_FALSE(m_aggregatedData.empty());
    }

    void andEveryObjectIsPlacedToSearchCellWithCloseTimestamp()
    {
        for (const auto& aggregatedTrack: m_aggregatedData)
        {
            for (const auto& objectId: aggregatedTrack.objectIds)
            {
                ASSERT_LE(
                    std::chrono::abs(m_objectToTimestamp[objectId] - aggregatedTrack.timestamp),
                    kTrackAggregationPeriod);
            }
        }
    }

private:
    ObjectTrackAggregator m_aggregator;
    std::vector<common::metadata::DetectionMetadataPacketPtr> m_packets;
    std::map<QnUuid /*objectId*/, std::chrono::milliseconds /*timestamp*/> m_objectToTimestamp;

    std::vector<AggregatedTrackData> m_aggregatedData;
};

TEST_F(AnalyticsDbObjectTrackAggregator, never_aggregates_packets_with_a_large_ts_difference)
{
    whenAddObjectsWithLargeTimestampDifference();
    whenRequestAggregatedData();

    thenAggregatedDataIsProvided();
    andEveryObjectIsPlacedToSearchCellWithCloseTimestamp();
}

} // namespace nx::analytics::storage::test
