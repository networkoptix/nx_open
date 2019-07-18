#include "object_track_aggregator.h"

#include <analytics/db/config.h>

#include "serializers.h"

namespace nx::analytics::db {

ObjectTrackAggregator::AggregationContext::AggregationContext():
    rectAggregator(QSize(kTrackSearchResolutionX, kTrackSearchResolutionY))
{
}

//-------------------------------------------------------------------------------------------------

ObjectTrackAggregator::ObjectTrackAggregator(
    int resolutionX,
    int resolutionY,
    std::chrono::milliseconds aggregationPeriod)
    :
    m_resolution(resolutionX, resolutionY),
    m_aggregationPeriod(aggregationPeriod)
{
}

void ObjectTrackAggregator::add(
    const QnUuid& objectId,
    std::chrono::milliseconds timestamp,
    const QRectF& box)
{
    if (m_aggregations.empty() ||
        std::chrono::abs(*m_aggregations.back().aggregationStartTimestamp - timestamp) >
            m_aggregationPeriod)
    {
        m_aggregations.push_back(AggregationContext());
    }

    add(
        &m_aggregations.back(),
        objectId,
        timestamp,
        box);
}

std::vector<AggregatedTrackData> ObjectTrackAggregator::getAggregatedData(bool flush)
{
    if (m_aggregations.empty())
        return {};

    std::vector<AggregatedTrackData> totalAggregated;
    while (m_aggregations.size() > 1)
        takeOldestData(&totalAggregated);

    if (flush || length(m_aggregations.front()) >= m_aggregationPeriod)
        takeOldestData(&totalAggregated);

    return totalAggregated;
}

void ObjectTrackAggregator::add(
    AggregationContext* context,
    const QnUuid& objectId,
    std::chrono::milliseconds timestamp,
    const QRectF& box)
{
    const auto translatedBox = translateToSearchGrid(box);

    context->rectAggregator.add(translatedBox, objectId);

    context->aggregationStartTimestamp = context->aggregationStartTimestamp
        ? std::min(*context->aggregationStartTimestamp, timestamp)
        : timestamp;

    context->aggregationEndTimestamp = context->aggregationEndTimestamp
        ? std::max(*context->aggregationEndTimestamp, timestamp)
        : timestamp;
}

std::vector<AggregatedTrackData> ObjectTrackAggregator::getAggregatedData(
    AggregationContext* context)
{
    auto aggregated = context->rectAggregator.takeAggregatedData();
    std::vector<AggregatedTrackData> result;
    result.reserve(aggregated.size());
    for (auto& rect : aggregated)
    {
        result.push_back(AggregatedTrackData());
        result.back().boundingBox = rect.rect;
        result.back().objectIds = std::exchange(rect.values, {});
        // TODO: #ak It would be better to use precise timestamp for this region.
        result.back().timestamp = *context->aggregationStartTimestamp;
    }

    context->aggregationStartTimestamp = std::nullopt;
    context->aggregationEndTimestamp = std::nullopt;

    return result;
}

void ObjectTrackAggregator::takeOldestData(
    std::vector<AggregatedTrackData>* totalAggregated)
{
    auto aggregated = getAggregatedData(&m_aggregations.front());
    std::move(aggregated.begin(), aggregated.end(), std::back_inserter(*totalAggregated));
    m_aggregations.pop_front();
}

std::chrono::milliseconds ObjectTrackAggregator::length(
    const AggregationContext& context) const
{
    return context.aggregationStartTimestamp
        ? *context.aggregationEndTimestamp - *context.aggregationStartTimestamp
        : std::chrono::milliseconds::zero();
}

} // namespace nx::analytics::db
