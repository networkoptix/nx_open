
#include "abstract_action_metric.h"

AbstractActionMetric::AbstractActionMetric()
{}

AbstractActionMetric::~AbstractActionMetric()
{}

QnMetricsHash AbstractActionMetric::metrics() const
{
    Q_ASSERT_X(false, Q_FUNC_INFO, "Pure virtual method called!");
    return QnMetricsHash();
}
