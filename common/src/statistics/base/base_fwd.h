
#pragma once

#include <QtCore/QSharedPointer>

class QnAbstractMetric;
typedef QSharedPointer<QnAbstractMetric> QnAbstractMetricPtr;

class QnTimeDurationMetric;
typedef QSharedPointer<QnTimeDurationMetric> QnTimeDurationMetricPtr;

class QnMetricsContainer;
typedef QSharedPointer<QnMetricsContainer> QnMetricsContainerPtr;

class QnStatisticsValuesProvider;
typedef QSharedPointer<QnStatisticsValuesProvider> QnStatisticsValuesProviderPtr;
