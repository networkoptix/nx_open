
#pragma once

#include <QtCore/QObject>

#include <utils/common/connective.h>
#include <statistics/base/base_fwd.h>
#include <statistics/base/abstract_metric.h>

class QnWorkbench;

class AvgTabsCountMetric : public Connective<QObject>
    , public QnAbstractMetric
{
    typedef Connective<QObject> base_type;

public:
    AvgTabsCountMetric(QnWorkbench *workbench);

    virtual ~AvgTabsCountMetric();

    QString value() const override;

    void reset() override;

private:
    typedef QPointer<QnWorkbench> QnWorkbenchPtr;
    typedef QHash<int, QnTimeDurationMetricPtr> TabsCountDurationsMetrics;

    const QnWorkbenchPtr m_workbench;
    TabsCountDurationsMetrics m_tabsCountDurations;
};