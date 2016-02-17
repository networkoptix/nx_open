
#pragma once

#include <statistics/base/base_fwd.h>
#include <statistics/base/statistics_values_provider.h>

class QnMetricsContainer : public QnStatisticsValuesProvider
{
    typedef QnStatisticsValuesProvider base_type;

public:
    QnMetricsContainer();

    virtual ~QnMetricsContainer();

    void addMetric(const QString &alias
        , const QnAbstractMetricPtr &metric);

    template<typename MetricType>
    void addMetric(const QString &alias)
    {
        addMetric(alias, QnAbstractMetricPtr(new MetricType()));
    }

    void clearMetrics();

public:
    QnStatisticValuesHash values() const override;

    void reset() override;

private:
    typedef QHash<QString, QnAbstractMetricPtr> MetricsHash;
    MetricsHash m_metrics;
};