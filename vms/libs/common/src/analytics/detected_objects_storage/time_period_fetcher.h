#pragma once

#include <nx/sql/filter.h>
#include <nx/sql/query_context.h>

#include <recording/time_period_list.h>

#include "analytics_events_storage_types.h"

namespace nx::analytics::storage {

class DeviceDao;
class ObjectTypeDao;
class TimePeriodDao;

class TimePeriodFetcher
{
public:
    TimePeriodFetcher(
        const DeviceDao& deviceDao,
        const ObjectTypeDao& objectTypeDao,
        const TimePeriodDao& timePeriodDao,
        std::chrono::milliseconds maxRecordedTimestamp);

    nx::sql::DBResult selectTimePeriods(
        nx::sql::QueryContext* queryContext,
        const Filter& filter,
        const TimePeriodsLookupOptions& options,
        QnTimePeriodList* result);

private:
    const DeviceDao& m_deviceDao;
    const ObjectTypeDao& m_objectTypeDao;
    const TimePeriodDao& m_timePeriodDao;
    const std::chrono::milliseconds m_maxRecordedTimestamp;

    void prepareSelectTimePeriodsSimpleQuery(
        nx::sql::AbstractSqlQuery* query,
        const std::vector<QnUuid>& deviceIds,
        const QnTimePeriod& timePeriod,
        const TimePeriodsLookupOptions& options);

    void prepareSelectTimePeriodsFilteredQuery(
        nx::sql::AbstractSqlQuery* query,
        const Filter& filter,
        const TimePeriodsLookupOptions& options);

    void loadTimePeriods(
        nx::sql::AbstractSqlQuery* query,
        const QnTimePeriod& timePeriod,
        const TimePeriodsLookupOptions& options,
        QnTimePeriodList* result);
};

} // namespace nx::analytics::storage
