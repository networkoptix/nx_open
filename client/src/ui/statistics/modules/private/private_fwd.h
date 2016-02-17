
#pragma once

#include <QtCore/QSharedPointer>

class AbstractSingleMetric;
typedef QSharedPointer<AbstractSingleMetric> AbstractSingleMetricPtr;

class TimeDurationMetric;
typedef QSharedPointer<TimeDurationMetric> TimeDurationMetricPtr;