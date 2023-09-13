// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QPointer>

#include <statistics/base/abstract_metric.h>
#include <statistics/base/base_fwd.h>
#include <statistics/base/time_duration_metric.h>

namespace nx::vms::client::desktop { class Workbench; }

class AvgTabsCountMetric : public QObject
    , public QnAbstractMetric
{
    typedef QObject base_type;

public:
    AvgTabsCountMetric(nx::vms::client::desktop::Workbench *workbench);

    virtual ~AvgTabsCountMetric();

    QString value() const override;

    void reset() override;

private:
    typedef QPointer<nx::vms::client::desktop::Workbench> QnWorkbenchPtr;
    typedef QHash<int, QnTimeDurationMetricPtr> TabsCountDurationsMetrics;

    const QnWorkbenchPtr m_workbench;
    TabsCountDurationsMetrics m_tabsCountDurations;
};
