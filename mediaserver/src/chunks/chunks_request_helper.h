#pragma once

#include <recording/time_period_list.h>
#include <api/helpers/chunks_request_data.h>

class QnChunksRequestHelper
{
public:
    static QnTimePeriodList load(const QnChunksRequestData& request);
};

