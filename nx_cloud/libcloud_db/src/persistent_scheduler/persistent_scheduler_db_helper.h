#pragma once

#include <utils/db/types.h>
#include <nx/utils/uuid.h>
#include "persistent_scheduler_common.h"

namespace nx {
namespace cdb {

class AbstractSchedulerDbHelper
{
public:
    virtual nx::db::DBResult registerEventReceiver(const QnUuid& receiverId) = 0;
    virtual ScheduleDataVector getScheduleData() const = 0;
};

}
}
