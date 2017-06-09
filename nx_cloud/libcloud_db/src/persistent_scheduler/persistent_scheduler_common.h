#pragma once

#include <unordered_map>
#include <string>
#include <vector>
#include <chrono>
#include <nx/utils/uuid.h>

namespace nx {
namespace cdb {

using ScheduleParams = std::unordered_map<std::string, std::string>;
struct ScheduleTaskInfo
{
    ScheduleParams params;
    std::chrono::steady_clock::time_point fireTimePoint;
    std::chrono::milliseconds period;
};

using TaskToParams = std::unordered_map<QnUuid, ScheduleTaskInfo>;
using FunctorToTasks = std::unordered_map<QnUuid, QnUuid>;

struct ScheduleData
{
    FunctorToTasks functorToTasks;
    TaskToParams taskToParams;
};

}
}