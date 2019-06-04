#include "time_period_fetcher.h"

#include <nx/sql/query.h>
#include <nx/utils/elapsed_timer.h>

#include <analytics/db/config.h>

#include "attributes_dao.h"
#include "device_dao.h"
#include "object_searcher.h"
#include "serializers.h"

namespace nx::analytics::db {

static const QSize kSearchResolution(
    kTrackSearchResolutionX,
    kTrackSearchResolutionY);

TimePeriodFetcher::TimePeriodFetcher(
    const DeviceDao& deviceDao,
    const ObjectTypeDao& objectTypeDao,
    AttributesDao* attributesDao,
    AnalyticsArchiveDirectory* analyticsArchive,
    std::chrono::milliseconds maxRecordedTimestamp)
    :
    m_deviceDao(deviceDao),
    m_objectTypeDao(objectTypeDao),
    m_attributesDao(attributesDao),
    m_analyticsArchive(analyticsArchive),
    m_maxRecordedTimestamp(maxRecordedTimestamp)
{
}

nx::sql::DBResult TimePeriodFetcher::selectTimePeriods(
    nx::sql::QueryContext* queryContext,
    const Filter& filter,
    const TimePeriodsLookupOptions& options,
    QnTimePeriodList* result)
{
    if (!filter.objectAppearanceId.isNull())
        *result = selectTimePeriodsByObject(queryContext, filter, options);
    else
        *result = selectTimePeriodsFiltered(queryContext, filter, options);

    return nx::sql::DBResult::ok;
}

QnTimePeriodList TimePeriodFetcher::selectTimePeriodsByObject(
    nx::sql::QueryContext* queryContext,
    const Filter& filter,
    const TimePeriodsLookupOptions& options)
{
    using namespace std::chrono;

    ObjectSearcher objectSearcher(m_deviceDao, m_objectTypeDao, filter);
    const auto objects = objectSearcher.lookup(queryContext);

    QnTimePeriodList result;
    for (const auto& object: objects)
    {
        for (const auto& position: object.track)
        {
            result += QnTimePeriod(
                duration_cast<milliseconds>(microseconds(position.timestampUsec)),
                duration_cast<milliseconds>(microseconds(position.durationUsec)));
        }
    }

    return QnTimePeriodList::aggregateTimePeriodsUnconstrained(
        result, options.detailLevel);
}

QnTimePeriodList TimePeriodFetcher::selectTimePeriodsFiltered(
    nx::sql::QueryContext* queryContext,
    const Filter& filter,
    const TimePeriodsLookupOptions& options)
{
    AnalyticsArchive::Filter archiveFilter = prepareArchiveFilter(filter, options);

    nx::utils::ElapsedTimer timer;

    if (!filter.freeText.isEmpty())
    {
        timer.restart();
        archiveFilter.allAttributesHash = 
            m_attributesDao->lookupCombinedAttributes(queryContext, filter.freeText);
        NX_DEBUG(this, "Text '%1' lookup completed in %2", filter.freeText, timer.elapsed());
    }

    timer.restart();
    const auto timePeriods = m_analyticsArchive->matchPeriods(filter.deviceIds, archiveFilter);
    NX_DEBUG(this, "Time periods lookup completed in %1", timer.elapsed());
    return timePeriods;
}

AnalyticsArchive::Filter TimePeriodFetcher::prepareArchiveFilter(
    const Filter& filter,
    const TimePeriodsLookupOptions& options)
{
    AnalyticsArchive::Filter archiveFilter;

    if (!filter.objectTypeId.empty())
    {
        std::transform(
            filter.objectTypeId.begin(), filter.objectTypeId.end(),
            std::inserter(archiveFilter.objectTypes, archiveFilter.objectTypes.begin()),
            [this](const auto& objectTypeName) { return m_objectTypeDao.objectTypeIdFromName(objectTypeName); });
    }

    archiveFilter.region = options.region;
    archiveFilter.timePeriod = filter.timePeriod;
    archiveFilter.sortOrder = filter.sortOrder;
    if (filter.maxObjectsToSelect > 0)
        archiveFilter.limit = filter.maxObjectsToSelect;
    archiveFilter.detailLevel = options.detailLevel;

    return archiveFilter;
}

} // namespace nx::analytics::db
