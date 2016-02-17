
#pragma once

#include <statistics/base/base_fwd.h>
#include <statistics/abstract_statistics_module.h>

class QnWorkbenchContext;

class QnGraphicsStatisticsModule : public QnAbstractStatisticsModule
{
    Q_OBJECT

    typedef QnAbstractStatisticsModule base_type;

public:
    QnGraphicsStatisticsModule(QObject *parent);

    virtual ~QnGraphicsStatisticsModule();

    void setContext(QnWorkbenchContext *context);

    virtual QnStatisticValuesHash values() const override;

    virtual void reset() override;

private:
    void recreateMetrics();

private:
    typedef QPointer<QnWorkbenchContext> ContextPtr;

    ContextPtr m_context;
    QnMetricsContainerPtr m_metrics;
};
