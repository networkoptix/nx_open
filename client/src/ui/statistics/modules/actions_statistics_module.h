
#pragma once

#include <statistics/abstract_statistics_module.h>

#include <utils/common/connective.h>

class QnActionManager;
class AbstractActionMetric;
typedef QPointer<QnActionManager> QnActionManagerPtr;

class QnActionsStatisticsModule : public Connective<QnAbstractStatisticsModule>
{
    typedef Connective<QnAbstractStatisticsModule> base_type;

public:
    QnActionsStatisticsModule(QObject *parent);

    virtual ~QnActionsStatisticsModule();

    void setActionManager(const QnActionManagerPtr &manager);

    QnMetricsHash metrics() const override;

    void resetMetrics() override;

private:
    typedef QSharedPointer<AbstractActionMetric> MetricsPtr;
    typedef QHash<QString, MetricsPtr> MetricsHash;

    QnActionManagerPtr m_actionManager;
    MetricsHash m_metrics;
};