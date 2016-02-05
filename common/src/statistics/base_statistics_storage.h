
#pragma once

#include <QtCore/QObject>

#include <statistics/types.h>


class QnBaseStatisticsStorage : public QObject
{
    Q_OBJECT

    typedef QObject base_type;

public:
    QnBaseStatisticsStorage(QObject *parent = nullptr);

    virtual ~QnBaseStatisticsStorage();

public:
    virtual void storeMetrics(const QnMetricsHash &metrics) = 0;

    virtual QnMetricHashesList getMetricsList(qint64 startTimeMs
        , int limit) const = 0;

    //

    virtual bool saveCustomSettings(const QString &name
        , const QByteArray &settings) = 0;

    virtual QByteArray getCustomSettings(const QString &name) const = 0;

    virtual void removeCustomSettings(const QString &name) = 0;

};