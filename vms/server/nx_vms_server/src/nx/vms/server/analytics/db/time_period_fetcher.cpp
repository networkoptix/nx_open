#include "time_period_fetcher.h"

#include <nx/sql/query.h>
#include <nx/utils/elapsed_timer.h>

#include <analytics/db/config.h>

#include "attributes_dao.h"
#include "device_dao.h"
#include "object_track_searcher.h"
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
    if (!filter.objectTrackId.isNull())
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

    ObjectTrackSearcher objectTrackSearcher(
        m_deviceDao, m_objectTypeDao, m_attributesDao, m_analyticsArchive, filter);

    const auto tracks = objectTrackSearcher.lookup(queryContext);

    QnTimePeriodList result;
    for (const auto& track: tracks)
    {
        for (const auto& position: track.objectPositionSequence)
        {
            result += QnTimePeriod(
                duration_cast<milliseconds>(microseconds(position.timestampUs)),
                duration_cast<milliseconds>(microseconds(position.durationUs)));
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
    auto archiveFilter =
        AnalyticsArchiveDirectory::prepareArchiveFilter(
            queryContext, filter, m_objectTypeDao, m_attributesDao);

    if (!archiveFilter)
    {
        NX_DEBUG(this, "Time periods lookup canceled. The filter is %1", filter);
        return QnTimePeriodList(); //< No records with such text.
    }

    archiveFilter->detailLevel = options.detailLevel;

    nx::utils::ElapsedTimer timer;
    timer.restart();
    NX_DEBUG(this, "Time periods lookup started, filter is %1", filter);
    const auto timePeriods = m_analyticsArchive->matchPeriods(filter.deviceIds, *archiveFilter);
    NX_DEBUG(this, "Time periods lookup completed in %1, %2 periods found by filter %3",
        timer.elapsed(), timePeriods.size(), filter);
    return timePeriods;
}

} // namespace nx::analytics::db
