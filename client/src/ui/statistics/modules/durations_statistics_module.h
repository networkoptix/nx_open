
#pragma once

#include <statistics/abstract_statistics_module.h>

class SingleMetricsHolder;

class QnDurationStatisticsModule : public QnAbstractStatisticsModule
{
    Q_OBJECT

    typedef QnAbstractStatisticsModule base_type;

public:
    QnDurationStatisticsModule(QObject *parent);

    virtual ~QnDurationStatisticsModule();

    QnMetricsHash metrics() const override;

    void resetMetrics() override;

private:
    typedef QSharedPointer<SingleMetricsHolder> BaseMultimetricPtr;

    const BaseMultimetricPtr m_metrics;
};