#include "analytics_archive.h"

#include <nx/utils/std/algorithm.h>

#include <analytics/db/config.h>

#include "serializers.h"

namespace nx::analytics::db {

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
    const std::vector<QRect>& region,
    int64_t objectType,
    int64_t allAttributesId)
{
    for (const auto& rect: region)
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

    std::vector<QRect> translatedRegion;
    for (const auto& rect: filter.region)
        translatedRegion.push_back(rect);

    for (const auto& item: m_items)
    {
        if (satisfiesFilter(item, filter, translatedRegion))
            periods += QnTimePeriod(item.timestamp, std::chrono::milliseconds(1));
    }

    periods = QnTimePeriodList::aggregateTimePeriodsUnconstrained(periods, filter.detailLevel);

    if (filter.sortOrder == Qt::SortOrder::DescendingOrder)
    {
        std::sort(
            periods.begin(), periods.end(),
            [](const auto& left, const auto& right) { return left.startTime() > right.startTime(); });
    }

    return periods;
}

bool AnalyticsArchive::satisfiesFilter(
    const Item& item,
    const Filter& filter,
    const std::vector<QRect>& regionFilter)
{
    if (!(item.timestamp >= filter.startTime && item.timestamp < filter.endTime))
        return false;

    if (!filter.objectTypes.empty() && !nx::utils::contains(filter.objectTypes, item.objectType))
        return false;

    if (!filter.allAttributesHash.empty() &&
        !nx::utils::contains(filter.allAttributesHash, item.allAttributesId))
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

} // namespace nx::analytics::db
