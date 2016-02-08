
#pragma once

#include <statistics/types.h>
#include <api/helpers/multiserver_request_data.h>

struct QnSendStatisticsRequestData : public QnMultiserverRequestData
{
    QnMetricHashesList metricsList;
};
