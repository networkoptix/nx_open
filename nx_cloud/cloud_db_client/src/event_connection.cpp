/**********************************************************
* Apr 29, 2016
* akolesnikov
***********************************************************/

#include "event_connection.h"

#include <nx/network/cloud/cdb_endpoint_fetcher.h>
#include <nx/utils/log/log.h>
#include <utils/math/math.h>
#include <utils/media/custom_output_stream.h>

#include "data/types.h"


namespace nx {
namespace cdb {
namespace cl {

EventConnection::EventConnection(
    network::cloud::CloudModuleEndPointFetcher* const endPointFetcher,
    std::string login,
    std::string password)
:
    m_cdbEndPointFetcher(endPointFetcher),
    m_login(std::move(login)),
    m_password(std::move(password)),
    m_httpClient(nx_http::AsyncHttpClient::create())
{
    connect(
        m_httpClient.get(), &nx_http::AsyncHttpClient::responseReceived,
        this, &EventConnection::onHttpResponseReceived,
        Qt::DirectConnection);
    connect(
        m_httpClient.get(), &nx_http::AsyncHttpClient::someMessageBodyAvailable,
        this, &EventConnection::onSomeMessageBodyAvailable,
        Qt::DirectConnection);
    connect(
        m_httpClient.get(), &nx_http::AsyncHttpClient::done,
        this, &EventConnection::onHttpClientDone,
        Qt::DirectConnection);
}

EventConnection::~EventConnection()
{
    //TODO #ak cancel m_cdbEndPointFetcher->get

    m_httpClient->terminate();
    m_httpClient.reset();
}

void EventConnection::start(
    api::SystemEventHandlers eventHandlers,
    std::function<void(api::ResultCode)> completionHandler)
{
    using namespace std::placeholders;
    NX_ASSERT(!m_connectCompletionHandler);
    m_connectCompletionHandler = std::move(completionHandler);
    m_cdbEndPointFetcher->get(
        std::bind(&EventConnection::cdbEndpointResolved, this, _1, _2));
}

static const QString kEventHandlerPath("/cdb/events");

void EventConnection::cdbEndpointResolved(
    nx_http::StatusCode::Value resCode,
    SocketAddress endpoint)
{
    if (resCode != nx_http::StatusCode::ok)
    {
        auto handler = std::move(m_connectCompletionHandler);
        return handler(api::httpStatusCodeToResultCode(resCode));
    }

    //connecting with Http client to cloud_db
    QUrl url;
    url.setScheme("http");
    url.setHost(endpoint.address.toString());
    url.setPort(endpoint.port);
    url.setPath(url.path() + kEventHandlerPath);
    url.setUserName(QString::fromStdString(m_login));
    url.setPassword(QString::fromStdString(m_password));
    m_httpClient->doGet(url);
}

void EventConnection::onHttpResponseReceived(nx_http::AsyncHttpClientPtr httpClient)
{
    if (httpClient->failed())
    {
        NX_LOGX(lm("Error issuing request to %1: %2")
            .arg(httpClient->url().toString())
            .arg(SystemError::toString(httpClient->lastSysErrorCode())),
            cl_logDEBUG1);
        auto handler = std::move(m_connectCompletionHandler);
        return handler(api::ResultCode::networkError);
    }

    const nx_http::StatusCode::Value responseStatusCode = 
        static_cast<nx_http::StatusCode::Value>(
            httpClient->response()->statusLine.statusCode);
    if (!nx_http::StatusCode::isSuccessCode(responseStatusCode))
    {
        NX_LOGX(lm("Received error response from %1: %2")
            .arg(httpClient->url().toString())
            .arg(nx_http::StatusCode::toString(responseStatusCode)),
            cl_logDEBUG1);
        auto handler = std::move(m_connectCompletionHandler);
        return handler(api::httpStatusCodeToResultCode(responseStatusCode));
    }

    const auto contentType = nx_http::getHeaderValue(httpClient->response()->headers, "Content-Type");
    NX_LOGX(lm("Received success response (%1) from %2. Content-Type %3")
        .arg(nx_http::StatusCode::toString(responseStatusCode))
        .arg(httpClient->url().toString())
        .arg(nx_http::getHeaderValue(httpClient->response()->headers, "Content-Type")),
        cl_logDEBUG2);

    m_multipartContentParser = std::make_shared<nx_http::MultipartContentParser>();
    m_multipartContentParser->setNextFilter(
        makeCustomOutputStream(
            std::bind(
                &EventConnection::onReceivingSerializedEvent,
                this,
                std::placeholders::_1)));

    //reading and parsing multipart content
    if (!m_multipartContentParser->setContentType(contentType))
    {
        NX_LOGX(lm("cdb (%1) responded with Content-Type (%2) which does not define multipart HTTP content")
            .arg(httpClient->url().toString()).arg(contentType),
            cl_logWARNING);
        auto handler = std::move(m_connectCompletionHandler);
        return handler(api::ResultCode::invalidFormat);
    }
}

void EventConnection::onSomeMessageBodyAvailable(nx_http::AsyncHttpClientPtr httpClient)
{
    m_multipartContentParser->processData(httpClient->fetchMessageBodyBuffer());
}

void EventConnection::onHttpClientDone(nx_http::AsyncHttpClientPtr httpClient)
{
    NX_LOGX(lm("Http connection to %1 has been closed. Retrying...")
        .arg(httpClient->url().toString()), cl_logDEBUG1);

    //TODO #ak retrying 
}

void EventConnection::onReceivingSerializedEvent(QByteArray serializedEvent)
{
    const auto dataContentType = nx_http::getHeaderValue(
        m_multipartContentParser->prevFrameHeaders(), "Content-Type");

    NX_LOGX(lm("Received event from %1: total %2 bytes. %3")
        .arg(m_httpClient->url().toString())
        .arg(serializedEvent.size()).arg(serializedEvent),
        cl_logDEBUG2);

    if (m_multipartContentParser->isEpilogue())
    {
        NX_LOGX(lm("cdb has closed event stream. Retrying..."), cl_logDEBUG1);
        //TODO #ak retrying 
        return;
    }

    //TODO #ak parsing event
    //TODO #ak reporting event
}

}   //namespace cl
}   //namespace cdb
}   //namespace nx
