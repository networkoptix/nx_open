#pragma once

#include <nx/update/info/detail/data_provider/abstract_async_raw_data_provider.h>
#include <nx/network/http/http_async_client.h>

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
    virtual void getSpecificUpdateData(
        const QString& customization,
        const QString& version) override;

private:
    QString m_baseUrl;
    AbstractAsyncRawDataProviderHandler* m_handler;
    network::http::AsyncClient m_asyncHttpClient;

    void onGetMetaUpdatesInformationDone();
    void onGetSpecificUpdateInformationDone();

    template<typename HandlerFunciton>
    bool hasTransportError(HandlerFunciton handlerFunction);
};

} // namespace data_parser
} // namespace detail
} // namespace info
} // namespace update
} // namespace nx
