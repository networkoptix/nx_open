#pragma once

#include <recording/time_period_list.h>

struct QnChunksRequestData;

class QnChunksRequestHelper
{
public:
    static QnTimePeriodList load(const QnChunksRequestData& request);

private:
    static QnTimePeriodList loadAnalyticsTimePeriods(const QnChunksRequestData& request);
};

