#include "analytics_archive.h"

#include "config.h"
#include "serializers.h"

namespace nx::analytics::storage {

AnalyticsArchive::AnalyticsArchive(
    const QString& dataDir,
    const QnUuid& resourceId)
    :
    m_dataDir(dataDir),
    m_resourceId(resourceId)
{
}

AnalyticsArchive::~AnalyticsArchive()
{
}

bool AnalyticsArchive::saveToArchive(
    std::chrono::milliseconds timestamp,
    const std::vector<QRectF>& region,
    int objectType,
    long long allAttributesId)
{
    const auto translatedRegion = translate<>(
        region,
        QSize(kTrackSearchResolutionX, kTrackSearchResolutionY));

    for (const auto& rect: translatedRegion)
    {
        m_items.push_back({
            timestamp,
            rect,
            objectType,
            allAttributesId});
    }

    return true;
}

QnTimePeriodList AnalyticsArchive::matchPeriod(const Filter& filter)
{
    QnTimePeriodList periods;

    const auto translatedRegion = translate<>(
        filter.region,
        QSize(kTrackSearchResolutionX, kTrackSearchResolutionY));

    std::cout<<"Total items: " << m_items.size() << std::endl; 

    for (const auto& item: m_items)
    {
        if (satisfiesFilter(item, filter, translatedRegion))
            periods += QnTimePeriod(item.timestamp, std::chrono::milliseconds(1));
    }

    return QnTimePeriodList::aggregateTimePeriodsUnconstrained(periods, filter.detailLevel);
}

bool AnalyticsArchive::satisfiesFilter(
    const Item& item,
    const Filter& filter,
    const std::vector<QRect>& regionFilter)
{
    if (!filter.timePeriod.isEmpty() && !filter.timePeriod.contains(item.timestamp))
        return false;

    if (!filter.objectTypes.empty() && filter.objectTypes.count(item.objectType) == 0)
        return false;

    if (!filter.allAttributesHash.empty() && 
        filter.allAttributesHash.count(item.allAttributesId) == 0)
    {
        return false;
    }

    if (!regionFilter.empty())
    {
        bool intersects = false;
        for (const auto& rect: regionFilter)
            intersects |= item.rect.intersects(rect);
        if (!intersects)
            return false;
    }

    return true;
}

} // namespace nx::analytics::storage
