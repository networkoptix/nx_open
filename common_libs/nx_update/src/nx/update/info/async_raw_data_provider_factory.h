#pragma once

#include <memory>
#include <functional>

namespace nx {
namespace update {
namespace info {

namespace detail {
class AbstractAsyncRawDataProvider;
}

using AbstractAsyncRawDataProviderPtr = std::unique_ptr<detail::AbstractAsyncRawDataProvider>;
using AsyncRawDataProviderFactoryFunction = std::function<AbstractAsyncRawDataProviderPtr(const QString&)>;

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
