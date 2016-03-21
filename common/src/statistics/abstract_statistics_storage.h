
#pragma once

#include <QtCore/QObject>

#include <statistics/statistics_fwd.h>


class QnAbstractStatisticsStorage
{
public:
    QnAbstractStatisticsStorage();

    virtual ~QnAbstractStatisticsStorage();

public:
    virtual void storeMetrics(const QnStatisticValuesHash &metrics) = 0;

    virtual QnMetricHashesList getMetricsList(qint64 startTimeMs
        , int limit) const = 0;

    //

    virtual bool saveCustomSettings(const QString &name
        , const QByteArray &settings) = 0;

    virtual QByteArray getCustomSettings(const QString &name) const = 0;

    virtual void removeCustomSettings(const QString &name) = 0;

};