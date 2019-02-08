#pragma once

#include <memory>

#include <QtCore/QHash>
#include <QtCore/QString>
#include <QtCore/QPointer>

class QnAbstractStatisticsModule;

class QnAbstractStatisticsStorage;
typedef std::unique_ptr<QnAbstractStatisticsStorage> QnStatisticsStoragePtr;

struct StatisticsSettings;

class QnAbstractStatisticsSettingsLoader;
typedef std::unique_ptr<QnAbstractStatisticsSettingsLoader> QnStatisticsLoaderPtr;

typedef QHash<QString, QString> QnStatisticValuesHash;

typedef QList<QnStatisticValuesHash> QnMetricHashesList;
typedef QSet<QString> QnStringsSet;

Q_DECLARE_METATYPE(QnMetricHashesList)