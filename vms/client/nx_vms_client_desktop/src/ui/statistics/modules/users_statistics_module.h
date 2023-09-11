// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>
#include <statistics/abstract_statistics_module.h>
#include <ui/workbench/workbench_context_aware.h>

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
