
#pragma once

#include <QtCore/QObject>

#include <ui/statistics/modules/module_private/active_time_metric.h>

class AppActiveTimeMetric : public QObject
    , public ActiveTimeMetric
{
    typedef QObject base_type;

public:
    AppActiveTimeMetric(QObject *parent = nullptr);

    virtual ~AppActiveTimeMetric();

private:
    bool eventFilter(QObject *watched, QEvent *event) override;
};