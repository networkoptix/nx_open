#pragma once

#include <memory>
#include <nx/update/info/detail/fwd.h>
#include <nx/update/info/detail/data_provider/abstract_async_raw_data_provider_handler.h>
#include <nx/update/info/abstract_update_registry.h>

namespace nx {
namespace update {
namespace info {

using AbstractUpdateRegistryPtr = std::unique_ptr<AbstractUpdateRegistry>;
using UpdateCheckCallback = utils::MoveOnlyFunc<void(ResultCode, AbstractUpdateRegistryPtr)>;
namespace detail { class AsyncUpdateCheckerImpl; }

class NX_UPDATE_API AsyncUpdateChecker
{
public:
    AsyncUpdateChecker();
    ~AsyncUpdateChecker();
    void check(const QString& baseUrl, UpdateCheckCallback callback);

private:
    std::unique_ptr<detail::AsyncUpdateCheckerImpl> m_impl;
};

} // namespace info
} // namespace update
} // namespace nx
