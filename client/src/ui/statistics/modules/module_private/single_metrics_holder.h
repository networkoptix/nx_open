
#pragma once

#include <ui/statistics/modules/module_private/abstract_multimetric.h>

class AbstractSingleMetric;
typedef QSharedPointer<AbstractSingleMetric> AbstractSingleMetricPtr;

class SingleMetricsHolder : public AbstractMultimetric
{
    typedef AbstractMultimetric base_type;

public:
    SingleMetricsHolder();

    virtual ~SingleMetricsHolder();

    QnMetricsHash metrics() const override;

    void reset() override;

    void addMetric(const QString &alias
        , const AbstractSingleMetricPtr &metric);

    template<typename MetricType>
    void addMetric(const QString &alias)
    {
        addMetric(alias, AbstractSingleMetricPtr(new MetricType()));
    }

    void clearMetrics();

private:
    typedef QHash<QString, AbstractSingleMetricPtr> MetricsHash;
    MetricsHash m_metrics;
};