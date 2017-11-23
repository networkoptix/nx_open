#pragma once

#include <nx/utils/move_only_func.h>

namespace nx {
namespace update {
namespace info {

class detail::AbstractAsyncRawDataProvider;
using UpdateCheckHandler = utils::MoveOnlyFunc<void(const UpdateInformation& updateInformation)>;

class AsyncUpdateChecker
{
public:
    AsyncUpdateChecker(AbstractAsyncRawDataProvider* dataProvider);
    void check(const QString& baseVersion, UpdateCheckHandler handler);

private:

};

} // namespace info
} // namespace update
} // namespace nx
