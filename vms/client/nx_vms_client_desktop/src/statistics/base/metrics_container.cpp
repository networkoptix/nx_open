
#include "metrics_container.h"

#include <statistics/base/abstract_metric.h>

QnMetricsContainer::QnMetricsContainer()
    : base_type()
    , m_metrics()
{}

QnMetricsContainer::~QnMetricsContainer()
{}

QnStatisticValuesHash QnMetricsContainer::values() const
{
    QnStatisticValuesHash result;
    for (auto it = m_metrics.begin(); it != m_metrics.end(); ++it)
    {
        const auto metric = it.value();
        if (!metric->isSignificant())
            continue;

        const auto alias = it.key();
        result.insert(alias, metric->value());
    }

    return result;
}

void QnMetricsContainer::reset()
{
    for(const auto metric: m_metrics)
        metric->reset();
}

void QnMetricsContainer::addMetric(const QString &alias
    , const QnAbstractMetricPtr &metric)
{
    m_metrics.insert(alias, metric);
}

void QnMetricsContainer::clearMetrics()
{
    m_metrics.clear();
}
