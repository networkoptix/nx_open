
#pragma once

#include <statistics/statistics_fwd.h>

class AbstractActionMetric
{
public:
    AbstractActionMetric();

    virtual ~AbstractActionMetric();

    virtual QnMetricsHash metrics() const = 0;

    virtual void reset() = 0;
};