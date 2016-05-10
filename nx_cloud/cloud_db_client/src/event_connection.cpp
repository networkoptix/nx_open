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
    m_httpClient(nx_http::AsyncHttpClient::create()),
    m_reconnectTimer(network::RetryPolicy(
        network::RetryPolicy::kInfiniteRetries,
        std::chrono::milliseconds::zero(),
        2,
        std::chrono::minutes(1))),
    m_state(State::init)
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
    m_eventHandlers = std::move(eventHandlers);
    m_connectCompletionHandler = std::move(completionHandler);
    m_state = State::connecting;
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
        connectionAttemptHasFailed(api::httpStatusCodeToResultCode(resCode));
        return;
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

void EventConnection::connectionAttemptHasFailed(api::ResultCode result)
{
    switch (m_state)
    {
        case State::connecting:
        {
            auto handler = std::move(m_connectCompletionHandler);
            return handler(result);
        }

        case State::connected:
            m_state = State::reconnecting;

        case State::reconnecting:
        {
            //retrying 
            const bool nextTryScheduled = m_reconnectTimer.scheduleNextTry(
                [this]
                {
                    using namespace std::placeholders;
                    m_cdbEndPointFetcher->get(
                        std::bind(&EventConnection::cdbEndpointResolved, this, _1, _2));
                });
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
            return;
        }

        default:
            NX_CRITICAL(false, lm("m_state = %1").arg(static_cast<int>(m_state)));
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

    const auto contentType = nx_http::getHeaderValue(
        httpClient->response()->headers, "Content-Type");
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
        NX_LOGX(lm("cdb (%1) responded with Content-Type (%2) "
            "which does not define multipart HTTP content")
            .arg(httpClient->url().toString()).arg(contentType),
            cl_logWARNING);
        return connectionAttemptHasFailed(api::ResultCode::invalidFormat);
    }

    m_state = State::connected;
    m_reconnectTimer.reset();
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

}   //namespace cl
}   //namespace cdb
}   //namespace nx
