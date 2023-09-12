// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "http_async_client.h"

#include <nx/network/buffered_stream_socket.h>
#include <nx/network/nx_network_ini.h>
#include <nx/network/socket_factory.h>
#include <nx/network/socket_global.h>
#include <nx/network/url/url_builder.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/utils/crypt/linux_passwd_crypt.h>
#include <nx/utils/log/log.h>
#include <nx/utils/system_error.h>
#include <nx/utils/thread/mutex.h>

#include "auth_cache.h"
#include "auth_tools.h"
#include "buffer_source.h"
#include "custom_headers.h"
#include "http_client_message_body_source.h"
#include "nonce_cache.h"

namespace {

static bool forceTrafficLogging = false;

static bool logTraffic()
{
    return nx::network::ini().httpClientTraffic || forceTrafficLogging;
}

} // namespace

namespace nx::network::http {

static AsyncClient::Timeouts kDefaultTimeouts{
    /*sendTimeout*/ std::chrono::milliseconds(3001),
    /*responseReadTimeout*/ std::chrono::milliseconds(3002),
    /*messageBodyReadTimeout*/ std::chrono::milliseconds(10003),
};

const AsyncClient::Timeouts& AsyncClient::Timeouts::defaults()
{
    return kDefaultTimeouts;
}

void AsyncClient::Timeouts::setDefaults(Timeouts value)
{
    kDefaultTimeouts = value;
}

AsyncClient::AsyncClient(ssl::AdapterFunc adapterFunc): m_adapterFunc(std::move(adapterFunc))
{
    NX_VERBOSE(this, "Creating an instance...");
    SocketGlobals::instance().allocationAnalyzer().recordObjectCreation(this);
    ++SocketGlobals::instance().debugCounters().httpClientConnectionCount;
    if (!NX_ASSERT(m_adapterFunc))
        m_adapterFunc = ssl::kDefaultCertificateCheck;
}

AsyncClient::AsyncClient(
    std::unique_ptr<AbstractStreamSocket> socket, ssl::AdapterFunc adapterFunc):
    AsyncClient(std::move(adapterFunc))
{
    NX_VERBOSE(this, "Creating an instance...");
    m_socket = std::move(socket);
    if (m_socket)
    {
        bindToAioThread(m_socket->getAioThread());
        initializeMessagePipeline();
    }
}

AsyncClient::~AsyncClient()
{
    NX_VERBOSE(this, "Deleting the instance...");
    --SocketGlobals::instance().debugCounters().httpClientConnectionCount;
    SocketGlobals::instance().allocationAnalyzer().recordObjectDestruction(this);
}

const std::unique_ptr<AbstractStreamSocket>& AsyncClient::socket()
{
    return m_messagePipeline
        ? m_messagePipeline->socket()
        : m_socket;
}

std::unique_ptr<AbstractStreamSocket> AsyncClient::takeSocket()
{
    NX_ASSERT(isInSelfAioThread());

    if (m_socket)
    {
        m_socket->cancelIOSync(nx::network::aio::etNone);
        NX_VERBOSE(this, "Giving away socket %1", m_socket.get());
        SocketGlobals::instance().allocationAnalyzer().recordObjectMove(m_socket.get());
        return std::exchange(m_socket, nullptr);
    }

    if (m_messagePipeline)
    {
        auto socket = m_messagePipeline->takeSocket();
        SocketGlobals::instance().allocationAnalyzer().recordObjectMove(socket.get());
        m_messagePipeline.reset();
        NX_VERBOSE(this, "Giving away socket %1", socket.get());
        return socket;
    }

    return nullptr;
}

void AsyncClient::bindToAioThread(nx::network::aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    if (m_socket)
        m_socket->bindToAioThread(aioThread);
    if (m_messagePipeline)
        m_messagePipeline->bindToAioThread(aioThread);
    if (m_requestBody)
        m_requestBody->bindToAioThread(aioThread);
}

bool AsyncClient::failed() const
{
    return m_state == State::sFailed || response() == nullptr;
}

void AsyncClient::setKeepAlive(const KeepAliveOptions& keepAliveOptions)
{
    if (m_socket)
    {
        if (!m_socket->setKeepAlive(keepAliveOptions))
        {
            NX_DEBUG(this, "URL %1. Error enabling TCP keep-alive: %2",
                m_contentLocationUrl, SystemError::getLastOSErrorText());
        }
    }

    m_keepAliveOptions = keepAliveOptions;
}

SystemError::ErrorCode AsyncClient::lastSysErrorCode() const
{
    if (m_lastSysErrorCode != SystemError::noError)
        return m_lastSysErrorCode;
    // Ensuring system error code is always non-zero in case of failure
    //  to simplify AsyncClient user's life.
    return failed() ? SystemError::connectionReset : SystemError::noError;
}

void AsyncClient::setOnRequestHasBeenSent(
    nx::utils::MoveOnlyFunc<void(bool /*isRetryAfterUnauthorizedResponse*/)> handler)
{
    m_onRequestHasBeenSent = std::move(handler);
}

void AsyncClient::setOnResponseReceived(nx::utils::MoveOnlyFunc<void()> handler)
{
    m_onResponseReceived = std::move(handler);
}

void AsyncClient::setOnSomeMessageBodyAvailable(nx::utils::MoveOnlyFunc<void()> handler)
{
    m_onSomeMessageBodyAvailable = std::move(handler);
}

void AsyncClient::setOnDone(nx::utils::MoveOnlyFunc<void()> handler)
{
    m_onDone = std::move(handler);
}

void AsyncClient::setRequestBody(std::unique_ptr<AbstractMsgBodySource> body)
{
    m_requestBody = std::move(body);
    m_requestBody->bindToAioThread(getAioThread());
}

void AsyncClient::doGet(const nx::utils::Url& url)
{
    doRequest(nx::network::http::Method::get, url);
}

void AsyncClient::doGet(
    const nx::utils::Url& url,
    nx::utils::MoveOnlyFunc<void()> completionHandler)
{
    m_onDone = std::move(completionHandler);
    doGet(url);
}

void AsyncClient::doHead(const nx::utils::Url& url)
{
    doRequest(nx::network::http::Method::head, url);
}

void AsyncClient::doHead(
    const nx::utils::Url& url,
    nx::utils::MoveOnlyFunc<void()> completionHandler)
{
    m_onDone = std::move(completionHandler);
    doHead(url);
}

void AsyncClient::doPost(const nx::utils::Url& url)
{
    doRequest(nx::network::http::Method::post, url);
}

void AsyncClient::doPost(
    const nx::utils::Url& url,
    std::unique_ptr<AbstractMsgBodySource> body)
{
    setRequestBody(std::move(body));
    doPost(url);
}

void AsyncClient::doPost(
    const nx::utils::Url& url,
    std::unique_ptr<AbstractMsgBodySource> body,
    nx::utils::MoveOnlyFunc<void()> completionHandler)
{
    m_onDone = std::move(completionHandler);
    doPost(url, std::move(body));
}

void AsyncClient::doPost(
    const nx::utils::Url& url,
    nx::utils::MoveOnlyFunc<void()> completionHandler)
{
    m_onDone = std::move(completionHandler);
    doPost(url);
}

void AsyncClient::doPut(const nx::utils::Url& url)
{
    doRequest(nx::network::http::Method::put, url);
}

void AsyncClient::doPut(
    const nx::utils::Url& url,
    std::unique_ptr<AbstractMsgBodySource> body)
{
    setRequestBody(std::move(body));
    doPut(url);
}

void AsyncClient::doPut(
    const nx::utils::Url& url,
    std::unique_ptr<AbstractMsgBodySource> body,
    nx::utils::MoveOnlyFunc<void()> completionHandler)
{
    m_onDone = std::move(completionHandler);
    doPut(url, std::move(body));
}

void AsyncClient::doPut(
    const nx::utils::Url& url,
    nx::utils::MoveOnlyFunc<void()> completionHandler)
{
    m_onDone = std::move(completionHandler);
    doPut(url);
}

void AsyncClient::doPatch(const nx::utils::Url& url)
{
    doRequest(nx::network::http::Method::patch, url);
}

void AsyncClient::doPatch(
    const nx::utils::Url& url,
    std::unique_ptr<AbstractMsgBodySource> body)
{
    setRequestBody(std::move(body));
    doPatch(url);
}

void AsyncClient::doPatch(
    const nx::utils::Url& url,
    std::unique_ptr<AbstractMsgBodySource> body,
    nx::utils::MoveOnlyFunc<void()> completionHandler)
{
    m_onDone = std::move(completionHandler);
    doPatch(url, std::move(body));
}

void AsyncClient::doPatch(
    const nx::utils::Url& url,
    nx::utils::MoveOnlyFunc<void()> completionHandler)
{
    m_onDone = std::move(completionHandler);
    doPatch(url);
}

void AsyncClient::doDelete(const nx::utils::Url& url)
{
    doRequest(nx::network::http::Method::delete_, url);
}

void AsyncClient::doDelete(
    const nx::utils::Url& url,
    nx::utils::MoveOnlyFunc<void()> completionHandler)
{
    m_onDone = std::move(completionHandler);
    doDelete(url);
}

void AsyncClient::doUpgrade(
    const nx::utils::Url& url,
    const std::string& protocolToUpgradeTo)
{
    doUpgrade(url, Method::options, protocolToUpgradeTo);
}

void AsyncClient::doUpgrade(
    const nx::utils::Url& url,
    const Method& method,
    const std::string& protocolToUpgradeTo)
{
    NX_ASSERT(url.isValid());

    resetDataBeforeNewRequest();
    m_requestUrl = url;
    m_contentLocationUrl = url;
    if (m_additionalHeaders.count("Connection") == 0)
        m_additionalHeaders.emplace("Connection", "Upgrade");
    if (m_additionalHeaders.count("Upgrade") == 0)
        m_additionalHeaders.emplace("Upgrade", protocolToUpgradeTo);
    m_additionalHeaders.emplace("Content-Length", "0");
    composeRequest(method);
    initiateHttpMessageDelivery();
}

void AsyncClient::doUpgrade(
    const nx::utils::Url& url,
    const std::string& protocolToUpgradeTo,
    nx::utils::MoveOnlyFunc<void()> completionHandler)
{
    doUpgrade(
        url,
        nx::network::http::Method::options,
        protocolToUpgradeTo,
        std::move(completionHandler));
}

void AsyncClient::doUpgrade(
    const nx::utils::Url& url,
    const Method& method,
    const std::string& protocolToUpgradeTo,
    nx::utils::MoveOnlyFunc<void()> completionHandler)
{
    m_onDone = std::move(completionHandler);
    doUpgrade(url, method, protocolToUpgradeTo);
}

void AsyncClient::doConnect(
    const nx::utils::Url& proxyUrl,
    const std::string& targetHost)
{
    NX_ASSERT(proxyUrl.isValid());

    resetDataBeforeNewRequest();
    m_requestUrl = proxyUrl;
    m_contentLocationUrl = proxyUrl;
    composeRequest(Method::connect);

    // Previous m_request, if any, is reset by resetDataBeforeNewRequest() call above.
    // Setting request-target to the authority form ([rfc7231#4.3.6]).
    m_request.requestLine.url = url::Builder().setAuthority(targetHost);

    initiateHttpMessageDelivery();
}

void AsyncClient::doConnect(
    const nx::utils::Url& proxyUrl,
    const std::string& targetHost,
    nx::utils::MoveOnlyFunc<void()> completionHandler)
{
    m_onDone = std::move(completionHandler);
    doConnect(proxyUrl, targetHost);
}

void AsyncClient::doRequest(
    const Method& method,
    const nx::utils::Url& urlOriginal)
{
    NX_VERBOSE(this, "Issuing request %1 %2 to %3", method, urlOriginal.path(), urlOriginal);

    nx::utils::Url url = urlOriginal;
    NX_ASSERT(!url.host().isEmpty() || m_messagePipeline, "Url: %1", url);

    if (m_messagePipeline && url.host().isEmpty())
    {
        url.setHost(m_messagePipeline->socket()->getForeignAddress().address.toString());
        url.setPort(m_messagePipeline->socket()->getForeignAddress().port);
    }

    NX_ASSERT(url.isValid(), "Url: %1", url);

    resetDataBeforeNewRequest();
    m_requestUrl = url;
    m_contentLocationUrl = url;
    composeRequest(method);
    if (m_requestBody)
    {
        addBodyToRequest();
    }
    else if (method == Method::post || method == Method::put)
    {
        m_request.headers.emplace("Content-Length", "0");
    }

    if (m_customRequestPrepareFunc)
        m_customRequestPrepareFunc(&m_request);

    initiateHttpMessageDelivery();
}

void AsyncClient::doRequest(
    const Method& method,
    const nx::utils::Url& url,
    nx::utils::MoveOnlyFunc<void()> completionHandler)
{
    m_onDone = std::move(completionHandler);
    doRequest(method, url);
}

const nx::network::http::Request& AsyncClient::request() const
{
    return m_request;
}

const Response* AsyncClient::response() const
{
    return m_response.type == MessageType::response
        ? m_response.response
        : nullptr;
}

std::string AsyncClient::contentType() const
{
    if (m_response.type == MessageType::none)
        return "";

    auto contentTypeIter = m_response.headers().find("Content-Type");
    if (contentTypeIter == m_response.headers().end())
        return "";
    return contentTypeIter->second;
}

bool AsyncClient::hasRequestSucceeded() const
{
    if (m_state == State::sFailed)
        return false;

    if (auto resp = response())
        return StatusCode::isSuccessCode(resp->statusLine.statusCode);

    return false;
}

nx::Buffer AsyncClient::fetchMessageBodyBuffer()
{
    if (logTraffic())
        NX_VERBOSE(this, "Response message body buffer:\n%1\n\n", m_responseMessageBody);

    return std::exchange(m_responseMessageBody, {});
}

std::unique_ptr<AbstractMsgBodySource> AsyncClient::takeResponseBodySource()
{
    NX_VERBOSE(this, "Connection to %1 is taken as response body source",
        m_contentLocationUrl);

    m_state = State::sDone;

    // Removing this from "connection closed" event receivers.
    m_messagePipeline->removeCloseHandler(m_closeHandlerId);

    return std::make_unique<HttpClientMessageBodySource>(
        *m_response.response,
        std::exchange(m_messagePipeline, nullptr));
}

const nx::utils::Url& AsyncClient::url() const
{
    return m_requestUrl;
}

const nx::utils::Url& AsyncClient::contentLocationUrl() const
{
    return m_contentLocationUrl;
}

quint64 AsyncClient::bytesRead() const
{
    return m_messagePipeline->totalBytesReceived();
}

void AsyncClient::setUseCompression(bool toggleUseEntityEncoding)
{
    m_contentEncodingUsed = toggleUseEntityEncoding;
}

void AsyncClient::setSubsequentReconnectTries(int /*reconnectTries*/)
{
    //TODO #akolesnikov
}

void AsyncClient::setTotalReconnectTries(int /*reconnectTries*/)
{
    //TODO #akolesnikov
}

void AsyncClient::setUserAgent(const std::string& userAgent)
{
    m_userAgent = userAgent;
}

const Credentials& AsyncClient::credentials() const
{
    return m_credentials;
}

void AsyncClient::setCredentials(const Credentials& credentials)
{
    m_credentials = credentials;
}

void AsyncClient::setProxyCredentials(const Credentials& credentials)
{
    m_proxyCredentials = credentials;
}

void AsyncClient::setProxyVia(
    const SocketAddress& proxyEndpoint, bool isSecure, ssl::AdapterFunc adapterFunc)
{
    if (proxyEndpoint.isNull())
    {
        m_proxyEndpoint.reset();
    }
    else
    {
        NX_ASSERT(proxyEndpoint.port > 0);
        m_proxyEndpoint = proxyEndpoint;
        m_isProxySecure = isSecure;
        m_proxyAdapterFunc = std::move(adapterFunc);
    }
}

void AsyncClient::setDisablePrecalculatedAuthorization(bool val)
{
    m_precalculatedAuthorizationDisabled = val;
}

void AsyncClient::setSendTimeout(std::chrono::milliseconds sendTimeout)
{
    m_timeouts.sendTimeout = sendTimeout;
}

void AsyncClient::setResponseReadTimeout(
    std::chrono::milliseconds _responseReadTimeout)
{
    m_timeouts.responseReadTimeout = _responseReadTimeout;
}

void AsyncClient::setMessageBodyReadTimeout(
    std::chrono::milliseconds messageBodyReadTimeout)
{
    m_timeouts.messageBodyReadTimeout = messageBodyReadTimeout;
}

void AsyncClient::setTimeouts(const Timeouts& timeouts)
{
    m_timeouts = timeouts;
}

void AsyncClient::stopWhileInAioThread()
{
    m_socket.reset();
    m_messagePipeline.reset();
    m_requestBody.reset();

    if (m_state == State::sReceivingResponse)
    {
        NX_VERBOSE(this, "Client is stopped while waiting for response from %1",
            m_contentLocationUrl);
    }
}

void AsyncClient::asyncConnectDone(SystemError::ErrorCode errorCode)
{
    NX_VERBOSE(this, "Connect to %1 completed with result %2", m_contentLocationUrl, errorCode);

    initializeMessagePipeline();

    if (errorCode == SystemError::noError)
    {
        NX_VERBOSE(this, "Sending request \"%1\" with body of %2 bytes to %3",
            m_request.requestLine, m_request.messageBody.size(), m_contentLocationUrl);

        m_remoteEndpointWithProtocol = endpointWithProtocol(m_contentLocationUrl);
        sendRequest();
        return;
    }

    NX_DEBUG(this, "Failed to establish TCP connection to %1. %2",
        m_contentLocationUrl, SystemError::toString(errorCode));

    reportConnectionFailure(errorCode);
}

void AsyncClient::sendRequest()
{
    m_state = State::sSendingRequest;

    Message msg(MessageType::request);
    *msg.request = m_request;
    m_messagePipeline->sendMessage(
        std::move(msg),
        [this](auto resultCode) { onRequestSent(resultCode); });
}

void AsyncClient::reportConnectionFailure(SystemError::ErrorCode err)
{
    m_lastSysErrorCode = err;

    m_state = State::sFailed;
    if (emitDone() != Result::proceed)
        return;

    m_socket.reset();
    m_messagePipeline.reset(); //< Closing failed connection so that it is not reused.
}

void AsyncClient::onRequestSent(SystemError::ErrorCode errorCode)
{
    if (errorCode != SystemError::noError)
    {
        NX_DEBUG(this, "Error sending (1) HTTP request to %1. %2",
            m_contentLocationUrl, SystemError::toString(errorCode));
        return;
    }

    NX_VERBOSE(this, "Request has been successfully sent to %1 from %2. %3",
        m_contentLocationUrl, m_messagePipeline->socket()->getLocalAddress(),
        logTraffic() ? request().toString() : request().requestLine.toString());

    ++m_totalRequestsSentViaCurrentConnection;
    ++m_totalRequestsSent;

    if (emitRequestHasBeenSent(m_authorizationTried) != Result::proceed)
        return;

    if (const auto& socket = m_messagePipeline->socket(); !socket->setRecvTimeout(kNoTimeout))
    {
        NX_VERBOSE(this, "Error configuring connection to %1. %2",
            m_contentLocationUrl, SystemError::getLastOSErrorText());
        if (!reconnectIfAppropriate())
        {
            m_state = State::sFailed;
            emitDone();
        }
        return;
    }

    m_state = State::sReceivingResponse;
    m_messagePipeline->startReadingConnection(m_timeouts.responseReadTimeout);
}

void AsyncClient::initializeMessagePipeline()
{
    m_messagePipeline = std::make_unique<AsyncMessagePipeline>(
        std::exchange(m_socket, nullptr));

    m_closeHandlerId = m_messagePipeline->registerCloseHandler(
        [this](auto reason, auto /*connectionDestroyed*/) { onConnectionClosed(reason); });
    m_messagePipeline->setMessageHandler(
        [this](auto&&... args) { onMessageReceived(std::forward<decltype(args)>(args)...); });
    m_messagePipeline->setOnSomeMessageBodyAvailable(
        [this](auto&&... args) { onSomeMessageBodyAvailable(std::forward<decltype(args)>(args)...); });
    m_messagePipeline->setOnMessageEnd(
        [this](auto&&... args) { onMessageEnd(std::forward<decltype(args)>(args)...); });

    m_isPersistentConnection = false;
}

void AsyncClient::onMessageReceived(Message message)
{
    if (isIgnoringCurrentMessage())
        return;

    if (message.type != nx::network::http::MessageType::response)
    {
        NX_DEBUG(this, "Received unexpected message of type %1 from %2 while expecting response! "
            "Ignoring...", message.type, m_contentLocationUrl);
        m_state = State::sFailed;
        emitDone();
        return;
    }

    m_response = std::move(message);

    m_isPersistentConnection =
        (m_response.version() == nx::network::http::http_1_1) &&
        (nx::network::http::getHeaderValue(m_response.headers(), "Connection") != "close");

    if (isMalformed(*m_response.response))
    {
        m_state = State::sFailed;
        emitDone();
        return;
    }

    NX_VERBOSE(this, "Response headers from %1 has been successfully read: %2",
        m_contentLocationUrl,
        nx::utils::trim(logTraffic() ? response()->toString() : response()->statusLine.toString(), "\r\n"));

    if (repeatRequestIfNeeded(*m_response.response))
        return;

    m_state = State::sResponseReceived;
    if (emitResponseReceived() != Result::proceed)
        return;

    const bool messageBodyAllowed = Method::isMessageBodyAllowedInResponse(
        m_request.requestLine.method,
        StatusCode::Value(m_response.response->statusLine.statusCode));

    const bool messageHasMessageBody =
        (m_messagePipeline->parser().streamReader().state() == HttpStreamReader::ReadState::readingMessageBody) ||
        (m_messagePipeline->parser().streamReader().state() == HttpStreamReader::ReadState::pullingLineEndingBeforeMessageBody);
    NX_ASSERT(m_messagePipeline->parser().streamReader().messageBodyBufferSize() == 0);

    if (!messageBodyAllowed && messageHasMessageBody)
    {
        // NOTE: The parser cannot make this decision due to the lack of
        // information (the request method).
        m_messagePipeline->parser().streamReader().forceEndOfMsgBody();
    }

    if (messageBodyAllowed && messageHasMessageBody)
    {
        m_messagePipeline->setInactivityTimeout(m_timeouts.messageBodyReadTimeout);
        m_state = State::sReadingMessageBody;
    }
}

void AsyncClient::onSomeMessageBodyAvailable(nx::Buffer buffer)
{
    NX_VERBOSE(this, "%1 message body bytes have been received from %2",
        buffer.size(), m_contentLocationUrl);

    if (isIgnoringCurrentMessage())
        return;

    m_responseMessageBody += std::move(buffer);

    emitSomeMessageBodyAvailable();
}

void AsyncClient::onMessageEnd()
{
    // NOTE: There is no "messageBegin" event which would increment m_currentMessageNumber,
    // so emulating it here.

    const auto ignore = isIgnoringCurrentMessage();
    ++m_messageReceivedThroughTheCurrentConnectionCount;
    if (ignore)
    {
        ++m_currentMessageNumber;
        return;
    }

    m_state = State::sDone;

    const auto handlerInvokationResult = emitDone();
    if (handlerInvokationResult != Result::proceed)
        return;

    ++m_currentMessageNumber;

    // User did not schedule new request.
    m_messagePipeline->stopReadingConnection();
}

void AsyncClient::onConnectionClosed(SystemError::ErrorCode errorCode)
{
    NX_VERBOSE(this, "Connection is closed in state %1. Url %2. %3",
        toString(m_state), m_contentLocationUrl, SystemError::toString(errorCode));

    m_lastSysErrorCode = errorCode;

    if (m_state == State::sDone)
        return;

    // Reconnecting only in case of failure.
    if (reconnectIfAppropriate())
        return;

    if (m_state != State::sFailed)
    {
        m_state = State::sFailed;
        NX_VERBOSE(this, "Moved to state %1", toString(m_state));

        if (emitDone() != Result::proceed)
            return;
    }

    m_messagePipeline.reset();   //< Closing failed socket so that it is not reused.
}

void AsyncClient::resetDataBeforeNewRequest()
{
    ++m_requestSequence;
    m_authorizationTried = false;
    m_ha1RecalcTried = false;
    m_numberOfRedirectsTried = 0;
    m_request = nx::network::http::Request();
    m_responseMessageBody.clear();
    m_response = Message();
}

int AsyncClient::totalRequestsSentViaCurrentConnection() const
{
    return m_totalRequestsSentViaCurrentConnection;
}

int AsyncClient::totalRequestsSent() const
{
    return m_totalRequestsSent;
}

void AsyncClient::initiateHttpMessageDelivery()
{
    const auto connectionReusePolicy = getConnectionReusePolicy();
    if (connectionReusePolicy == ConnectionReusePolicy::noReuse)
    {
        m_lastReportedMessageNumber = -1;
        m_lastSysErrorCode = SystemError::noError;
        m_totalRequestsSentViaCurrentConnection = 0;
        m_messageReceivedThroughTheCurrentConnectionCount = 0;
        m_currentMessageNumber = 0;
    }

    ++m_totalRequestsScheduled;
    // NOTE: The message number is counting from zero.
    m_awaitedMessageNumber = m_totalRequestsSentViaCurrentConnection;

    m_state = State::sInit;

    dispatch(
        [this, connectionReusePolicy]()
        {
            switch (connectionReusePolicy)
            {
                case ConnectionReusePolicy::establishedPipeline:
                    NX_VERBOSE(this, "Sending request %1 (url %2) via reused connection",
                        m_request.requestLine, m_contentLocationUrl);

                    m_remoteEndpointWithProtocol = endpointWithProtocol(m_contentLocationUrl);
                    sendRequest();
                    break;

                case ConnectionReusePolicy::rawConnection:
                    sendRequestOverExternalConnection();
                    break;

                case ConnectionReusePolicy::noReuse:
                    m_messagePipeline.reset();
                    initiateTcpConnection();
                    break;
            }
        });
}

AsyncClient::ConnectionReusePolicy AsyncClient::getConnectionReusePolicy() const
{
    if (m_messagePipeline
        && m_isPersistentConnection
        && m_remoteEndpointWithProtocol == endpointWithProtocol(m_contentLocationUrl)
        && m_lastSysErrorCode == SystemError::noError)
    {
        return ConnectionReusePolicy::establishedPipeline;
    }

    if (m_messagePipeline && m_totalRequestsScheduled == 0)
        return ConnectionReusePolicy::rawConnection;

    return ConnectionReusePolicy::noReuse;
}

void AsyncClient::sendRequestOverExternalConnection()
{
    NX_VERBOSE(this, "Sending request %1 (url %2) via external connection",
        m_request.requestLine, m_contentLocationUrl);

    if (!configureSocket(m_messagePipeline->socket().get()))
    {
        return post([this, err = SystemError::getLastOSErrorCode()]()
        {
            NX_DEBUG(this, "Error configuring connection to %1. %2",
                m_contentLocationUrl, SystemError::toString(err));
            reportConnectionFailure(err);
        });
    }

    m_remoteEndpointWithProtocol = endpointWithProtocol(m_contentLocationUrl);
    sendRequest();
}

bool AsyncClient::configureSocket(AbstractStreamSocket* connection)
{
    if (!connection->setNonBlockingMode(true) ||
        !connection->setSendTimeout(m_timeouts.sendTimeout) ||
        !connection->setRecvTimeout(m_timeouts.responseReadTimeout))
    {
        NX_VERBOSE(this, "Error configuring connection to %1. %2",
            m_contentLocationUrl, SystemError::getLastOSErrorText());
        return false;
    }

    return true;
}

void AsyncClient::initiateTcpConnection()
{
    NX_ASSERT(!m_socket);

    m_state = State::sInit;

    SocketAddress remoteAddress;
    bool isSecureConnection = false;
    if (m_proxyEndpoint)
    {
        remoteAddress = *m_proxyEndpoint;
        isSecureConnection = m_isProxySecure;
    }
    else
    {
        remoteAddress = nx::network::url::getEndpoint(m_contentLocationUrl);
        isSecureConnection = m_contentLocationUrl.scheme() == nx::network::http::kSecureUrlSchemeName;
    }

    if (remoteAddress.port == 0)
        remoteAddress.port = nx::network::http::defaultPort(isSecureConnection);

    const int ipVersion =
        (bool) HostAddress(m_contentLocationUrl.host().toStdString()).isPureIpV6()
        ? AF_INET6
        : SocketFactory::tcpClientIpVersion();

    m_socket =
        SocketFactory::createStreamSocket(m_proxyEndpoint ? m_proxyAdapterFunc : m_adapterFunc,
            isSecureConnection, nx::network::NatTraversalSupport::enabled, ipVersion);
    m_socket->bindToAioThread(getAioThread());
    if (m_keepAliveOptions)
    {
        if (!m_socket->setKeepAlive(*m_keepAliveOptions))
        {
            NX_DEBUG(this, "URL %1. Error enabling TCP keep-alive: %2",
                m_contentLocationUrl, SystemError::getLastOSErrorText());
        }
    }

    NX_VERBOSE(this, "Opening connection to %1. url %2, socket %3",
        remoteAddress, m_contentLocationUrl, m_socket->handle());

    if (!configureSocket(m_socket.get()))
        return post([this, err = SystemError::getLastOSErrorCode()]() { asyncConnectDone(err); });

    m_state = State::sWaitingConnectToHost;

    m_socket->connectAsync(
        remoteAddress,
        [this](auto result) { asyncConnectDone(result); });
}

void AsyncClient::stopReading()
{
    NX_ASSERT(isInSelfAioThread());

    m_messagePipeline->stopReadingConnection();
    m_readingCeased = true;
}

void AsyncClient::resumeReading()
{
    NX_ASSERT(isInSelfAioThread());

    if (m_readingCeased)
    {
        const auto timeout = (m_state == State::sReadingMessageBody)
            ? m_timeouts.messageBodyReadTimeout
            : m_timeouts.responseReadTimeout;

        m_messagePipeline->startReadingConnection(timeout);
        m_readingCeased = false;
    }
}

bool AsyncClient::isReading()
{
    NX_ASSERT(isInSelfAioThread());
    return !m_readingCeased;
}

bool AsyncClient::isMalformed(const nx::network::http::Response& response) const
{
    if (response.statusLine.statusCode == StatusCode::switchingProtocols)
    {
        if (nx::network::http::getHeaderValue(response.headers, "Upgrade").empty())
        {
            NX_DEBUG(this, "Received malformed response from %1. "
                "Status code is %2 and no Upgrade header present",
                m_contentLocationUrl, response.statusLine.statusCode);
            return true;
        }
    }

    return false;
}

bool AsyncClient::repeatRequestIfNeeded(const Response& response)
{
    switch (response.statusLine.statusCode)
    {
        case StatusCode::unauthorized:
        {
            if (!m_ha1RecalcTried &&
                response.headers.find(Qn::REALM_HEADER_NAME) != response.headers.cend())
            {
                m_authorizationTried = false;
                m_ha1RecalcTried = true;
            }

            if (!m_authorizationTried && (!m_credentials.username.empty() || !m_credentials.authToken.empty()))
            {
                //trying authorization
                if (resendRequestWithAuthorization(response))
                    return true;
            }

            break;
        }

        case StatusCode::proxyAuthenticationRequired:
        {
            if (!m_proxyAuthorizationTried &&
                (!m_proxyCredentials.username.empty() || !m_proxyCredentials.authToken.empty()))
            {
                if (resendRequestWithAuthorization(response, true))
                    return true;
            }
            break;
        }

        case StatusCode::movedPermanently:
        case StatusCode::found:
        case StatusCode::seeOther:
        case StatusCode::temporaryRedirect:
        case StatusCode::permanentRedirect:
            return sendRequestToNewLocation(response);

        default:
            break;
    }

    return false;
}

bool AsyncClient::sendRequestToNewLocation(const Response& response)
{
    if (m_numberOfRedirectsTried >= m_maxNumberOfRedirects)
        return false;
    ++m_numberOfRedirectsTried;

    // For now, using first Location if many have been provided in response.
    const auto locationIter = response.headers.find("Location");
    if (locationIter == response.headers.end())
        return false;

    NX_VERBOSE(this, nx::format("Redirect to location [ %1 ] from [ %2 ]").args(
        locationIter->second, m_contentLocationUrl));

    m_authorizationTried = false;
    m_ha1RecalcTried = false;

    nx::utils::Url newUrl(locationIter->second);
    if (newUrl.host().isEmpty())
    {
        newUrl.setHost(m_contentLocationUrl.host());
        newUrl.setPort(m_contentLocationUrl.port());
        newUrl.setScheme(m_contentLocationUrl.scheme());
    }
    m_contentLocationUrl = newUrl;

    const auto method = m_request.requestLine.method;
    composeRequest(method);
    initiateHttpMessageDelivery();
    return true;
}

void AsyncClient::composeRequest(const Method& httpMethod)
{
    const bool useHttp11 = true;   //TODO #akolesnikov check if we need it (e.g. we using keep-alive or requesting live capture)

    prepareRequestLine(useHttp11, httpMethod);

    // Adding user credentials.
    if (!m_contentLocationUrl.userName().isEmpty())
        m_credentials.username = m_contentLocationUrl.userName().toStdString();
    else
        m_contentLocationUrl.setUserName(m_credentials.username);

    if (!m_contentLocationUrl.password().isEmpty())
        m_credentials.authToken.setPassword(m_contentLocationUrl.password().toStdString());
    else if (m_credentials.authToken.isPassword())
        m_contentLocationUrl.setPassword(m_credentials.authToken.value);

    prepareRequestHeaders(useHttp11, httpMethod);
}

void AsyncClient::prepareRequestLine(
    bool useHttp11,
    const Method& httpMethod)
{
    m_request.requestLine.method = httpMethod;

    if (m_proxyEndpoint)
    {
        m_request.requestLine.url = m_contentLocationUrl;
    }
    else //< If no proxy specified then erasing http://host:port from request url.
    {
        if (httpMethod == nx::network::http::Method::options && m_contentLocationUrl.path().isEmpty())
            m_request.requestLine.url = "*";
        else
            m_request.requestLine.url = m_contentLocationUrl.path();
        m_request.requestLine.url.setQuery(m_contentLocationUrl.query());
        m_request.requestLine.url.setFragment(m_contentLocationUrl.fragment());
    }
    m_request.requestLine.version = useHttp11 ? nx::network::http::http_1_1 : nx::network::http::http_1_0;
}

void AsyncClient::prepareRequestHeaders(
    bool useHttp11,
    const Method& httpMethod)
{
    nx::network::http::insertOrReplaceHeader(
        &m_request.headers,
        HttpHeader("Date", nx::network::http::formatDateTime(QDateTime::currentDateTime())));
    nx::network::http::insertOrReplaceHeader(
        &m_request.headers,
        HttpHeader(
            "User-Agent",
            m_userAgent.empty() ? nx::network::http::userAgentString() : m_userAgent));
    if (useHttp11)
    {
        if (httpMethod == nx::network::http::Method::get || httpMethod == nx::network::http::Method::head)
        {
            if (m_contentEncodingUsed && m_additionalHeaders.count("Accept-Encoding") == 0)
            {
                http::insertOrReplaceHeader(
                    &m_request.headers,
                    HttpHeader("Accept-Encoding", "gzip"));
            }
        }

        if (m_additionalHeaders.count("Connection") == 0)
        {
            nx::network::http::insertOrReplaceHeader(
                &m_request.headers,
                HttpHeader("Connection", "keep-alive"));
        }

        if (m_additionalHeaders.count("Host") == 0)
            insertOrReplaceHeader(&m_request.headers, header::Host(url::getEndpoint(m_contentLocationUrl)));
    }

    // It is not correct just to replace headers because there
    // could be multiple headers with same name in m_additionalHeaders.
    for (const auto& header: m_additionalHeaders)
        m_request.headers.erase(header.first);
    m_request.headers.insert(m_additionalHeaders.cbegin(), m_additionalHeaders.cend());

    addAppropriateAuthenticationInformation();
}

void AsyncClient::addAppropriateAuthenticationInformation()
{
    // Adding X-Nx-User-Name to help server to port data from 2.1 to 2.3 and to 3.0
    // (generate user's digest after changing realm).
    // TODO: #akolesnikov Remove it after version prior to 3.0 support is over.
    if (!m_credentials.username.empty() &&
        m_request.headers.find(Qn::CUSTOM_USERNAME_HEADER_NAME) == m_request.headers.end())
    {
        nx::network::http::insertOrReplaceHeader(
            &m_request.headers,
            HttpHeader(Qn::CUSTOM_USERNAME_HEADER_NAME, m_credentials.username));
    }

    if (m_proxyCredentials.authToken.isBearerToken())
    {
        header::BearerAuthorization bearerAuthorization(m_proxyCredentials.authToken.value);
        nx::network::http::insertOrReplaceHeader(
            &m_request.headers,
            nx::network::http::HttpHeader(
                header::kProxyAuthorization, bearerAuthorization.serialized()));
    }

    if (m_credentials.authToken.isBearerToken())
    {
        header::BearerAuthorization bearerAuthorization(m_credentials.authToken.value);
        nx::network::http::insertOrReplaceHeader(
            &m_request.headers,
            nx::network::http::HttpHeader(
                header::Authorization::NAME,
                bearerAuthorization.serialized()));
        return;
    }

    if (m_precalculatedAuthorizationDisabled || m_credentials.username.empty())
        return;

    // Not using Basic authentication by default, since it is not secure.
    nx::network::http::removeHeader(&m_request.headers, header::Authorization::NAME);

    if (m_authType == AuthType::authBasic && m_credentials.authToken.isPassword())
    {
        addBasicAuthorizationToRequest();
    }
    else
    {
        // If there is a server' nonce in the cache, then calculating Digest response right now.
        const auto cachedServerResponse = AuthInfoCache::instance().getServerResponse(
            url::getEndpoint(m_contentLocationUrl),
            server::Role::resourceServer,
            m_credentials.username);

        if (cachedServerResponse &&
            cachedServerResponse->authScheme == header::AuthScheme::digest)
        {
            addDigestAuthorizationToRequest(
                url::getEndpoint(m_contentLocationUrl),
                server::Role::resourceServer,
                m_credentials,
                *cachedServerResponse,
                header::Authorization::NAME);
        }
    }
}

void AsyncClient::addBodyToRequest()
{
    if (!m_requestBody)
        return;

    m_request.headers.emplace("Content-Type", m_requestBody->mimeType());
    if (m_requestBody->contentLength())
    {
        m_request.headers.emplace(
            "Content-Length",
            std::to_string(*m_requestBody->contentLength()));
    }
    // TODO: #akolesnikov Support chunked encoding & compression.

    // TODO: #akolesnikov Add support for any body.
    NX_CRITICAL(
        dynamic_cast<AbstractMsgBodySource*>(m_requestBody.get()) != nullptr,
        "Only fixed request body supported at the moment");
    m_requestBody->readAsync(
        [this](SystemError::ErrorCode /*sysErrorCode*/, nx::Buffer buffer)
        {
            m_request.messageBody = std::move(buffer);
        });
}

bool AsyncClient::isIgnoringCurrentMessage() const
{
    return
        // Awaiting the next message already. Just skipping the current one.
        m_awaitedMessageNumber > m_messageReceivedThroughTheCurrentConnectionCount ||
        // Have already invoked "request completed" handler.
        m_lastReportedMessageNumber >= m_messageReceivedThroughTheCurrentConnectionCount;
}

void AsyncClient::addAdditionalHeader(const std::string& key, const std::string& value)
{
    m_additionalHeaders.emplace(key, value);
}

void AsyncClient::removeAdditionalHeader(const std::string& key)
{
    m_additionalHeaders.erase(key);
}

void AsyncClient::setAdditionalHeaders(HttpHeaders additionalHeaders)
{
    m_additionalHeaders = std::move(additionalHeaders);
}

void AsyncClient::addRequestHeaders(const HttpHeaders& headers)
{
    m_additionalHeaders.insert(headers.begin(), headers.end());
}

void AsyncClient::setCustomRequestPrepareFunc(CustomRequestPrepareFunc func)
{
    m_customRequestPrepareFunc = std::move(func);
}

bool AsyncClient::reconnectIfAppropriate()
{
    // Reconnecting only if persistent connection has been closed by the remote side during
    // the inactivity interval (the period between two requests).
    // Checking that at least one response was successfully received through the connection.

    if ((m_state == State::sSendingRequest || m_state == State::sReceivingResponse) &&
        m_messageReceivedThroughTheCurrentConnectionCount > 0)
    {
        m_messagePipeline.reset();
        initiateHttpMessageDelivery();
        return true;
    }

    return false;
}

std::string AsyncClient::endpointWithProtocol(const nx::utils::Url& url)
{
    return nx::format("%1://%2:%3").args(url.scheme(), url.host(),
        url.port(nx::network::http::defaultPortForScheme(url.scheme().toStdString()))).toStdString();
}

static std::optional<header::WWWAuthenticate> extractAuthenticateHeader(
    const HttpHeaders& headers, bool isProxy, AuthType authType)
{
    const std::string authenticateHeaderName = isProxy ? "Proxy-Authenticate" : "WWW-Authenticate";

    std::optional<header::WWWAuthenticate> result;
    const auto [authHeaderBegin, authHeaderEnd] = headers.equal_range(authenticateHeaderName);
    for (HttpHeaders::const_iterator it = authHeaderBegin; it != authHeaderEnd; it++)
    {
        result = header::WWWAuthenticate();
        if (!result->parse(it->second))
        {
            NX_INFO(typeid(AsyncClient), "Error while parsing %1 header: '%2'. Skipping it.",
                authenticateHeaderName, it->second);
            continue;
        }

        auto authScheme = result->authScheme;
        if (authType == AuthType::authBasic && authScheme == header::AuthScheme::basic)
            return result;
        if (authType == AuthType::authDigest && authScheme == header::AuthScheme::digest)
            return result;

        // Try to use digest if both are available.
        if (authType == AuthType::authBasicAndDigest && authScheme == header::AuthScheme::digest)
            return result;
    }

    // Lets use basic auth if digest header is not available.
    if (authType == AuthType::authBasicAndDigest)
        return result;
    return std::nullopt;
}

bool AsyncClient::resendRequestWithAuthorization(
    const nx::network::http::Response& response,
    bool isProxy)
{
    // If response contains WWW-Authenticate with Digest authentication, generating
    // "Authorization: Digest" header and adding it to custom headers.
    NX_ASSERT(response.statusLine.statusCode == StatusCode::unauthorized ||
        response.statusLine.statusCode == StatusCode::proxyAuthenticationRequired);

    const std::string authorizationHeaderName =
        isProxy ? std::string("Proxy-Authorization") : header::Authorization::NAME;
    const auto credentials = isProxy ? m_proxyCredentials : m_credentials;

    auto wwwAuthenticateHeader = extractAuthenticateHeader(response.headers, isProxy, m_authType);
    if (!wwwAuthenticateHeader)
        return false;

    if (wwwAuthenticateHeader->authScheme == header::AuthScheme::digest)
    {
        const auto& serverEndpoint = isProxy && m_proxyEndpoint
            ? *m_proxyEndpoint
            : url::getEndpoint(m_contentLocationUrl);
        const auto serverRole = isProxy ? server::Role::proxy : server::Role::resourceServer;

        AuthInfoCache::instance().cacheServerResponse(
            serverEndpoint,
            serverRole,
            credentials.username,
            *wwwAuthenticateHeader);

        if (!addDigestAuthorizationToRequest(
                serverEndpoint,
                serverRole,
                credentials,
                *wwwAuthenticateHeader,
                authorizationHeaderName))
        {
            return false;
        }
    }
    else if (wwwAuthenticateHeader->authScheme == header::AuthScheme::basic &&
        (credentials.authToken.isPassword() || credentials.authToken.empty()))
    {
        addBasicAuthorizationToRequest();
    }

    doSomeCustomLogic(response, &m_request);

    if (isProxy)
        m_proxyAuthorizationTried = true;
    else
        m_authorizationTried = true;
    initiateHttpMessageDelivery();
    return true;
}

bool AsyncClient::addDigestAuthorizationToRequest(
    const SocketAddress& serverEndpoint,
    server::Role serverRole,
    const Credentials& credentials,
    const header::WWWAuthenticate& authenticateHeader,
    const std::string_view& authorizationHeaderName)
{
    int nonceCount = 1;
    if (const auto nonceIter = authenticateHeader.params.find("nonce");
        nonceIter != authenticateHeader.params.end())
    {
        nonceCount = NonceCache::instance().getNonceCount(
            serverEndpoint, serverRole, nonceIter->second);
    }

    const auto authorization = generateDigestAuthorization(
        m_request,
        credentials,
        authenticateHeader,
        nonceCount);
    if (!authorization)
    {
        NX_VERBOSE(this, "Failed to generate Authorization header for URL %1, username %2",
            m_contentLocationUrl, credentials.username);
        return false;
    }

    nx::network::http::insertOrReplaceHeader(
        &m_request.headers,
        nx::network::http::HttpHeader(
            authorizationHeaderName,
            authorization->serialized()));

    return true;
}

void AsyncClient::addBasicAuthorizationToRequest()
{
    header::BasicAuthorization basicAuthorization(
        m_credentials.username,
        m_credentials.authToken.value);
    nx::network::http::insertOrReplaceHeader(
        &m_request.headers,
        nx::network::http::HttpHeader(
            header::Authorization::NAME,
            basicAuthorization.serialized()));
}

void AsyncClient::doSomeCustomLogic(
    const nx::network::http::Response& response,
    Request* const request)
{
    // TODO: #akolesnikov This method is not part of http, so it should not be in this class.

    if (!m_credentials.authToken.isPassword())
        return;

    auto realmIter = response.headers.find(Qn::REALM_HEADER_NAME);
    if (realmIter == response.headers.end())
        return;

    // Calculating user's digest with new realm.
    const auto newRealmDigest = calcHa1(
        m_credentials.username,
        realmIter->second,
        m_credentials.authToken.value);
    const auto cryptSha512Hash = nx::crypt::linuxCryptSha512(
        QByteArray::fromStdString(m_credentials.authToken.value),
        nx::crypt::generateSalt(nx::crypt::kLinuxCryptSaltLength));

    nx::network::http::insertOrReplaceHeader(
        &request->headers,
        HttpHeader(Qn::HA1_DIGEST_HEADER_NAME, newRealmDigest));
    nx::network::http::insertOrReplaceHeader(
        &request->headers,
        HttpHeader(Qn::CRYPT_SHA512_HASH_HEADER_NAME, cryptSha512Hash));
    nx::network::http::insertOrReplaceHeader(
        &request->headers,
        HttpHeader(Qn::REALM_HEADER_NAME, realmIter->second));
}

const char* AsyncClient::toString(State state)
{
    switch (state)
    {
    case State::sInit:
        return "init";
    case State::sWaitingConnectToHost:
        return "waitingConnectToHost";
    case State::sSendingRequest:
        return "sendingRequest";
    case State::sReceivingResponse:
        return "receivingResponse";
    case State::sResponseReceived:
        return "responseReceived";
    case State::sReadingMessageBody:
        return "readingMessageBody";
    case State::sFailed:
        return "failed";
    case State::sDone:
        return "done";
    default:
        return "unknown";
    }
}

void AsyncClient::setAuthType(AuthType value)
{
    m_authType = value;
}

AsyncClient::Result AsyncClient::emitDone()
{
    m_lastReportedMessageNumber = m_currentMessageNumber;
    return invokeHandler(m_onDone);
}

AsyncClient::Result AsyncClient::emitRequestHasBeenSent(bool authorizationTried)
{
    return invokeHandler(m_onRequestHasBeenSent, authorizationTried);
}

AsyncClient::Result AsyncClient::emitResponseReceived()
{
    return invokeHandler(m_onResponseReceived);
}

AsyncClient::Result AsyncClient::emitSomeMessageBodyAvailable()
{
    return invokeHandler(m_onSomeMessageBodyAvailable);
}

void AsyncClient::setMaxNumberOfRedirects(int maxNumberOfRedirects)
{
    m_maxNumberOfRedirects = maxNumberOfRedirects;
}

template<typename ... Args>
AsyncClient::Result AsyncClient::invokeHandler(
    nx::utils::MoveOnlyFunc<void(Args...)>& handler,
    Args... args)
{
    if (!handler)
        return Result::proceed;

    const auto requestSequenceBak = m_requestSequence;
    nx::utils::InterruptionFlag::Watcher objectDestructionWatcher(&m_objectDestructionFlag);

    handler(args...);

    if (objectDestructionWatcher.interrupted())
        return Result::thisDestroyed;

    if (m_requestSequence != requestSequenceBak)
        return Result::newRequestScheduled;

    if (!m_socket && !m_messagePipeline)
        return Result::cancelled;

    return Result::proceed;
}

ssl::AdapterFunc AsyncClient::setAdapterFunc(ssl::AdapterFunc adapterFunc)
{
    if (NX_ASSERT(adapterFunc))
        return std::exchange(m_adapterFunc, std::move(adapterFunc));
    return m_adapterFunc;
}

const ssl::AdapterFunc& AsyncClient::adapterFunc() const
{
    return m_adapterFunc;
}

void AsyncClient::setForceTrafficLogging(bool value)
{
    forceTrafficLogging = value;
}

} // namespace nx::network::http
