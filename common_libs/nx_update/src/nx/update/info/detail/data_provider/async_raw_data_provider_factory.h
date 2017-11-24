#pragma once

#include <nx/update/info/detail/fwd.h>

namespace nx {
namespace update {
namespace info {
namespace detail {
namespace data_provider {

class AsyncRawDataProviderFactory
{
public:
    AsyncRawDataProviderFactory();
    AbstractAsyncRawDataProviderPtr create(
        const QString& baseUrl,
        AbstractAsyncRawDataProviderHandler* handler);
    void setFactoryFunction(AsyncRawDataProviderFactoryFunction function);

private:
    AsyncRawDataProviderFactoryFunction m_defaultFactoryFunction;
    AsyncRawDataProviderFactoryFunction m_factoryFunction;
};

} // namespace data_provider
} // namespace detail
} // namespace info
} // namespace update
} // namespace nx
