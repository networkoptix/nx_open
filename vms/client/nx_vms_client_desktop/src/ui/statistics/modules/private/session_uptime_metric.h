
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
