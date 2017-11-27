#pragma once

#include <memory>
#include <nx/update/info/detail/fwd.h>

namespace nx {
namespace update {
namespace info {

class AbstractUpdateRegistry;
using AbstractUpdateRegistryPtr = std::unique_ptr<AbstractUpdateRegistry>;

using UpdateCheckCallback = utils::MoveOnlyFunc<void(ResultCode, AbstractUpdateRegistryPtr)>;

class AsyncUpdateChecker
{
public:
    AsyncUpdateChecker();
    void check(const QString& url, UpdateCheckCallback callback);

private:
    detail::AbstractAsyncRawDataProviderPtr m_rawDataProvider;
};

} // namespace info
} // namespace update
} // namespace nx
