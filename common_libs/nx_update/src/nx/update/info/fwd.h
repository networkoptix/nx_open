#pragma once

#include <memory>
#include <nx/utils/move_only_func.h>
#include <nx/update/info/result_code.h>
#include <nx/update/info/detail/abstract_async_raw_data_provider.h>

namespace nx {
namespace update {
namespace info {

using AbstractAsyncRawDataProviderPtr = std::unique_ptr<detail::AbstractAsyncRawDataProvider>;
using AsyncRawDataProviderFactoryFunction =
    nx::utils::MoveOnlyFunc<AbstractAsyncRawDataProviderPtr(const QString&)>;

} // namespace info
} // namespace update
} // namespace nx
