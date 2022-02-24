// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/


#pragma once

#include <QtCore/QObject>

#include <statistics/base/time_duration_metric.h>

class AppActiveTimeMetric : public QObject
    , public QnTimeDurationMetric
{
    typedef QObject base_type;

public:
    AppActiveTimeMetric(QObject *parent = nullptr);

    virtual ~AppActiveTimeMetric();

private:
    bool eventFilter(QObject *watched, QEvent *event) override;
};