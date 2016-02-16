
#include "single_metrics_holder.h"

#include <ui/statistics/modules/private/time_duration_metric.h>

SingleMetricsHolder::SingleMetricsHolder()
    : base_type()
    , m_metrics()
{}

SingleMetricsHolder::~SingleMetricsHolder()
{}

QnMetricsHash SingleMetricsHolder::metrics() const
{
    QnMetricsHash result;
    for (auto it = m_metrics.begin(); it != m_metrics.end(); ++it)
    {
        const auto metric = it.value();
        if (!metric->significant())
            continue;

        const auto alias = it.key();
        result.insert(alias, metric->value());
    }

    return result;
}

void SingleMetricsHolder::reset()
{
    for(const auto metric: m_metrics)
        metric->reset();
}

void SingleMetricsHolder::addMetric(const QString &alias
    , const AbstractSingleMetricPtr &metric)
{
    m_metrics.insert(alias, metric);
}

void SingleMetricsHolder::clearMetrics()
{
    m_metrics.clear();
}
