#pragma once

#include <api/helpers/multiserver_request_data.h>

typedef QHash<QString, QString> QnStatisticValuesHash;

typedef QList<QnStatisticValuesHash> QnMetricHashesList;

// TODO: #GDM #3.1 move to api namespace in common module together with defines
struct QnSendStatisticsRequestData : public QnMultiserverRequestData
{
    void loadFromParams(QnResourcePool* resourcePool, const QnRequestParamList& params) override;

    QnRequestParamList toParams() const override;

    QString statisticsServerUrl;
    QnMetricHashesList metricsList;
};
