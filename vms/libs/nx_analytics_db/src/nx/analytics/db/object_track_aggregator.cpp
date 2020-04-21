#include "object_track_aggregator.h"

#include <nx/utils/log/log.h>

#include <analytics/db/analytics_db_utils.h>
#include <analytics/db/config.h>

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
    const QnUuid& trackId,
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
        trackId,
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

    for (const auto& data: totalAggregated)
    {
        NX_VERBOSE(this, "Returning aggregation: timestamp %1, box %2, trackIds %3",
            data.timestamp, data.boundingBox, containerString(data.trackIds));
    }

    return totalAggregated;
}

void ObjectTrackAggregator::add(
    AggregationContext* context,
    const QnUuid& trackId,
    std::chrono::milliseconds timestamp,
    const QRectF& box)
{
    const auto translatedBox = translateToSearchGrid(box);

    NX_VERBOSE(this, "Adding track %1, box %2, timestamp %3. Translated box %4",
        trackId, box, timestamp, translatedBox);

    context->rectAggregator.add(translatedBox, trackId);

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
        result.back().trackIds = std::exchange(rect.values, {});
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
