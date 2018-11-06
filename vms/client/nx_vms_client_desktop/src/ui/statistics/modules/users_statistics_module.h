
#pragma once

#include <core/resource/resource_fwd.h>
#include <ui/workbench/workbench_context_aware.h>
#include <statistics/abstract_statistics_module.h>

class QnUsersStatisticsModule : public QnAbstractStatisticsModule
    , public QnWorkbenchContextAware
{
    Q_OBJECT

    typedef QnAbstractStatisticsModule base_type;

public:
    QnUsersStatisticsModule(QObject *parent);

    virtual ~QnUsersStatisticsModule();

    QnStatisticValuesHash values() const override;

    void reset() override;
};