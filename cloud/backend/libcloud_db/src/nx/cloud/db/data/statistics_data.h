#pragma once

#include <nx/fusion/model_functions_fwd.h>

#include <nx/sql/db_statistics_collector.h>

namespace nx {
namespace sql {

#define DurationStatistics_Fields (min)(max)(average)

#define QueryStatistics_Fields \
    (statisticalPeriod)(requestsSucceeded)(requestsFailed) \
    (requestsCancelled)(requestExecutionTimes)(waitingForExecutionTimes)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (DurationStatistics)(QueryStatistics),
    (json))

} // namespace sql
} // namespace nx

namespace nx::cloud::db {
namespace data {

struct Statistics
{
    int onlineServerCount;
    nx::sql::QueryStatistics dbQueryStatistics;
    std::size_t pendingSqlQueryCount;

    Statistics():
        onlineServerCount(0),
        pendingSqlQueryCount(0)
    {
    }
};

#define Statistics_data_Fields (onlineServerCount)(dbQueryStatistics)(pendingSqlQueryCount)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (Statistics),
    (json))

} // namespace data
} // namespace nx::cloud::db
