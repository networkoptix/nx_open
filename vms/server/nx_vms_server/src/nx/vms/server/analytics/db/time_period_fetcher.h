#pragma once

#include <nx/sql/filter.h>
#include <nx/sql/query_context.h>

#include <recording/time_period_list.h>

#include <analytics/db/analytics_db_types.h>

#include "analytics_archive_directory.h"

namespace nx::analytics::db {

class DeviceDao;
class ObjectTypeDao;
class TimePeriodDao;
class AttributesDao;
class AnalyticsArchiveDirectory;

class TimePeriodFetcher
{
public:
    TimePeriodFetcher(
        const DeviceDao& deviceDao,
        const ObjectTypeDao& objectTypeDao,
        const TimePeriodDao& timePeriodDao,
        AttributesDao* attributesDao,
        AnalyticsArchiveDirectory* analyticsArchive,
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
    AttributesDao* m_attributesDao = nullptr;
    AnalyticsArchiveDirectory* m_analyticsArchive = nullptr;
    const std::chrono::milliseconds m_maxRecordedTimestamp;

    QnTimePeriodList selectTimePeriodsByObject(
        nx::sql::QueryContext* queryContext,
        const Filter& filter,
        const TimePeriodsLookupOptions& options);

    QnTimePeriodList selectFullTimePeriods(
        nx::sql::QueryContext* queryContext,
        const std::vector<QnUuid>& deviceIds,
        const QnTimePeriod& timePeriod,
        const TimePeriodsLookupOptions& options);

    void prepareSelectTimePeriodsSimpleQuery(
        nx::sql::AbstractSqlQuery* query,
        const std::vector<QnUuid>& deviceIds,
        const QnTimePeriod& timePeriod,
        const TimePeriodsLookupOptions& options);

    QnTimePeriodList selectTimePeriodsFiltered(
        nx::sql::QueryContext* queryContext,
        const Filter& filter,
        const TimePeriodsLookupOptions& options);

    AnalyticsArchive::Filter prepareArchiveFilter(
        const Filter& filter,
        const TimePeriodsLookupOptions& options);

    QnTimePeriodList loadTimePeriods(
        nx::sql::AbstractSqlQuery* query,
        const QnTimePeriod& timePeriod,
        const TimePeriodsLookupOptions& options);
};

} // namespace nx::analytics::db
