
#pragma once

#include <ui/workbench/workbench_context_aware.h>
#include <statistics/abstract_statistics_module.h>

class AbstractActionMetric;

class QnActionsStatisticsModule : public QnAbstractStatisticsModule
    , public QnWorkbenchContextAware
{
    typedef QnAbstractStatisticsModule base_type;
public:
    QnActionsStatisticsModule(QObject *parent);

    virtual ~QnActionsStatisticsModule();

    QnMetricsHash metrics() const override;

    void resetMetrics() override;

private:
    typedef QSharedPointer<AbstractActionMetric> MetricsPtr;
    typedef QHash<QString, MetricsPtr> MetricsHash;

    MetricsHash m_metrics;
};