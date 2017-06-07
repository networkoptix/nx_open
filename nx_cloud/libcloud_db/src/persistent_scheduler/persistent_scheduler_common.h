#pragma once

#include <string>
#include <vector>
#include <nx/utils/uuid.h>

namespace nx {
namespace cdb {

struct ScheduleData
{
    QnUuid functorId;
    QnUuid taskId;
    std::string key;
    std::string value;
};

using ScheduleDataVector = std::vector<ScheduleData>;

}
}