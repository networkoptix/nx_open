
#pragma once

#include <QtCore/QObject>

#include <statistics/base/time_duration_metric.h>

class QnWorkbench;

class PreviewSearchDurationMetric : public QObject
    , public QnTimeDurationMetric
{
    typedef QObject base_type;

public:
    PreviewSearchDurationMetric(QnWorkbench *workbench);

    virtual ~PreviewSearchDurationMetric();
};
