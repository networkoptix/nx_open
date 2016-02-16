
#pragma once

#include <ui/statistics/modules/private/time_duration_metric.h>

class SessionUptimeMetric : public TimeDurationMetric
{
    typedef TimeDurationMetric base_type;

public:
    SessionUptimeMetric()
        : base_type(true)
    {}
};
