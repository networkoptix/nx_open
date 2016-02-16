
#pragma once

#include <statistics/statistics_fwd.h>

class AbstractMultimetric
{
public:
    AbstractMultimetric();

    virtual ~AbstractMultimetric();

    virtual QnMetricsHash metrics() const = 0;

    virtual void reset() = 0;
};