#pragma once

#include <nx/update/info/info_fwd.h>

namespace nx {
namespace update {
namespace info {

class AsyncRawDataProviderFactory
{
public:
    AsyncRawDataProviderFactory();
    AbstractAsyncRawDataProviderPtr create(const QString& baseUrl);
    void setFactoryFunction(AsyncRawDataProviderFactoryFunction function);

private:
    AsyncRawDataProviderFactoryFunction m_defaultFactoryFunction;
    AsyncRawDataProviderFactoryFunction m_factoryFunction;
};

} // namespace info
} // namespace update
} // namespace nx
