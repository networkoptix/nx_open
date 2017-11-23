#pragma once

namespace nx {
namespace update {
namespace info {
namespace detail {

class AbstractAsyncRawDataProviderHandler;

class AbstractAsyncRawDataProvider
{
public:
    virtual ~AbstractAsyncRawDataProvider() {}
    virtual void getUpdatesMetaInformation(AbstractAsyncRawDataProviderHandler* handler) = 0;
    virtual void getSpecific(
        AbstractAsyncRawDataProviderHandler* handler,
        const QString& customization,
        const QString& version) = 0;
};

} // namespace detail
} // namespace info
} // namespace update
} // namespace nx
