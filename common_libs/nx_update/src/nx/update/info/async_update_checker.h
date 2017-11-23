#pragma once

#include <nx/update/info/info_fwd.h>

namespace nx {
namespace update {
namespace info {

class UpdateInformation;
using UpdateCheckCallback = utils::MoveOnlyFunc<void(ResultCode, const UpdateInformation&)>;

class AsyncUpdateChecker
{
public:
    AsyncUpdateChecker(AbstractAsyncRawDataProviderPtr rawDataProvider);
    void check(const QString& baseVersion, UpdateCheckCallback callback);

private:
    AbstractAsyncRawDataProviderPtr m_rawDataProvider;
};

} // namespace info
} // namespace update
} // namespace nx
