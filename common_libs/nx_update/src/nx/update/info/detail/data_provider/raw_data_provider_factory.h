#pragma once

#include <nx/update/info/detail/fwd.h>

namespace nx {
namespace update {
namespace info {
namespace detail {
namespace data_provider {

class RawDataProviderFactory
{
public:
    static AbstractAsyncRawDataProviderPtr create(
        const QString& baseUrl,
        AbstractAsyncRawDataProviderHandler* handler);
    static void setFactoryFunction(AsyncRawDataProviderFactoryFunction function);

private:
    static AsyncRawDataProviderFactoryFunction m_factoryFunction;
};

} // namespace data_provider
} // namespace detail
} // namespace info
} // namespace update
} // namespace nx
