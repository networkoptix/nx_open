// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "avg_tabs_count_metric.h"

#include <nx/vms/client/desktop/workbench/workbench.h>
#include <statistics/base/time_duration_metric.h>

using namespace nx::vms::client::desktop;

AvgTabsCountMetric::AvgTabsCountMetric(Workbench *workbench)
    : base_type()
    , QnAbstractMetric()
    , m_workbench(workbench)
    , m_tabsCountDurations()
{
    NX_ASSERT(m_workbench, "Workbench is null");
    if (!m_workbench)
        return;

    const QPointer<AvgTabsCountMetric> guard(this);
    const auto layoutsCountChangedHandler = [this, guard]()
    {
        if (!guard || !m_workbench)
            return;

        for (const auto &metric: m_tabsCountDurations)
            metric->setCounterActive(false);

        const auto layoutsCount = m_workbench->layouts().size();
        auto it = m_tabsCountDurations.find(layoutsCount);
        if (it == m_tabsCountDurations.end())
        {
            it = m_tabsCountDurations.insert(layoutsCount
                , QnTimeDurationMetricPtr(new QnTimeDurationMetric()));
        }

        const auto durationCounter = *it;
        durationCounter->setCounterActive(true);
    };

    connect(m_workbench, &Workbench::layoutsChanged, this, layoutsCountChangedHandler);
}

AvgTabsCountMetric::~AvgTabsCountMetric()
{}

QString AvgTabsCountMetric::value() const
{
    double sum = 0.0;
    qint64 overalDuration = 0;
    for (auto it = m_tabsCountDurations.cbegin();
        it != m_tabsCountDurations.cend(); ++it)
    {
        const auto tabsCount = it.key();
        const auto durationCounter = it.value();

        const auto duration = durationCounter->duration();
        sum += tabsCount * duration;
        overalDuration += duration;
    }

    return QString::number(overalDuration == 0 ? 0 : sum / overalDuration);
}

void AvgTabsCountMetric::reset()
{
    for (const auto &metric: m_tabsCountDurations)
        metric->reset();
}
