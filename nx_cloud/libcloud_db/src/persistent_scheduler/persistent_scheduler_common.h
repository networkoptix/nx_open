#pragma once

#include <unordered_map>
#include <string>
#include <vector>
#include <nx/utils/uuid.h>

namespace nx {
namespace cdb {

using ScheduleParams = std::unordered_map<std::string, std::string>;
using TaskToParams = std::unordered_map<QnUuid, ScheduleParams>;
using FunctorToTasks = std::unordered_map<QnUuid, QnUuid>;

struct ScheduleData
{
    FunctorToTasks functorToTasks;
    TaskToParams taskToParams;
};

}
}