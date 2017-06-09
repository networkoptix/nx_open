#pragma once

#include <utils/db/query_context.h>
#include <utils/db/types.h>
#include <nx/utils/uuid.h>
#include "persistent_scheduler_common.h"

namespace nx {
namespace cdb {

class AbstractSchedulerDbHelper
{
public:
    virtual nx::db::DBResult registerEventReceiver(
        nx::db::QueryContext*,
        const QnUuid& fucntorId) = 0;

    virtual nx::db::DBResult getScheduleData(
        nx::db::QueryContext* queryContext,
        ScheduleData* scheduleData) const = 0;

    virtual nx::db::DBResult subscribe(
        nx::db::QueryContext* queryContext,
        const QnUuid& functorId,
        QnUuid* outTaskId,
        std::chrono::steady_clock::time_point timepoint,
        const ScheduleParams& params) = 0;

};

}
}
