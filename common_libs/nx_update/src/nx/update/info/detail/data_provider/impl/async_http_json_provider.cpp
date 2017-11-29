#include <functional>
#include "async_http_json_provider.h"
#include <nx/network/url/url_builder.h>
#include <nx/utils/log/log.h>
#include "../abstract_async_raw_data_provider_handler.h"

namespace nx {
namespace update {
namespace info {
namespace detail {
namespace data_provider {

namespace {

static const QString kUpdatesUrlPostfix = "/updates.json";
static const QString kUpdateUrlPostfix = "/update.json";

} // namespace

AsyncHttpJsonProvider::AsyncHttpJsonProvider(
    const QString& baseUrl,
    AbstractAsyncRawDataProviderHandler *handler)
    :
    m_baseUrl(baseUrl),
    m_handler(handler)
{
}

void AsyncHttpJsonProvider::getUpdatesMetaInformation()
{
    nx::network::url::Builder urlBuilder(m_baseUrl);
    urlBuilder.appendPath(kUpdatesUrlPostfix);

    m_asyncHttpClient.doGet(
        urlBuilder.toUrl(),
        std::bind(
            &AsyncHttpJsonProvider::onGetMetaUpdatesInformationDone,
            this));
}

void AsyncHttpJsonProvider::getSpecificUpdateData(const QString& customization, const QString& version)
{
    nx::network::url::Builder urlBuilder(m_baseUrl);
    urlBuilder.appendPath("/" + customization);
    urlBuilder.appendPath("/" + version);
    urlBuilder.appendPath(kUpdateUrlPostfix);

    m_asyncHttpClient.doGet(
        urlBuilder.toUrl(),
        std::bind(
            &AsyncHttpJsonProvider::onGetSpecificUpdateInformationDone,
            this));

}

void AsyncHttpJsonProvider::onGetMetaUpdatesInformationDone()
{
    using namespace std::placeholders;

    if (hasTransportError(
            std::bind(
                &AbstractAsyncRawDataProviderHandler::onGetUpdatesMetaInformationDone,
                m_handler, _1, _2)))
    {
        return;
    }

    NX_VERBOSE(this, "Succesfully got http response");
    m_handler->onGetUpdatesMetaInformationDone(ResultCode::ok, m_asyncHttpClient.fetchMessageBodyBuffer());
}

template<typename HandlerFunction>
bool AsyncHttpJsonProvider::hasTransportError(HandlerFunction handlerFunction)
{
    if (m_asyncHttpClient.state() != nx_http::AsyncClient::State::sDone)
    {
        NX_ERROR(
            this,
            lm("Http client transport error: %1").args(m_asyncHttpClient.lastSysErrorCode()));
        handlerFunction(ResultCode::getRawDataError, QByteArray());
        return true;
    }

    int statusCode = m_asyncHttpClient.response()->statusLine.statusCode;
    if (statusCode != nx_http::StatusCode::ok)
    {
        NX_ERROR(this, lm("Http client http error: ").args(statusCode));
        handlerFunction(ResultCode::getRawDataError, QByteArray());
        return true;
    }

    return false;
}

void AsyncHttpJsonProvider::onGetSpecificUpdateInformationDone()
{
    using namespace std::placeholders;

    if (hasTransportError(
            std::bind(
                &AbstractAsyncRawDataProviderHandler::onGetSpecificUpdateInformationDone,
                m_handler, _1, _2)))
    {
        return;
    }

    NX_VERBOSE(this, "Succesfully got http response");
    m_handler->onGetSpecificUpdateInformationDone(ResultCode::ok, m_asyncHttpClient.fetchMessageBodyBuffer());
}

} // namespace data_provider
} // namespace detail
} // namespace info
} // namespace update
} // namespace nx
