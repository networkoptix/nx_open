#pragma once

#include <recording/time_period_list.h>
#include <nx/mediaserver/server_module_aware.h>

struct QnChunksRequestData;

class QnChunksRequestHelper: public nx::mediaserver::ServerModuleAware
{
public:
    QnChunksRequestHelper(QnMediaServerModule* serverModule);

    QnTimePeriodList load(const QnChunksRequestData& request) const;

private:
    QnTimePeriodList loadAnalyticsTimePeriods(const QnChunksRequestData& request) const;
};

