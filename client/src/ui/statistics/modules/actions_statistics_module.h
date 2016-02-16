
#pragma once

#include <statistics/abstract_statistics_module.h>

class QnActionManager;
class AbstractMultimetric;
typedef QPointer<QnActionManager> QnActionManagerPtr;

class QnActionsStatisticsModule : public QnAbstractStatisticsModule
{
    Q_OBJECT

    typedef QnAbstractStatisticsModule base_type;

public:
    QnActionsStatisticsModule(QObject *parent);

    virtual ~QnActionsStatisticsModule();

    void setActionManager(const QnActionManagerPtr &manager);

    QnMetricsHash metrics() const override;

    void resetMetrics() override;

private:
    typedef QSharedPointer<AbstractMultimetric> MultiMetricsPtr;
    typedef QList<MultiMetricsPtr> MultimetricsList;

    QnActionManagerPtr m_actionManager;
    MultimetricsList m_multiMetrics;
};