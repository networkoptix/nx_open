
#pragma once

#include <QtCore/QObject>

#include <ui/statistics/modules/private/time_duration_metric.h>

class QnWorkbench;

class PreviewSearchDurationMetric : public QObject
    , public TimeDurationMetric
{
    typedef QObject base_type;

public:
    PreviewSearchDurationMetric(QnWorkbench *workbench);

    virtual ~PreviewSearchDurationMetric();
};
