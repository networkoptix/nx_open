#include "raw_data_provider_factory.h"
#include "abstract_async_raw_data_provider.h"
#include "impl/async_http_json_provider.h"

namespace nx {
namespace update {
namespace info {
namespace detail {
namespace data_provider {

AsyncRawDataProviderFactoryFunction RawDataProviderFactory::s_factoryFunction = nullptr;

namespace {

static AbstractAsyncRawDataProviderPtr createHttpJsonProvider(
    const QString& baseUrl,
    AbstractAsyncRawDataProviderHandler* handler)
{
    return detail::AbstractAsyncRawDataProviderPtr(new AsyncHttpJsonProvider(baseUrl, handler));
}

} // namespace

AbstractAsyncRawDataProviderPtr RawDataProviderFactory::create(
    const QString& baseUrl,
    AbstractAsyncRawDataProviderHandler* handler)
{
    if (s_factoryFunction)
        return s_factoryFunction(baseUrl, handler);

    return createHttpJsonProvider(baseUrl, handler);
}

void RawDataProviderFactory::setFactoryFunction(AsyncRawDataProviderFactoryFunction function)
{
    s_factoryFunction = std::move(function);
}

} // namespace data_provider
} // namespace detail
} // namespace info
} // namespace update
} // namespace nx
