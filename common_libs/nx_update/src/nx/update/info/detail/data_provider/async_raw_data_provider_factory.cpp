#include "async_raw_data_provider_factory.h"
#include "abstract_async_raw_data_provider.h"
#include "async_http_json_provider.h"

namespace nx {
namespace update {
namespace info {
namespace detail {
namespace data_provider {

namespace {

static AbstractAsyncRawDataProviderPtr createHttpJsonProvider(
    const QString& baseUrl,
    AbstractAsyncRawDataProviderHandler* handler)
{
    return detail::AbstractAsyncRawDataProviderPtr(new AsyncHttpJsonProvider(baseUrl, handler));
}

} // namespace

AsyncRawDataProviderFactory::AsyncRawDataProviderFactory():
    m_defaultFactoryFunction(&createHttpJsonProvider)
{}

AbstractAsyncRawDataProviderPtr AsyncRawDataProviderFactory::create(
    const QString& baseUrl,
    AbstractAsyncRawDataProviderHandler* handler)
{
    if (m_factoryFunction)
        return m_factoryFunction(baseUrl, handler);

    return m_defaultFactoryFunction(baseUrl, handler);
}

void AsyncRawDataProviderFactory::setFactoryFunction(AsyncRawDataProviderFactoryFunction function)
{
    m_factoryFunction = std::move(function);
}

} // namespace data_provider
} // namespace detail
} // namespace info
} // namespace update
} // namespace nx
