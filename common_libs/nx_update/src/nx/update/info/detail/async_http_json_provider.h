#pragma once

#include <nx/update/info/detail/abstract_async_raw_data_provider.h>

namespace nx {
namespace update {
namespace info {
namespace detail {

class AsyncHttpJsonProvider: public AbstractAsyncRawDataProvider
{
public:
    AsyncHttpJsonProvider(const QString& baseUrl);
    virtual void getUpdatesMetaInformation(AbstractAsyncRawDataProviderHandler* handler) override;
    virtual void getSpecific(
        AbstractAsyncRawDataProviderHandler* handler,
        const QString& customization,
        const QString& version) override;

private:
    QString m_baseUrl;
};

} // namespace detail
} // namespace info
} // namespace update
} // namespace nx
