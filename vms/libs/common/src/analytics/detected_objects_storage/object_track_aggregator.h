#pragma once

#include <chrono>
#include <optional>
#include <vector>

#include <QtCore/QRect>

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

    std::chrono::milliseconds length() const;

private:
    const int m_resolutionX;
    const int m_resolutionY;
    const std::chrono::milliseconds m_aggregationPeriod;

    std::optional<std::chrono::milliseconds> m_aggregationStartTimestamp;
    std::optional<std::chrono::milliseconds> m_aggregationEndTimestamp;
    RectAggregator<QnUuid /*objectId*/> m_rectAggregator;

    QRect translate(const QRectF& box);
};

} // namespace nx::analytics::storage
