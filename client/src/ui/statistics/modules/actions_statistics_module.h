
#pragma once

#include <statistics/abstract_statistics_module.h>

class QnActionManager;
class AbstractActionMetric;

class QnActionsStatisticsModule : public QnAbstractStatisticsModule
{
    typedef QnAbstractStatisticsModule base_type;
public:
    QnActionsStatisticsModule(QObject *parent);

    virtual ~QnActionsStatisticsModule();

    void setActionManager(QnActionManager *manager);

    QnMetricsHash metrics() const override;

    void resetMetrics() override;

private:
    typedef QSharedPointer<AbstractActionMetric> MetricsPtr;
    typedef QHash<QString, MetricsPtr> MetricsHash;

    QnActionManager *m_actionManager;
    MetricsHash m_metrics;
};