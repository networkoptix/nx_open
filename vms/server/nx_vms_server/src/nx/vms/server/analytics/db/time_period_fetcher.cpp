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

    ObjectSearcher objectSearcher(m_deviceDao, m_objectTypeDao, m_attributesDao, m_analyticsArchive, filter);
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
    AnalyticsArchive::Filter archiveFilter =
        AnalyticsArchiveDirectory::prepareArchiveFilter(filter, m_objectTypeDao);
    archiveFilter.region = options.region;
    archiveFilter.detailLevel = options.detailLevel;

    nx::utils::ElapsedTimer timer;

    if (!filter.freeText.isEmpty())
    {
        timer.restart();
        const auto attributeGroups =
            m_attributesDao->lookupCombinedAttributes(queryContext, filter.freeText);
        std::copy(
            attributeGroups.begin(), attributeGroups.end(),
            std::back_inserter(archiveFilter.allAttributesHash));
        NX_DEBUG(this, "Text '%1' lookup completed in %2", filter.freeText, timer.elapsed());
    }

    timer.restart();
    const auto timePeriods = m_analyticsArchive->matchPeriods(filter.deviceIds, archiveFilter);
    NX_DEBUG(this, "Time periods lookup completed in %1", timer.elapsed());
    return timePeriods;
}

} // namespace nx::analytics::db
