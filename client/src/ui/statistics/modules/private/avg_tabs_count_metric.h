
#pragma once

#include <QtCore/QObject>

#include <utils/common/connective.h>
#include <ui/statistics/modules/private/private_fwd.h>
#include <ui/statistics/modules/private/abstract_single_metric.h>

class QnWorkbench;

class AvgTabsCountMetric : public Connective<QObject>
    , public AbstractSingleMetric
{
    typedef Connective<QObject> base_type;

public:
    AvgTabsCountMetric(QnWorkbench *workbench);

    virtual ~AvgTabsCountMetric();

    bool significant() const override;

    QString value() const override;

    void reset() override;

private:
    typedef QPointer<QnWorkbench> QnWorkbenchPtr;
    typedef QHash<int, TimeDurationMetricPtr> TabsCountDurationsMetrics;

    const QnWorkbenchPtr m_workbench;
    TabsCountDurationsMetrics m_tabsCountDurations;
};