
#pragma once

#include <QtCore/QObject>

#include <utils/common/connective.h>
#include <ui/statistics/modules/private/time_duration_metric.h>

class QnWorkbench;

class PreviewSearchDurationMetric : public Connective<QObject>
    , public TimeDurationMetric
{
    typedef Connective<QObject> base_type;

public:
    PreviewSearchDurationMetric(QnWorkbench *workbench);

    virtual ~PreviewSearchDurationMetric();

private:
    typedef QPointer<QnWorkbench> QnWorkbenchPtr;

    const QnWorkbenchPtr m_workbench;
};