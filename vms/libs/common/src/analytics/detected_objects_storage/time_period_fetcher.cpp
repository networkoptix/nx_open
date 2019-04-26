#include "time_period_fetcher.h"

#include <nx/sql/query.h>

#include "config.h"
#include "device_dao.h"
#include "object_searcher.h"
#include "serializers.h"
#include "time_period_dao.h"

namespace nx::analytics::storage {

static const QSize kSearchResolution(
    kTrackSearchResolutionX,
    kTrackSearchResolutionY);

TimePeriodFetcher::TimePeriodFetcher(
    const DeviceDao& deviceDao,
    const ObjectTypeDao& objectTypeDao,
    const TimePeriodDao& timePeriodDao,
    std::chrono::milliseconds maxRecordedTimestamp)
    :
    m_deviceDao(deviceDao),
    m_objectTypeDao(objectTypeDao),
    m_timePeriodDao(timePeriodDao),
    m_maxRecordedTimestamp(maxRecordedTimestamp)
{
}

nx::sql::DBResult TimePeriodFetcher::selectTimePeriods(
    nx::sql::QueryContext* queryContext,
    const Filter& filter,
    const TimePeriodsLookupOptions& options,
    QnTimePeriodList* result)
{
    auto query = queryContext->connection()->createQuery();
    query->setForwardOnly(true);

    Filter localFilter = filter;
    localFilter.deviceIds.clear();
    localFilter.timePeriod.clear();

    if (localFilter.empty())
    {
        prepareSelectTimePeriodsSimpleQuery(
            query.get(), filter.deviceIds, filter.timePeriod, options);
    }
    else
    {
        prepareSelectTimePeriodsFilteredQuery(query.get(), filter, options);
    }

    query->exec();
    loadTimePeriods(query.get(), filter.timePeriod, options, result);

    return nx::sql::DBResult::ok;
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

static constexpr char kFromObject[] = "%fromObject%";
static constexpr char kObjectExpr[] = "%objectExpr%";
static constexpr char kFullTextFrom[] = "%fullTextFrom%";
static constexpr char kFullTextExpr[] = "%fullTextExpr%";
static constexpr char kJoinObjectToObjectSearch[] = "%joinObjectToObjectSearch%";
static constexpr char kBoundingBoxExpr[] = "%boundingBoxExpr%";

void TimePeriodFetcher::prepareSelectTimePeriodsFilteredQuery(
    nx::sql::AbstractSqlQuery* query,
    const Filter& filter,
    const TimePeriodsLookupOptions& /*options*/)
{
    using namespace std::chrono;

    std::map<QString, QString> queryTextParams({
        {kFromObject, ""},
        {kObjectExpr, ""},
        {kFullTextFrom, ""},
        {kFullTextExpr, ""},
        {kJoinObjectToObjectSearch, ""},
        {kBoundingBoxExpr, ""},
    });

    QString queryText = lm(R"sql(
        SELECT DISTINCT os.timestamp_seconds_utc * 1000 AS period_start_ms,
            %1 AS duration_ms,
            -1 AS id
        FROM
			object_search os
		    %fromObject%
			%fullTextFrom%
        WHERE
			%objectExpr%
			%fullTextExpr%
			%boundingBoxExpr%
            %joinObjectToObjectSearch%
			1 = 1
        ORDER BY period_start_ms ASC
    )sql").args(duration_cast<milliseconds>(kTrackAggregationPeriod).count());

    nx::sql::Filter objectFilter;
    ObjectSearcher::addObjectFilterConditions(
        filter,
        m_deviceDao,
        m_objectTypeDao,
        {"guid", {"track_start_ms", "track_end_ms"}},
        &objectFilter);

    auto objectFilterSqlText = objectFilter.toString();
    if (!objectFilterSqlText.empty())
        queryTextParams[kObjectExpr] = (objectFilterSqlText + " AND ").c_str();

    if (!objectFilterSqlText.empty() || !filter.freeText.isEmpty())
    {
        queryTextParams[kFromObject] = ", object o, object_search_to_object os_to_o";
        queryTextParams[kJoinObjectToObjectSearch] =
            "os_to_o.object_search_id = os.id AND os_to_o.object_id = o.id AND ";
    }

    if (!filter.freeText.isEmpty())
    {
        queryTextParams[kFullTextFrom] = ", attributes_text_index fts";
        queryTextParams[kFullTextExpr] =
            "fts.content MATCH :textQuery AND fts.docid = o.attributes_id AND ";
    }

    nx::sql::Filter boundingBoxFilter;
    if (!filter.boundingBox.isEmpty())
    {
        ObjectSearcher::addBoundingBoxToFilter(
            translate(filter.boundingBox, kSearchResolution),
            &boundingBoxFilter);
        queryTextParams[kBoundingBoxExpr] = (boundingBoxFilter.toString() + " AND ").c_str();
    }

    for (const auto& [name, value]: queryTextParams)
        queryText.replace(name, value);

    query->prepare(queryText);

    objectFilter.bindFields(query);
    if (!filter.freeText.isEmpty())
        query->bindValue(":textQuery", filter.freeText + "*");
    if (!filter.boundingBox.isEmpty())
        boundingBoxFilter.bindFields(query);
}

void TimePeriodFetcher::loadTimePeriods(
    nx::sql::AbstractSqlQuery* query,
    const QnTimePeriod& timePeriodFilter,
    const TimePeriodsLookupOptions& options,
    QnTimePeriodList* result)
{
    using namespace std::chrono;

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

        if (!result->empty() && result->back() == timePeriod)
            continue;

        result->push_back(timePeriod);
    }

    *result = QnTimePeriodList::aggregateTimePeriodsUnconstrained(
        *result, std::max<milliseconds>(options.detailLevel, kMinTimePeriodAggregationPeriod));
}

} // namespace nx::analytics::storage
