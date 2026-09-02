// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "metrics.h"

#include <nx/reflect/enum_instrument.h>
#include <nx/reflect/enum_string_conversion.h>
#include <nx/utils/log/assert.h>

namespace nx::sql::detail {

namespace {

constexpr char kQueryTasksName[] = "nx_sql_query_tasks_total";
constexpr char kQueryTasksHelp[] = "Number of completed SQL query execution tasks";
constexpr char kQueryDurationName[] = "nx_sql_query_duration_seconds";
constexpr char kQueryDurationHelp[] = "Execution duration of SQL query execution tasks";
constexpr char kQueueWaitName[] = "nx_sql_queue_wait_seconds";
constexpr char kQueueWaitHelp[] = "Time SQL query execution tasks spent queued before execution";

const std::string kOutcomeLabel = "outcome";

// Not the HTTP defaults, which start at 5ms; 0.5 is an edge so the SQL latency alert reads one.
const std::vector<double> kDurationBuckets = {
    0.001, 0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1, 2.5, 5, 10};

constexpr char kPendingQueriesName[] = "nx_sql_queue_pending_queries";
constexpr char kPendingQueriesHelp[] = "SQL query execution tasks waiting in the queue";
constexpr char kOldestQueryAgeName[] = "nx_sql_queue_oldest_query_age_seconds";
constexpr char kOldestQueryAgeHelp[] =
    "Time the oldest queued SQL query execution task has been waiting";
constexpr char kThreadPoolSizeName[] = "nx_sql_thread_pool_size";
constexpr char kThreadPoolSizeHelp[] = "Number of DB connection threads";

// Follows NX_REFLECTION_ENUM_CLASS(QueryType, ...): adding a value cannot desynchronise the list
// from the count.
constexpr auto kQueryTypes = nx::reflect::enumeration::allEnumValues<QueryType>();
constexpr auto kOutcomes = nx::reflect::enumeration::allEnumValues<Outcome>();

const std::string kQueryTypeLabel = "query_type";

} // namespace

std::size_t queryTasksIndex(QueryType queryType, Outcome outcome)
{
    return static_cast<std::size_t>(queryType) * kOutcomes.size()
        + static_cast<std::size_t>(outcome);
}

Metrics makeMetrics(nx::prometheus::Registry* registry)
{
    const auto queryDurationFamily =
        registry->histogramFamily(kQueryDurationName, kQueryDurationHelp, kDurationBuckets);
    const auto queueWaitFamily =
        registry->histogramFamily(kQueueWaitName, kQueueWaitHelp, kDurationBuckets);

    std::vector<nx::prometheus::Counter> queryTasks;
    queryTasks.reserve(kQueryTypes.size() * kOutcomes.size());
    std::vector<nx::prometheus::Histogram> queryDuration;
    queryDuration.reserve(kQueryTypes.size());
    std::vector<nx::prometheus::Histogram> queueWait;
    queueWait.reserve(kQueryTypes.size());
    for (const auto queryType: kQueryTypes)
    {
        const nx::prometheus::Labels queryTypeLabels{
            {kQueryTypeLabel, nx::reflect::enumeration::toString(queryType)}};

        for (const auto outcome: kOutcomes)
        {
            auto labels = queryTypeLabels;
            labels.emplace(kOutcomeLabel, nx::reflect::enumeration::toString(outcome));
            NX_ASSERT(queryTasks.size() == queryTasksIndex(queryType, outcome));
            queryTasks.push_back(registry->counter(kQueryTasksName, kQueryTasksHelp, labels));
        }

        NX_ASSERT(queryDuration.size() == static_cast<std::size_t>(queryType));
        queryDuration.push_back(queryDurationFamily.withLabels(queryTypeLabels));
        queueWait.push_back(queueWaitFamily.withLabels(queryTypeLabels));
    }

    return Metrics{std::move(queryTasks),
        std::move(queryDuration),
        std::move(queueWait),
        registry->gauge(kPendingQueriesName, kPendingQueriesHelp),
        registry->gauge(kOldestQueryAgeName, kOldestQueryAgeHelp),
        registry->gauge(kThreadPoolSizeName, kThreadPoolSizeHelp)};
}

} // namespace nx::sql::detail
