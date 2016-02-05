
#pragma once

#include <ui/workbench/workbench_context_aware.h>
#include <statistics/base_statistics_module.h>

class QnStatisticsMetric;

class QnActionsStatisticsModule : public QnBaseStatisticsModule
    , public QnWorkbenchContextAware
{
    typedef QnBaseStatisticsModule base_type;
public:
    QnActionsStatisticsModule(QObject *parent);

    virtual ~QnActionsStatisticsModule();

private:
    QnMetricsHash metrics() const override;

    void resetMetrics() override;

private:
    typedef QSharedPointer<QnStatisticsMetric> MetricsPtr;
    typedef QHash<QString, MetricsPtr> MetricsHash;

    MetricsHash m_metrics;
};