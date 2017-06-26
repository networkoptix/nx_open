#pragma once

#include <nx/fusion/model_functions_fwd.h>

#include <nx/utils/db/db_statistics_collector.h>

namespace nx {
namespace db {

#define DurationStatistics_Fields (min)(max)(average)

#define QueryStatistics_Fields \
    (statisticalPeriod)(requestsSucceeded)(requestsFailed) \
    (requestsCancelled)(requestExecutionTimes)(waitingForExecutionTimes)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (DurationStatistics)(QueryStatistics),
    (json))

} // namespace db
} // namespace nx

namespace nx {
namespace cdb {
namespace data {

struct Statistics
{
    int onlineServerCount;
    nx::utils::db::QueryStatistics dbQueryStatistics;
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
} // namespace cdb
} // namespace nx
