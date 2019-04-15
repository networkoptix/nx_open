#include "object_track_aggregator.h"

namespace nx::analytics::storage {

ObjectTrackAggregator::ObjectTrackAggregator(
    int resolutionX,
    int resolutionY,
    std::chrono::milliseconds aggregationPeriod)
    :
    m_resolutionX(resolutionX),
    m_resolutionY(resolutionY),
    m_aggregationPeriod(aggregationPeriod)
{
}

void ObjectTrackAggregator::add(
    const QnUuid& /*objectId*/,
    std::chrono::milliseconds /*timestamp*/,
    const QRectF& /*box*/)
{
    // TODO
}

std::vector<AggregatedTrackData> ObjectTrackAggregator::getAggregatedData()
{
    // TODO
    return {};
}

} // namespace nx::analytics::storage
