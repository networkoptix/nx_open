
#pragma once

#include <QtCore/QHash>
#include <QtCore/QString>
#include <QtCore/QPointer>

typedef QString QnMetricAlias;
typedef QString QnMetricValue;
typedef QHash<QnMetricAlias, QnMetricValue> QnMetricsHash;

typedef QList<QnMetricsHash> QnMetricHashesList;
typedef QSet<QString> QnStringsSet;

Q_DECLARE_METATYPE(QnMetricHashesList)