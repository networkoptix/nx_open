#pragma once

#include <chrono>
#include <set>
#include <string>
#include <unordered_map>

#include <nx/utils/uuid.h>

namespace nx {
namespace cdb {

using ScheduleParams = std::unordered_map<std::string, std::string>;
struct ScheduleTaskInfo
{
    ScheduleParams params;
    std::chrono::milliseconds period;
    std::chrono::steady_clock::time_point schedulePoint;
};

using TaskToParams = std::unordered_map<QnUuid, ScheduleTaskInfo>;
using FunctorToTasks = std::unordered_map<QnUuid, std::set<QnUuid>>;

struct ScheduleData
{
    FunctorToTasks functorToTasks;
    TaskToParams taskToParams;
};

} // namespace cdb
} // namespace nx
