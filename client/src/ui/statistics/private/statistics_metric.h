
#pragma once

#include <ui/statistics/types.h>

class QnStatisticsMetric
{
public:
    QnStatisticsMetric();

    virtual ~QnStatisticsMetric();

    virtual QnMetricsHash metrics() const = 0;

    virtual void reset() = 0;
};