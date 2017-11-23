#include "async_raw_data_provider_factory.h"
#include "detail/abstract_async_raw_data_provider.h"
#include "detail/async_http_json_provider.h"

namespace nx {
namespace update {
namespace info {

namespace {

static AbstractAsyncRawDataProviderPtr createHttpJsonProvider(const QString& baseUrl)
{
    return AbstractAsyncRawDataProviderPtr(new detail::AsyncHttpJsonProvider(baseUrl));
}

} // namespace

AsyncRawDataProviderFactory::AsyncRawDataProviderFactory():
    m_defaultFactoryFunction(&createHttpJsonProvider)
{}

AbstractAsyncRawDataProviderPtr AsyncRawDataProviderFactory::create(const QString& baseUrl)
{
    if (m_factoryFunction)
        return m_factoryFunction(baseUrl);

    return m_defaultFactoryFunction(baseUrl);
}

void AsyncRawDataProviderFactory::setFactoryFunction(AsyncRawDataProviderFactoryFunction function)
{
    m_factoryFunction = std::move(function);
}

} // namespace info
} // namespace update
} // namespace nx
