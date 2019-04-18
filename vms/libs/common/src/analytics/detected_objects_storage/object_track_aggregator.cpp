#include "object_track_aggregator.h"

#include <nx/utils/time.h>

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
    const QnUuid& objectId,
    std::chrono::milliseconds timestamp,
    const QRectF& box)
{
    const auto translatedBox = translate(box);

    m_rectAggregator.add(translatedBox, objectId);

    m_aggregationStartTimestamp = m_aggregationStartTimestamp
        ? std::min(*m_aggregationStartTimestamp, timestamp)
        : timestamp;

    m_aggregationEndTimestamp = m_aggregationEndTimestamp
        ? std::max(*m_aggregationEndTimestamp, timestamp)
        : timestamp;
}

std::vector<AggregatedTrackData> ObjectTrackAggregator::getAggregatedData(bool flush)
{
    if (!flush && length() < m_aggregationPeriod)
        return {};

    auto aggregated = m_rectAggregator.takeAggregatedData();
    std::vector<AggregatedTrackData> result;
    result.reserve(aggregated.size());
    for (auto& rect: aggregated)
    {
        result.push_back(AggregatedTrackData());
        result.back().boundingBox = rect.rect;
        result.back().objectIds = std::exchange(rect.values, {});
        // TODO: #ak It would be better to use precise timestamp for this region.
        result.back().timestamp = *m_aggregationStartTimestamp;
    }

    m_aggregationStartTimestamp = std::nullopt;
    m_aggregationEndTimestamp = std::nullopt;

    return result;
}

std::chrono::milliseconds ObjectTrackAggregator::length() const
{
    return m_aggregationStartTimestamp
        ? *m_aggregationEndTimestamp - *m_aggregationStartTimestamp
        : std::chrono::milliseconds::zero();
}

QRect ObjectTrackAggregator::translate(const QRectF& box)
{
    return QRect(
        box.x() * m_resolutionX,
        box.y() * m_resolutionY,
        std::ceil(box.width() * m_resolutionX),
        std::ceil(box.height() * m_resolutionY));
}

} // namespace nx::analytics::storage
