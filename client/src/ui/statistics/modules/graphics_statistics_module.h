
#pragma once

#include <statistics/base/base_fwd.h>
#include <statistics/abstract_statistics_module.h>
#include <ui/workbench/workbench_context_aware.h>

class QnGraphicsStatisticsModule : public QnAbstractStatisticsModule
    , public QnWorkbenchContextAware
{
    Q_OBJECT

    typedef QnAbstractStatisticsModule base_type;

public:
    QnGraphicsStatisticsModule(QObject *parent);

    virtual ~QnGraphicsStatisticsModule();

    virtual QnStatisticValuesHash values() const override;

    virtual void reset() override;

private:
    const QnMetricsContainerPtr m_metrics;
};
