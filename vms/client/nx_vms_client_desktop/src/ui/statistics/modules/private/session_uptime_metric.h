// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/


#pragma once

#include <statistics/base/time_duration_metric.h>

class SessionUptimeMetric : public QnTimeDurationMetric
{
    typedef QnTimeDurationMetric base_type;

public:
    SessionUptimeMetric()
        : base_type(true)
    {}
};
