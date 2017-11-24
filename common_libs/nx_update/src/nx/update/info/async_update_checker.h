#pragma once

#include <nx/update/info/detail/fwd.h>

namespace nx {
namespace update {
namespace info {

class UpdateInformation;
using UpdateCheckCallback = utils::MoveOnlyFunc<void(ResultCode, const UpdateInformationRegistry&)>;

class AsyncUpdateChecker
{
public:
    AsyncUpdateChecker();
    void check(const QString& version, const QString& customization, UpdateCheckCallback callback);
    void checkMeta();

private:
    detail::AbstractAsyncRawDataProviderPtr m_rawDataProvider;
};

} // namespace info
} // namespace update
} // namespace nx
