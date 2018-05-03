#pragma once

#include <memory>
#include <nx/utils/move_only_func.h>
#include <nx/update/info/result_code.h>
#include <nx/update/info/detail/data_provider/abstract_async_raw_data_provider.h>
#include <nx/update/info/detail/data_parser/abstract_raw_data_parser.h>

namespace nx {
namespace update {
namespace info {
namespace detail {

namespace data_provider {
class AbstractAsyncRawDataProviderHandler;
}

using AbstractAsyncRawDataProviderPtr = std::unique_ptr<data_provider::AbstractAsyncRawDataProvider>;
using AsyncRawDataProviderFactoryFunction =
    nx::utils::MoveOnlyFunc<AbstractAsyncRawDataProviderPtr(
        const QString&,
        data_provider::AbstractAsyncRawDataProviderHandler*)>;

using AbstractRawDataParserPtr = std::unique_ptr<data_parser::AbstractRawDataParser>;
using RawDataParserFactoryFunction = nx::utils::MoveOnlyFunc<AbstractRawDataParserPtr()>;

} // namespace detail
} // namespace info
} // namespace update
} // namespace nx
