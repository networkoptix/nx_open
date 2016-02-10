
#pragma once

#include <QtCore/QHash>
#include <QtCore/QString>
#include <QtCore/QPointer>

class QnAbstractStatisticsModule;
class QnAbstractStatisticsStorage;
class QnAbstractStatisticsSettingsLoader;

typedef QScopedPointer<QnAbstractStatisticsSettingsLoader> QnStatisticsSettingsPtr;
typedef QScopedPointer<QnAbstractStatisticsStorage> QnStatisticsStoragePtr;

typedef QString QnMetricAlias;
typedef QString QnMetricValue;
typedef QHash<QnMetricAlias, QnMetricValue> QnMetricsHash;

typedef QList<QnMetricsHash> QnMetricHashesList;
typedef QSet<QString> QnStringsSet;

Q_DECLARE_METATYPE(QnMetricHashesList)