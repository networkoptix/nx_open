// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSet>

#include <statistics/statistics_fwd.h>
#include <nx/fusion/model_functions_fwd.h>

struct NX_VMS_COMMON_API QnStatisticsSettings
{
    int limit = 1;
    int storeDays = 30;
    int minSendPeriodSecs = 60 * 60; //< 1 hour.
    QnStringsSet filters;
    QString statisticsServerUrl;

    bool operator==(const QnStatisticsSettings& other) const = default;
};

#define QnStatisticsSettings_Fields (limit)(storeDays)(minSendPeriodSecs)(filters)(statisticsServerUrl)
QN_FUSION_DECLARE_FUNCTIONS(QnStatisticsSettings,
    (json)(ubjson)(xml)(csv_record),
    NX_VMS_COMMON_API)
