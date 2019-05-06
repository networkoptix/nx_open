#pragma once

#include <chrono>
#include <deque>
#include <optional>
#include <vector>

#include <QtCore/QRect>
#include <QtCore/QSize>

#include "analytics_events_storage_types.h"
#include "rect_aggregator.h"

namespace nx::analytics::storage {

struct AggregatedTrackData
{
    std::chrono::milliseconds timestamp;
    /**
     * Resolution is defined by ObjectTrackAggregator settings.
     */
    QRect boundingBox;
    std::set<QnUuid> objectIds;
};

/**
 * NOTE: The class is not thread-safe.
 */
class ObjectTrackAggregator
{
public:
    ObjectTrackAggregator(
        int resolutionX,
        int resolutionY,
        std::chrono::milliseconds aggregationPeriod);

    /**
     * @param box Every coordinate is in [0;1] range.
     */
    void add(
        const QnUuid& objectId,
        std::chrono::milliseconds timestamp,
        const QRectF& box);

    /**
     * @param flush If true then all data is loaded, the aggregation period is ignored.
     */
    std::vector<AggregatedTrackData> getAggregatedData(bool flush);

private:
    struct AggregationContext
    {
        std::optional<std::chrono::milliseconds> aggregationStartTimestamp;
        std::optional<std::chrono::milliseconds> aggregationEndTimestamp;
        RectAggregator<QnUuid /*objectId*/> rectAggregator;
    };

    const QSize m_resolution;
    const std::chrono::milliseconds m_aggregationPeriod;

    std::deque<AggregationContext> m_aggregations;

    void add(
        AggregationContext* context,
        const QnUuid& objectId,
        std::chrono::milliseconds timestamp,
        const QRectF& box);

    std::vector<AggregatedTrackData> getAggregatedData(
        AggregationContext* context);

    void takeOldestData(
        std::vector<AggregatedTrackData>* const totalAggregated);

    std::chrono::milliseconds length(const AggregationContext& context) const;
};

} // namespace nx::analytics::storage
