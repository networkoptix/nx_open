/**********************************************************
* Apr 29, 2016
* akolesnikov
***********************************************************/

#include "event_connection.h"

#include <nx/fusion/serialization/lexical.h>
#include <nx/network/cloud/cloud_module_url_fetcher.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>

#include <nx/network/http/custom_headers.h>
#include <nx/utils/byte_stream/custom_output_stream.h>

#include "cdb_request_path.h"
#include "data/types.h"


namespace nx {
namespace cdb {
namespace client {

EventConnection::EventConnection(
    network::cloud::CloudModuleUrlFetcher* const endPointFetcher)
    :
    m_cdbEndPointFetcher(
        std::make_unique<network::cloud::CloudModuleUrlFetcher::ScopedOperation>(
            endPointFetcher)),
    m_reconnectTimer(network::RetryPolicy(
        network::RetryPolicy::kInfiniteRetries,
        std::chrono::milliseconds::zero(),
        2,
        std::chrono::minutes(1))),
    m_state(State::init)
{
}

void EventConnection::setCredentials(const std::string& login, const std::string& password)
{
    m_auth.username = QString::fromStdString(login);
    m_auth.password = QString::fromStdString(password);
}

void EventConnection::setProxyCredentials(const std::string& login, const std::string& password)
{
    m_auth.proxyUsername = QString::fromStdString(login);
    m_auth.proxyPassword = QString::fromStdString(password);
}

void EventConnection::setProxyVia(
    const std::string& proxyHost,
    std::uint16_t proxyPort)
{
    m_auth.proxyEndpoint = SocketAddress(proxyHost.c_str(), proxyPort);
}

EventConnection::~EventConnection()
{
    //TODO #ak cancel m_cdbEndPointFetcher->get
    m_cdbEndPointFetcher.reset();

    if (m_httpClient)
    {
        m_httpClient->pleaseStopSync();
        m_httpClient.reset();
    }
    m_reconnectTimer.pleaseStopSync();
}

void EventConnection::start(
    api::SystemEventHandlers eventHandlers,
    std::function<void(api::ResultCode)> completionHandler)
{
    using namespace std::placeholders;
    NX_ASSERT(!m_connectCompletionHandler);
    m_eventHandlers = std::move(eventHandlers);
    m_connectCompletionHandler = std::move(completionHandler);
    m_state = State::connecting;
    m_cdbEndPointFetcher->get(
        std::bind(&EventConnection::cdbEndpointResolved, this, _1, _2));
}

void EventConnection::cdbEndpointResolved(
    nx_http::StatusCode::Value resCode,
    QUrl url)
{
    if (resCode != nx_http::StatusCode::ok)
    {
        connectionAttemptHasFailed(api::httpStatusCodeToResultCode(resCode));
        return;
    }

    m_cdbUrl = std::move(url);
    initiateConnection();
}

void EventConnection::initiateConnection()
{
    if (!m_httpClient)
    {
        m_httpClient = nx_http::AsyncHttpClient::create();
        m_httpClient->bindToAioThread(m_reconnectTimer.getAioThread());

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

    //connecting with Http client to cloud_db
    QUrl url = m_cdbUrl;
    url.setPath(network::url::normalizePath(m_cdbUrl.path() + kSubscribeToSystemEventsPath));
    m_httpClient->setAuth(m_auth);
    m_httpClient->doGet(url);
}

void EventConnection::connectionAttemptHasFailed(api::ResultCode result)
{
    switch (m_state)
    {
        case State::connecting:
        {
            if (m_httpClient)
            {
                //we are in m_httpClient's aio thread
                m_httpClient->pleaseStopSync();
                m_httpClient.reset();
            }
            auto handler = std::move(m_connectCompletionHandler);
            return handler(result);
        }

        case State::connected:
            m_state = State::reconnecting;
            retryToConnect();
            break;

        case State::reconnecting:
            retryToConnect();
            break;

        default:
            NX_CRITICAL(false, lm("m_state = %1").arg(static_cast<int>(m_state)));
    }
}

void EventConnection::retryToConnect()
{
    const bool nextTryScheduled = m_reconnectTimer.scheduleNextTry(
        std::bind(&EventConnection::initiateConnection, this));
    if (!nextTryScheduled)
    {
        //reporting error event
        m_state = State::failed;
        if (m_eventHandlers.onConnectionLost)
        {
            //TODO #ak fill event data
            m_eventHandlers.onConnectionLost(api::ConnectionLostEvent());
        }
    }
}

void EventConnection::onHttpResponseReceived(nx_http::AsyncHttpClientPtr httpClient)
{
    const nx_http::StatusCode::Value responseStatusCode =
        static_cast<nx_http::StatusCode::Value>(
            httpClient->response()->statusLine.statusCode);
    if (!nx_http::StatusCode::isSuccessCode(responseStatusCode))
    {
        NX_LOGX(lm("Received error response from %1: %2")
            .arg(httpClient->url().toString())
            .arg(nx_http::StatusCode::toString(responseStatusCode)),
            cl_logDEBUG1);
        return connectionAttemptHasFailed(
            api::httpStatusCodeToResultCode(responseStatusCode));
    }

    //checking cdb result code
    auto cdbResultCodeIter =
        httpClient->response()->headers.find(Qn::API_RESULT_CODE_HEADER_NAME);
    if (cdbResultCodeIter == httpClient->response()->headers.end())
    {
        NX_LOGX(lm("Error. Received response without %1 header")
            .arg(Qn::API_RESULT_CODE_HEADER_NAME), cl_logDEBUG1);
        return connectionAttemptHasFailed(
            api::ResultCode::invalidFormat);
    }
    const auto cdbResultCode =
        QnLexical::deserialized<api::ResultCode>(cdbResultCodeIter->second);
    if (cdbResultCode != api::ResultCode::ok)
    {
        NX_LOGX(lm("Error. Received result code %1")
            .arg(cdbResultCodeIter->second), cl_logDEBUG1);
        return connectionAttemptHasFailed(cdbResultCode);
    }

    const auto contentType = nx_http::getHeaderValue(
        httpClient->response()->headers, "Content-Type");
    NX_LOGX(lm("Received success response (%1) from %2. Content-Type %3")
        .arg(nx_http::StatusCode::toString(responseStatusCode))
        .arg(httpClient->url().toString())
        .arg(nx_http::getHeaderValue(httpClient->response()->headers, "Content-Type")),
        cl_logDEBUG2);

    m_multipartContentParser = std::make_shared<nx_http::MultipartContentParser>();
    m_multipartContentParser->setNextFilter(
        nx::utils::bstream::makeCustomOutputStream(
            std::bind(
                &EventConnection::onReceivingSerializedEvent,
                this,
                std::placeholders::_1)));

    //reading and parsing multipart content
    if (!m_multipartContentParser->setContentType(contentType))
    {
        NX_LOGX(lm("cdb (%1) responded with Content-Type (%2) "
            "which does not define multipart HTTP content")
            .arg(httpClient->url().toString()).arg(contentType),
            cl_logWARNING);
        return connectionAttemptHasFailed(api::ResultCode::invalidFormat);
    }

    auto oldState = m_state;
    m_state = State::connected;
    m_reconnectTimer.reset();
    if (oldState == State::reconnecting)
        return;
    NX_ASSERT(oldState == State::connecting && m_connectCompletionHandler);
    auto handler = std::move(m_connectCompletionHandler);
    return handler(api::ResultCode::ok);
}

void EventConnection::onSomeMessageBodyAvailable(nx_http::AsyncHttpClientPtr httpClient)
{
    m_multipartContentParser->processData(httpClient->fetchMessageBodyBuffer());
}

void EventConnection::onHttpClientDone(nx_http::AsyncHttpClientPtr httpClient)
{
    if (httpClient->failed())
    {
        NX_LOGX(lm("Error issuing request to %1: %2")
            .arg(httpClient->url().toString())
            .arg(SystemError::toString(httpClient->lastSysErrorCode())),
            cl_logDEBUG1);
    }

    NX_LOGX(lm("Http connection to %1 has been closed/failed. Retrying...")
        .arg(httpClient->url().toString()), cl_logDEBUG1);

    return connectionAttemptHasFailed(api::ResultCode::networkError);
}

void EventConnection::onReceivingSerializedEvent(QByteArray serializedEvent)
{
    const auto dataContentType = nx_http::getHeaderValue(
        m_multipartContentParser->prevFrameHeaders(), "Content-Type");

    if (m_multipartContentParser->eof())
    {
        NX_LOGX(lm("cdb has closed event stream. Retrying..."), cl_logDEBUG1);
        m_httpClient->forceEndOfMsgBody();
        connectionAttemptHasFailed(api::ResultCode::networkError);  //TODO #ak better result code needed
        return;
    }

    NX_LOGX(lm("Received event from %1: total %2 bytes. %3")
        .arg(m_httpClient->url().toString())
        .arg(serializedEvent.size()).arg(serializedEvent),
        cl_logDEBUG2);

    //TODO #ak parsing event
    //TODO #ak reporting event
}

}   //namespace client
}   //namespace cdb
}   //namespace nx
