
#pragma once

#include <statistics/statistics_fwd.h>
#include <api/helpers/multiserver_request_data.h>

struct QnSendStatisticsRequestData : public QnMultiserverRequestData
{
    void loadFromParams(const QnRequestParamList& params) override;

    QnRequestParamList toParams() const override;

    QString statisticsServerUrl;
    QnMetricHashesList metricsList;
};
