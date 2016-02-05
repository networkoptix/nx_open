
#include "statistics_metric.h"

QnStatisticsMetric::QnStatisticsMetric()
{}

QnStatisticsMetric::~QnStatisticsMetric()
{}

QnMetricsHash QnStatisticsMetric::metrics() const
{
    Q_ASSERT_X(false, Q_FUNC_INFO, "Pure virtual method called!");
    return QnMetricsHash();
}
