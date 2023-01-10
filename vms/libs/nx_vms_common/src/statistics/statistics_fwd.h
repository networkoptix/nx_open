// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <QtCore/QHash>
#include <QtCore/QString>
#include <QtCore/QPointer>

class QnAbstractStatisticsModule;

class QnAbstractStatisticsStorage;
typedef std::unique_ptr<QnAbstractStatisticsStorage> QnStatisticsStoragePtr;

struct StatisticsSettings;

typedef QHash<QString, QString> QnStatisticValuesHash;

typedef QList<QnStatisticValuesHash> QnMetricHashesList;
typedef QSet<QString> QnStringsSet;
