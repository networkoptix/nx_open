#pragma once

#include <nx/utils/move_only_func.h>

namespace nx {
namespace update {
namespace info {

class AbstractJsonProvider;
using UpdateCheckHandler = utils::MoveOnlyFunc<void(const UpdateInformation& updateInformation)>;

class AsyncUpdateChecker
{
public:
    AsyncUpdateChecker(AbstractJsonProvider* jsonProvider);
    void check(const QString& baseVersion, UpdateCheckHandler handler);
};

} // namespace info
} // namespace update
} // namespace nx
