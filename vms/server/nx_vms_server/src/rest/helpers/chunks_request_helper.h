#pragma once

#include <recording/time_period_list.h>
#include <nx/vms/server/server_module_aware.h>

struct QnChunksRequestData;

class QnChunksRequestHelper: public /*mixin*/ nx::vms::server::ServerModuleAware
{
public:
    QnChunksRequestHelper(QnMediaServerModule* serverModule);

    QnTimePeriodList load(const QnChunksRequestData& request) const;

private:
    // All intervals in the list this function returns has non-zero duration.
    QnTimePeriodList loadAnalyticsTimePeriods(const QnChunksRequestData& request) const;
};
