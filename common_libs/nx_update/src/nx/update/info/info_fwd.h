#pragma once

#include <memory>
#include <nx/utils/move_only_func.h>

namespace detail {
class AbstractAsyncRawDataProvider;
}

using AbstractAsyncRawDataProviderPtr = std::unique_ptr<detail::AbstractAsyncRawDataProvider>;
using AsyncRawDataProviderFactoryFunction =
    nx::utils::MoveOnlyFunc<AbstractAsyncRawDataProviderPtr(const QString&)>;
