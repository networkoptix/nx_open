// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <cstddef>
#include <vector>

#include <nx/prometheus/registry.h>
#include <nx/reflect/enum_instrument.h>

#include "types.h"

namespace nx::sql::detail {

/** Value of the outcome label of nx_sql_query_tasks_total. */
NX_REFLECTION_ENUM_CLASS(Outcome, ok, failed, cancelled)

/**
 * Metric handles resolved once, at construction, so that recording a task costs no registry
 * lookup at all.
 */
struct Metrics
{
    // Indexed by queryTasksIndex(): one counter per query type and outcome.
    std::vector<nx::prometheus::Counter> queryTasks;
    // Both indexed by the query type, so that observing costs no per-label-set lookup.
    std::vector<nx::prometheus::Histogram> queryDuration;
    std::vector<nx::prometheus::Histogram> queueWait;
    nx::prometheus::Gauge pendingQueries;
    nx::prometheus::Gauge oldestQueryAge;
    nx::prometheus::Gauge threadPoolSize;
    // Last member: dropping it stops the collector before the gauges it writes go away.
    nx::prometheus::CollectorHandle collector;
};

std::size_t queryTasksIndex(QueryType queryType, Outcome outcome);

/** Registers every series of Metrics, leaving Metrics::collector unset. */
Metrics makeMetrics(nx::prometheus::Registry* registry);

} // namespace nx::sql::detail
