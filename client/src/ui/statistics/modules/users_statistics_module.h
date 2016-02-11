
#pragma once

#include <ui/workbench/workbench_context_aware.h>
#include <statistics/abstract_statistics_module.h>

/// Stores current user permissions
class QnUsersStatisticsModule : public QnAbstractStatisticsModule
    , public QnWorkbenchContextAware
{
    typedef QnAbstractStatisticsModule base_type;

public:
    QnUsersStatisticsModule(QObject *parent);

    virtual ~QnUsersStatisticsModule();

    QnMetricsHash metrics() const override;

    void resetMetrics() override;

private:
    quint64 m_permissions;
};