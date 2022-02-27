// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <api/helpers/multiserver_request_data.h>

typedef QHash<QString, QString> QnStatisticValuesHash;

typedef QList<QnStatisticValuesHash> QnMetricHashesList;

// TODO: #sivanov Move to api namespace in common module together with defines.
struct NX_VMS_COMMON_API QnSendStatisticsRequestData: public QnMultiserverRequestData
{
    void loadFromParams(
        QnResourcePool* resourcePool, const nx::network::rest::Params& params) override;

    nx::network::rest::Params toParams() const override;

    QString statisticsServerUrl;
    QnMetricHashesList metricsList;
};
