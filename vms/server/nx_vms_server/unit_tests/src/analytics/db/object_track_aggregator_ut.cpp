#include <gtest/gtest.h>

#include <analytics/db/config.h>
#include <nx/vms/server/analytics/db/object_track_aggregator.h>

#include "analytics_storage_types.h"

namespace nx::analytics::db::test {

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
                packet->timestampUs =
                    m_packets.back()->timestampUs + microseconds(hours(1)).count();
            }

            m_packets.push_back(packet);
        }

        for (auto& packet: m_packets)
        {
            for (auto& objectMetadata: packet->objectMetadataList)
            {
                const auto timestamp = milliseconds(packet->timestampUs / 1000);
                m_aggregator.add(
                    objectMetadata.trackId,
                    timestamp,
                    objectMetadata.boundingBox);
                m_objectToTimestamp[objectMetadata.trackId] = timestamp;
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
            for (const auto& trackId: aggregatedTrack.trackIds)
            {
                ASSERT_LE(
                    std::chrono::abs(m_objectToTimestamp[trackId] - aggregatedTrack.timestamp),
                    kTrackAggregationPeriod);
            }
        }
    }

private:
    ObjectTrackAggregator m_aggregator;
    std::vector<common::metadata::ObjectMetadataPacketPtr> m_packets;
    std::map<QnUuid /*trackId*/, std::chrono::milliseconds /*timestamp*/> m_objectToTimestamp;

    std::vector<AggregatedTrackData> m_aggregatedData;
};

TEST_F(AnalyticsDbObjectTrackAggregator, never_aggregates_packets_with_a_large_ts_difference)
{
    whenAddObjectsWithLargeTimestampDifference();
    whenRequestAggregatedData();

    thenAggregatedDataIsProvided();
    andEveryObjectIsPlacedToSearchCellWithCloseTimestamp();
}

} // namespace nx::analytics::db::test
