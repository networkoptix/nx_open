#pragma once

#include <utils/db/types.h>
#include <nx/utils/uuid.h>

namespace nx {
namespace cdb {

class AbstractSchedulerDbHelper
{
public:
    virtual nx::db::DBResult registerEventReceiver(const QnUuid& receiverId) = 0;
};

}
}
