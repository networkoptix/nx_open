#pragma once

#include <nx/fusion/model_functions_fwd.h>

#include <nx/sql/db_statistics_collector.h>

namespace nx::cloud::db {
namespace data {

struct Statistics
{
    int onlineServerCount = 0;
    nx::sql::QueryStatistics dbQueryStatistics;
    std::size_t pendingSqlQueryCount = 0;
};

#define Statistics_data_Fields (onlineServerCount)(dbQueryStatistics)(pendingSqlQueryCount)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (Statistics),
    (json))

} // namespace data
} // namespace nx::cloud::db
