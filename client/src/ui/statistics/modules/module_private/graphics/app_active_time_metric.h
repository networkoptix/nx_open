
#pragma once

#include <QtCore/QObject>

#include <ui/statistics/modules/module_private/time_duration_metric.h>

// TODO: add correct behavior between sessions
class AppActiveTimeMetric : public QObject
    , public TimeDurationMetric
{
    typedef QObject base_type;

public:
    AppActiveTimeMetric(QObject *parent = nullptr);

    virtual ~AppActiveTimeMetric();

private:
    bool eventFilter(QObject *watched, QEvent *event) override;
};