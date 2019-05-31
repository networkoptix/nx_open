#include "time_period_fetcher.h"

#include <nx/sql/query.h>
#include <nx/utils/elapsed_timer.h>

#include <analytics/db/config.h>

#include "attributes_dao.h"
#include "device_dao.h"
#include "object_searcher.h"
#include "serializers.h"
#include "time_period_dao.h"

namespace nx::analytics::db {

static const QSize kSearchResolution(
    kTrackSearchResolutionX,
    kTrackSearchResolutionY);

TimePeriodFetcher::TimePeriodFetcher(
    const DeviceDao& deviceDao,
    const ObjectTypeDao& objectTypeDao,
    const TimePeriodDao& timePeriodDao,
    AttributesDao* attributesDao,
    AnalyticsArchiveDirectory* analyticsArchive,
    std::chrono::milliseconds maxRecordedTimestamp)
    :
    m_deviceDao(deviceDao),
    m_objectTypeDao(objectTypeDao),
    m_timePeriodDao(timePeriodDao),
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

QnTimePeriodList TimePeriodFetcher::selectFullTimePeriods(
    nx::sql::QueryContext* queryContext,
    const std::vector<QnUuid>& deviceIds,
    const QnTimePeriod& timePeriod,
    const TimePeriodsLookupOptions& options)
{
    auto query = queryContext->connection()->createQuery();
    query->setForwardOnly(true);
    prepareSelectTimePeriodsSimpleQuery(
        query.get(), deviceIds, timePeriod, options);
    query->exec();

    return loadTimePeriods(query.get(), timePeriod, options);
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

void TimePeriodFetcher::prepareSelectTimePeriodsSimpleQuery(
    nx::sql::AbstractSqlQuery* query,
    const std::vector<QnUuid>& deviceGuids,
    const QnTimePeriod& timePeriod,
    const TimePeriodsLookupOptions& /*options*/)
{
    nx::sql::Filter sqlFilter;

    if (!deviceGuids.empty())
    {
        auto condition = std::make_unique<nx::sql::SqlFilterFieldAnyOf>(
            "device_id", ":deviceId");
        for (const auto& deviceGuid: deviceGuids)
            condition->addValue(m_deviceDao.deviceIdFromGuid(deviceGuid));
        sqlFilter.addCondition(std::move(condition));
    }

    if (!timePeriod.isEmpty())
    {
        auto localTimePeriod = timePeriod;
        if (localTimePeriod.durationMs == QnTimePeriod::kInfiniteDuration)
            localTimePeriod.setEndTime(m_maxRecordedTimestamp);
        else if (localTimePeriod.endTime() > m_maxRecordedTimestamp)
            localTimePeriod.durationMs = QnTimePeriod::kInfiniteDuration;

        ObjectSearcher::addTimePeriodToFilter<std::chrono::milliseconds>(
            localTimePeriod,
            {"period_end_ms", "period_start_ms"},
            &sqlFilter);
    }

    std::string whereClause;
    const auto sqlFilterStr = sqlFilter.toString();
    if (!sqlFilterStr.empty())
        whereClause = "WHERE " + sqlFilterStr;

    query->prepare(lm(R"sql(
        SELECT id, device_id, period_start_ms, period_end_ms - period_start_ms AS duration_ms
        FROM time_period_full
        %1
        ORDER BY period_start_ms ASC
    )sql").args(whereClause));
    sqlFilter.bindFields(query);
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

QnTimePeriodList TimePeriodFetcher::loadTimePeriods(
    nx::sql::AbstractSqlQuery* query,
    const QnTimePeriod& timePeriodFilter,
    const TimePeriodsLookupOptions& options)
{
    using namespace std::chrono;

    QnTimePeriodList result;

    while (query->next())
    {
        QnTimePeriod timePeriod(
            milliseconds(query->value("period_start_ms").toLongLong()),
            milliseconds(query->value("duration_ms").toLongLong()));

        // Fixing time period end if needed.
        const auto id = query->value("id").toLongLong();
        if (auto detailTimePeriod = m_timePeriodDao.getTimePeriodById(id);
            detailTimePeriod)
        {
            timePeriod.setEndTime(detailTimePeriod->endTime);
        }

        // Truncating time period by the filter
        if (!timePeriodFilter.isEmpty())
        {
            if (timePeriod.startTime() < timePeriodFilter.startTime())
                timePeriod.setStartTime(timePeriodFilter.startTime());

            if (timePeriodFilter.durationMs != QnTimePeriod::kInfiniteDuration &&
                timePeriod.endTime() > timePeriodFilter.endTime())
            {
                timePeriod.setEndTime(timePeriodFilter.endTime());
            }
        }

        if (!result.empty() && result.back() == timePeriod)
            continue;

        result.push_back(timePeriod);
    }

    return QnTimePeriodList::aggregateTimePeriodsUnconstrained(
        result, std::max<milliseconds>(options.detailLevel, kMinTimePeriodAggregationPeriod));
}

} // namespace nx::analytics::db
