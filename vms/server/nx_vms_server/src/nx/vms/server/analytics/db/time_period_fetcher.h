#pragma once

#include <nx/sql/filter.h>
#include <nx/sql/query_context.h>

#include <recording/time_period_list.h>

#include <analytics/db/analytics_db_types.h>

#include "analytics_archive_directory.h"

namespace nx::analytics::db {

class DeviceDao;
class ObjectTypeDao;
class AttributesDao;
class AnalyticsArchiveDirectory;

class TimePeriodFetcher
{
public:
    TimePeriodFetcher(
        const DeviceDao& deviceDao,
        const ObjectTypeDao& objectTypeDao,
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
    AttributesDao* m_attributesDao = nullptr;
    AnalyticsArchiveDirectory* m_analyticsArchive = nullptr;
    const std::chrono::milliseconds m_maxRecordedTimestamp;

    QnTimePeriodList selectTimePeriodsByObject(
        nx::sql::QueryContext* queryContext,
        const Filter& filter,
        const TimePeriodsLookupOptions& options);

    QnTimePeriodList selectTimePeriodsFiltered(
        nx::sql::QueryContext* queryContext,
        const Filter& filter,
        const TimePeriodsLookupOptions& options);
};

} // namespace nx::analytics::db
