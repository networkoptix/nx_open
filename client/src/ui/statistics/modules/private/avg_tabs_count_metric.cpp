
#include "avg_tabs_count_metric.h"

#include <ui/workbench/workbench.h>
#include <ui/statistics/modules/private/time_duration_metric.h>

AvgTabsCountMetric::AvgTabsCountMetric(QnWorkbench *workbench)
    : base_type()
    , AbstractSingleMetric()
    , m_workbench(workbench)
    , m_tabsCountDurations()
{
    if (!m_workbench)
        return;

    const QPointer<AvgTabsCountMetric> guard(this);
    const auto layoutsCountChangedHandler = [this, guard]()
    {
        if (!guard || !m_workbench)
            return;

        for (const auto &metric: m_tabsCountDurations)
            metric->activateCounter(false);

        const auto layoutsCount = m_workbench->layouts().size();
        auto it = m_tabsCountDurations.find(layoutsCount);
        if (it == m_tabsCountDurations.end())
        {
            it = m_tabsCountDurations.insert(layoutsCount
                , TimeDurationMetricPtr(new TimeDurationMetric()));
        }

        const auto durationCounter = *it;
        durationCounter->activateCounter(true);
    };

    connect(m_workbench, &QnWorkbench::layoutsChanged
        , this, layoutsCountChangedHandler);
}

AvgTabsCountMetric::~AvgTabsCountMetric()
{}

bool AvgTabsCountMetric::significant() const
{
    return true;
}

QString AvgTabsCountMetric::value() const
{
    double sum = 0.0;
    qint64 overalDuration = 0.0;
    for (auto it = m_tabsCountDurations.cbegin();
        it != m_tabsCountDurations.cend(); ++it)
    {
        const auto tabsCount = it.key();
        const auto durationCounter = it.value();

        const auto duration = durationCounter->duration();
        sum += tabsCount * duration;
        overalDuration += duration;
    }

    if (!overalDuration)
        return QString::number(0);

    return QString::number(sum / static_cast<double>(overalDuration));
}

void AvgTabsCountMetric::reset()
{
    for (const auto &metric: m_tabsCountDurations)
        metric->reset();
}