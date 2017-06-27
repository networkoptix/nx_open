#pragma once

#include <statistics/statistics_fwd.h>
#include <nx/fusion/model_functions_fwd.h>

struct QnStatisticsSettings
{
    int limit;
    int storeDays;
    int minSendPeriodSecs;
    QnStringsSet filters;
    QString statisticsServerUrl;

    QnStatisticsSettings();
};

#define QnStatisticsSettings_Fields (limit)(storeDays)(minSendPeriodSecs)(filters)(statisticsServerUrl)
QN_FUSION_DECLARE_FUNCTIONS(QnStatisticsSettings, (json)(ubjson)(xml)(csv_record)(eq)(metatype))