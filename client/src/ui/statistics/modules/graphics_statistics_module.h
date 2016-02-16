
#pragma once

#include <statistics/abstract_statistics_module.h>

class QnWorkbenchContext;
class SingleMetricsHolder;

class QnGraphicsStatisticsModule : public QnAbstractStatisticsModule
{
    Q_OBJECT

    typedef QnAbstractStatisticsModule base_type;

public:
    QnGraphicsStatisticsModule(QObject *parent);

    virtual ~QnGraphicsStatisticsModule();

    void setContext(QnWorkbenchContext *context);

    virtual QnMetricsHash metrics() const override;

    virtual void resetMetrics() override;

private:
    void recreateMetrics();

private:
    typedef QSharedPointer<SingleMetricsHolder> BaseMultimetricPtr;
    typedef QPointer<QnWorkbenchContext> ContextPtr;

    ContextPtr m_context;
    BaseMultimetricPtr m_metrics;
};
