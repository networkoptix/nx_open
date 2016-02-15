
#pragma once

#include <core/resource/resource_fwd.h>
#include <ui/workbench/workbench_context_aware.h>
#include <statistics/abstract_statistics_module.h>

class QnWorkbenchContext;

class QnUsersStatisticsModule : public QnAbstractStatisticsModule
{
    Q_OBJECT

    typedef QnAbstractStatisticsModule base_type;

public:
    QnUsersStatisticsModule(QObject *parent);

    virtual ~QnUsersStatisticsModule();

    QnMetricsHash metrics() const override;

    void resetMetrics() override;

    void setContext(QnWorkbenchContext *context);

private:
    typedef QPointer<QnWorkbenchContext> ContextPtr;
    ContextPtr m_context;
};