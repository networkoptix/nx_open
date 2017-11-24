#pragma once

#include "abstract_async_raw_data_provider.h"

namespace nx {
namespace update {
namespace info {
namespace detail {
namespace data_provider {

class AbstractAsyncRawDataProviderHandler;

class AsyncHttpJsonProvider: public AbstractAsyncRawDataProvider
{
public:
    AsyncHttpJsonProvider(const QString& baseUrl, AbstractAsyncRawDataProviderHandler* handler);
    virtual void getUpdatesMetaInformation() override;
    virtual void getSpecific(const QString& customization, const QString& version) override;

private:
    QString m_baseUrl;
    AbstractAsyncRawDataProviderHandler* m_handler;
};

} // namespace data_parser
} // namespace detail
} // namespace info
} // namespace update
} // namespace nx
