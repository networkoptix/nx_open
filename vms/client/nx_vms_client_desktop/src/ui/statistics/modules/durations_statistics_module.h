// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/


#pragma once

#include <statistics/base/base_fwd.h>
#include <statistics/abstract_statistics_module.h>

class QnDurationStatisticsModule : public QnAbstractStatisticsModule
{
    Q_OBJECT

    typedef QnAbstractStatisticsModule base_type;

public:
    QnDurationStatisticsModule(QObject *parent);

    virtual ~QnDurationStatisticsModule();

    QnStatisticValuesHash values() const override;

    void reset() override;

private:
    const QnMetricsContainerPtr m_metrics;
};