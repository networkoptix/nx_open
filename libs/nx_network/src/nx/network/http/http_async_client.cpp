#include "http_async_client.h"

#include <QtCore/QCryptographicHash>
#include <QtCore/QDateTime>

#include <nx/network/buffered_stream_socket.h>
#include <nx/network/socket_factory.h>
#include <nx/network/socket_global.h>
#include <nx/network/url/url_builder.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/utils/log/log.h>
#include <nx/utils/thread/mutex.h>

#include <nx/utils/crypt/linux_passwd_crypt.h>
#include <nx/utils/system_error.h>

#include "auth_tools.h"
#include "buffer_source.h"
#include "custom_headers.h"

#include <nx/fusion/model_functions.h>

using std::make_pair;

namespace {

static bool logTraffic()
{
    return nx::network::SocketGlobals::ini().httpClientTraffic;
}

} // namespace

namespace nx {
namespace network {
namespace http {

static const size_t RESPONSE_BUFFER_SIZE = 16 * 1024;

constexpr const std::chrono::milliseconds AsyncClient::Timeouts::kDefaultSendTimeout;
constexpr const std::chrono::milliseconds AsyncClient::Timeouts::kDefaultResponseReadTimeout;
constexpr const std::chrono::milliseconds AsyncClient::Timeouts::kDefaultMessageBodyReadTimeout;

AsyncClient::AsyncClient():
    m_state(State::sInit),
    m_connectionClosed(false),
    m_requestBytesSent(0),
    m_authorizationTried(false),
    m_proxyAuthorizationTried(false),
    m_ha1RecalcTried(false),
    m_terminated(false),
    m_totalBytesReadPerRequest(0),
    m_totalRequestsSentViaCurrentConnection(0),
    m_totalRequestsSent(0),
    m_contentEncodingUsed(true),
    m_sendTimeout(Timeouts::kDefaultSendTimeout),
    m_responseReadTimeout(Timeouts::kDefaultResponseReadTimeout),
    m_msgBodyReadTimeout(0),
    m_authType(AuthType::authBasicAndDigest),
    m_awaitedMessageNumber(0),
    m_lastSysErrorCode(SystemError::noError),
    m_requestSequence(0),
    m_forcedEof(false),
    m_precalculatedAuthorizationDisabled(false),
    m_numberOfRedirectsTried(0)
{
    m_responseBuffer.reserve(RESPONSE_BUFFER_SIZE);
}

AsyncClient::AsyncClient(std::unique_ptr<AbstractStreamSocket> socket):
    AsyncClient()
{
    m_socket = std::move(socket);
}

AsyncClient::~AsyncClient()
{
}

const std::unique_ptr<AbstractStreamSocket>& AsyncClient::socket()
{
    return m_socket;
}

std::unique_ptr<AbstractStreamSocket> AsyncClient::takeSocket()
{
    NX_ASSERT(isInSelfAioThread());

    m_terminated = true;

    if (!m_socket)
        return nullptr;

    std::unique_ptr<AbstractStreamSocket> result;
    result.swap(m_socket);
    result->cancelIOSync(nx::network::aio::etNone);
    if (!m_receivedBytesLeft.isEmpty())
    {
        decltype(m_receivedBytesLeft) receivedBytesLeft;
        receivedBytesLeft.swap(m_receivedBytesLeft);
        auto bufferedStreamSocket = std::make_unique<nx::network::BufferedStreamSocket>(
            std::move(result),
            std::move(receivedBytesLeft));
        result = std::move(bufferedStreamSocket);
    }
    return result;
}

void AsyncClient::bindToAioThread(nx::network::aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);
    if (m_socket)
        m_socket->bindToAioThread(aioThread);
    if (m_requestBody)
        m_requestBody->bindToAioThread(aioThread);
}

AsyncClient::State AsyncClient::state() const
{
    return m_state;
}

bool AsyncClient::failed() const
{
    return m_state == State::sFailed || (response() == nullptr && !m_expectOnlyBody);
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
    nx::utils::MoveOnlyFunc<void()> completionHandler)
{
    m_onDone = std::move(completionHandler);
    doPut(url);
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
    const StringType& protocolToUpgradeTo)
{
    doUpgrade(url, Method::options, protocolToUpgradeTo);
}

void AsyncClient::doUpgrade(
    const nx::utils::Url& url,
    nx::network::http::Method::ValueType method,
    const StringType& protocolToUpgradeTo)
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
    const StringType& protocolToUpgradeTo,
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
    nx::network::http::Method::ValueType method,
    const StringType& protocolToUpgradeTo,
    nx::utils::MoveOnlyFunc<void()> completionHandler)
{
    m_onDone = std::move(completionHandler);
    doUpgrade(url, method, protocolToUpgradeTo);
}

void AsyncClient::doConnect(
    const nx::utils::Url& proxyUrl,
    const StringType& targetHost)
{
    NX_ASSERT(proxyUrl.isValid());

    resetDataBeforeNewRequest();
    m_requestUrl = proxyUrl;
    m_contentLocationUrl = proxyUrl;
    composeRequest(Method::connect);

    // TODO: #ak Setting request-target to authority form ([rfc7231#4.3.6]).
    m_request.requestLine.url = nx::network::url::Builder().setPath(targetHost);

    initiateHttpMessageDelivery();
}

void AsyncClient::doConnect(
    const nx::utils::Url& proxyUrl,
    const StringType& targetHost,
    nx::utils::MoveOnlyFunc<void()> completionHandler)
{
    m_onDone = std::move(completionHandler);
    doConnect(proxyUrl, targetHost);
}

void AsyncClient::doRequest(
    nx::network::http::Method::ValueType method,
    const nx::utils::Url& urlOriginal)
{
    nx::utils::Url url = urlOriginal;
    if (url.host().isEmpty() && m_socket != nullptr)
    {
        url.setHost(m_socket->getForeignAddress().address.toString());
        url.setPort(m_socket->getForeignAddress().port);
    }

    NX_ASSERT(!url.host().isEmpty() || m_socket, url.toString());
    NX_ASSERT(url.isValid(), url.toString());

    resetDataBeforeNewRequest();
    m_requestUrl = url;
    m_contentLocationUrl = url;
    composeRequest(method);
    if (m_requestBody)
        addBodyToRequest();

    initiateHttpMessageDelivery();
}

void AsyncClient::doRequest(
    nx::network::http::Method::ValueType method,
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
    const Message& httpMsg = m_httpStreamReader.message();
    return httpMsg.type == MessageType::response ? httpMsg.response : NULL;
}

StringType AsyncClient::contentType() const
{
    const Message& httpMsg = m_httpStreamReader.message();
    if (httpMsg.type == MessageType::none)
        return StringType();
    HttpHeaders::const_iterator contentTypeIter = httpMsg.headers().find("Content-Type");
    if (contentTypeIter == httpMsg.headers().end())
        return StringType();
    return contentTypeIter->second;
}

bool AsyncClient::hasRequestSucceeded() const
{
    if (state() == State::sFailed)
        return false;

    if (auto resp = response())
        return StatusCode::isSuccessCode(resp->statusLine.statusCode);

    return false;
}

BufferType AsyncClient::fetchMessageBodyBuffer()
{
    const auto buffer = m_httpStreamReader.fetchMessageBody();
    if (logTraffic())
        NX_VERBOSE(this, lm("Response message body buffer:\n%1\n\n").arg(buffer));

    return buffer;
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
    return m_totalBytesReadPerRequest;
}

void AsyncClient::setUseCompression(bool toggleUseEntityEncoding)
{
    m_contentEncodingUsed = toggleUseEntityEncoding;
}

void AsyncClient::setSubsequentReconnectTries(int /*reconnectTries*/)
{
    //TODO #ak
}

void AsyncClient::setTotalReconnectTries(int /*reconnectTries*/)
{
    //TODO #ak
}

void AsyncClient::setUserAgent(const QString& userAgent)
{
    m_userAgent = userAgent;
}

void AsyncClient::setUserName(const QString& userName)
{
    m_user.username = userName;
}

Credentials AsyncClient::credentials() const
{
    return m_user;
}

void AsyncClient::setCredentials(const Credentials& credentials)
{
    m_user = credentials;
}

void AsyncClient::setUserPassword(const QString& userPassword)
{
    AuthToken authToken;
    authToken.setPassword(userPassword.toUtf8());
    setUserAuthToken(authToken);
}

void AsyncClient::setUserAuthToken(const AuthToken& userToken)
{
    m_user.authToken = userToken;
}

void AsyncClient::setUserCredentials(const Credentials& userCredentials)
{
    m_user = userCredentials;
}

void AsyncClient::setProxyUserName(const QString& userName)
{
    m_proxyUser.username = userName;
}

void AsyncClient::setProxyUserPassword(const QString& userPassword)
{
    AuthToken authToken;
    authToken.setPassword(userPassword.toUtf8());
    setProxyUserAuthToken(authToken);
}

void AsyncClient::setProxyUserAuthToken(const AuthToken& proxyUserToken)
{
    m_proxyUser.authToken = proxyUserToken;
}

void AsyncClient::setProxyUserCredentials(const Credentials& userCredentials)
{
    m_proxyUser = userCredentials;
}

void AsyncClient::setAuth(const AuthInfo& auth)
{
    m_user = auth.user;
    m_proxyUser = auth.proxyUser;

    setProxyVia(auth.proxyEndpoint, auth.isProxySecure);
}

void AsyncClient::setProxyVia(const SocketAddress& proxyEndpoint, bool isSecure)
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
    }
}

void AsyncClient::setDisablePrecalculatedAuthorization(bool val)
{
    m_precalculatedAuthorizationDisabled = val;
}

void AsyncClient::setSendTimeout(std::chrono::milliseconds sendTimeout)
{
    m_sendTimeout = sendTimeout;
}

void AsyncClient::setResponseReadTimeout(
    std::chrono::milliseconds _responseReadTimeout)
{
    m_responseReadTimeout = _responseReadTimeout;
}

void AsyncClient::setMessageBodyReadTimeout(
    std::chrono::milliseconds messageBodyReadTimeout)
{
    m_msgBodyReadTimeout = messageBodyReadTimeout;
}

void AsyncClient::setTimeouts(Timeouts timeouts)
{
    setSendTimeout(timeouts.sendTimeout);
    setResponseReadTimeout(timeouts.responseReadTimeout);
    setMessageBodyReadTimeout(timeouts.messageBodyReadTimeout);
}

void AsyncClient::stopWhileInAioThread()
{
    m_socket.reset();
    m_requestBody.reset();

    if (m_state == State::sReceivingResponse)
    {
        NX_VERBOSE(this, "Client is stopped while waiting for response from %1",
            m_contentLocationUrl);
    }
}

void AsyncClient::asyncConnectDone(SystemError::ErrorCode errorCode)
{
    NX_VERBOSE(this, "Opened connection to url %1. Result code %2", m_contentLocationUrl, errorCode);

    if (m_terminated)
        return;

    if (m_state != State::sWaitingConnectToHost)
    {
        NX_ASSERT(false);
        return;
    }

    if (errorCode == SystemError::noError)
    {
        //connect successful
        m_remoteEndpointWithProtocol = endpointWithProtocol(m_contentLocationUrl);
        serializeRequest();
        m_state = State::sSendingRequest;
        using namespace std::placeholders;
        NX_VERBOSE(this, "Sending request to url %1", m_contentLocationUrl);

        m_socket->sendAsync(m_requestBuffer, std::bind(&AsyncClient::asyncSendDone, this, _1, _2));
        return;
    }

    NX_DEBUG(this, "Failed to establish tcp connection to %1. %2",
        m_contentLocationUrl, SystemError::toString(errorCode));
    m_lastSysErrorCode = errorCode;

    m_state = State::sFailed;
    const auto requestSequenceBak = m_requestSequence;
    if (emitDone() == Result::thisDestroyed)
        return;
    if (m_requestSequence == requestSequenceBak)
        m_socket.reset();   //< Closing failed socket so that it is not reused.
}

void AsyncClient::asyncSendDone(SystemError::ErrorCode errorCode, size_t bytesWritten)
{
    if (m_terminated)
        return;

    if (m_state != State::sSendingRequest)
    {
        NX_ASSERT(false);
        return;
    }

    if (errorCode != SystemError::noError)
    {
        if (reconnectIfAppropriate())
            return;
        NX_DEBUG(this, lm("Error sending (1) http request to %1. %2")
            .arg(m_contentLocationUrl).arg(SystemError::toString(errorCode)));
        m_state = State::sFailed;
        m_lastSysErrorCode = errorCode;
        const auto requestSequenceBak = m_requestSequence;
        if (emitDone() == Result::thisDestroyed)
            return;
        if (m_requestSequence == requestSequenceBak)
            m_socket.reset();
        return;
    }

    //TODO #ak recheck connection closure situation

    using namespace std::placeholders;

    m_requestBytesSent += bytesWritten;
    if ((int)m_requestBytesSent < m_requestBuffer.size())
    {
        m_socket->sendAsync(m_requestBuffer, std::bind(&AsyncClient::asyncSendDone, this, _1, _2));
        return;
    }

    NX_VERBOSE(this, "Request has been successfully sent to %1 from %2. %3",
        m_contentLocationUrl, m_socket->getLocalAddress(),
        logTraffic() ? request().toString() : request().requestLine.toString());

    const auto requestSequenceBak = m_requestSequence;
    if (emitRequestHasBeenSent(m_authorizationTried) == Result::thisDestroyed)
        return;
    if (m_terminated || //user cancelled futher action
        (m_requestSequence != requestSequenceBak))  //user started new request within responseReceived handler
    {
        return;
    }

    NX_VERBOSE(this, "Waiting for response. Timeout %1", m_responseReadTimeout);

    m_state = m_expectOnlyBody ? State::sReadingMessageBody : State::sReceivingResponse;
    m_responseBuffer.resize(0);
    if (!m_socket->setRecvTimeout(m_responseReadTimeout))
    {
        const auto sysErrorCode = SystemError::getLastOSErrorCode();

        if (reconnectIfAppropriate())
            return;

        NX_DEBUG(this, lm("Url %1. Error setting receive timeout to %2. %3")
            .arg(m_contentLocationUrl).arg(m_responseReadTimeout)
            .arg(SystemError::toString(sysErrorCode)));
        m_state = State::sFailed;
        const auto requestSequenceBak = m_requestSequence;
        if (emitDone() == Result::thisDestroyed)
            return;
        if (m_requestSequence == requestSequenceBak)
            m_socket.reset();   //< Closing failed socket so that it is not reused.
        return;
    }

    if (!m_receivedBytesLeft.isEmpty())
    {
        // Processing what we have in the buffer first.
        NX_ASSERT(m_responseBuffer.isEmpty());
        m_responseBuffer.swap(m_receivedBytesLeft);
        m_responseBuffer.reserve(RESPONSE_BUFFER_SIZE);
        processReceivedBytes(m_responseBuffer.size());
    }
    else if (!m_readingCeased)
    {
        m_readInvoked = true;
        m_socket->readSomeAsync(
            &m_responseBuffer,
            std::bind(&AsyncClient::onSomeBytesReadAsync, this, _1, _2));
    }
}

void AsyncClient::onSomeBytesReadAsync(
    SystemError::ErrorCode errorCode,
    size_t bytesRead)
{
    using namespace std::placeholders;

    if (m_terminated)
        return;

    m_readInvoked = false;

    if (errorCode != SystemError::noError)
    {
        const auto stateBak = m_state;
        if ((m_httpStreamReader.state() == HttpStreamReader::messageDone) &&
                m_httpStreamReader.currentMessageNumber() == m_awaitedMessageNumber)
        {
            m_state = State::sDone;
        }
        else
        {
            // Reconnecting only in case of failure.
            if (reconnectIfAppropriate())
                return;
            m_state = State::sFailed;
        }

        NX_DEBUG(this, lm("Error reading (state %1) http response from %2. %3")
            .arg(toString(stateBak)).arg(m_contentLocationUrl).arg(SystemError::toString(errorCode)));
        if (stateBak == State::sReadingMessageBody)
        {
            const auto contentLength = m_httpStreamReader.contentLength();
            NX_DEBUG(this, "%1 out of %2 bytes of incomplete message body have been read",
                m_httpStreamReader.messageBodyBytesRead(), contentLength ? std::to_string(*contentLength) : "?");
        }

        m_lastSysErrorCode = errorCode;
        const auto requestSequenceBak = m_requestSequence;
        if (emitDone() == Result::thisDestroyed)
            return;
        if (m_requestSequence == requestSequenceBak)
            m_socket.reset();   //< Closing failed socket so that it is not reused.
        return;
    }

    processReceivedBytes(bytesRead);
}

void AsyncClient::resetDataBeforeNewRequest()
{
    ++m_requestSequence;
    m_authorizationTried = false;
    m_ha1RecalcTried = false;
    m_numberOfRedirectsTried = 0;
    m_request = nx::network::http::Request();
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
    using namespace std::placeholders;

    const bool canUseExistingConnection = canExistingConnectionBeUsed();
    if (!canUseExistingConnection)
    {
        m_httpStreamReader.resetState();
        m_receivedBytesLeft.clear();
        m_awaitedMessageNumber = 0;
        m_lastSysErrorCode = SystemError::noError;
        m_totalRequestsSentViaCurrentConnection = 0;
    }

    ++m_totalRequestsSentViaCurrentConnection;
    ++m_totalRequestsSent;
    m_totalBytesReadPerRequest = 0;

    m_state = State::sInit;

    dispatch(
        [this, canUseExistingConnection]()
        {
            if (canUseExistingConnection)
            {
                NX_ASSERT(m_socket);
                m_socket->cancelIOSync(nx::network::aio::etNone);
                ++m_awaitedMessageNumber;   //< Current message will be skipped.

                serializeRequest();
                m_state = State::sSendingRequest;
                NX_VERBOSE(this, "Sending request to url %1 via reused connection",
                    m_contentLocationUrl);
                m_socket->sendAsync(
                    m_requestBuffer,
                    std::bind(&AsyncClient::asyncSendDone, this, _1, _2));
                return;
            }
            // Keep socket for the very first request if it pass in constructor.
            if (m_totalRequestsSent > 1)
                m_socket.reset();
            initiateTcpConnection();
        });
}

bool AsyncClient::canExistingConnectionBeUsed() const
{
    bool canUseExistingConnection = false;
    if (m_httpStreamReader.message().type == nx::network::http::MessageType::response)
    {
        canUseExistingConnection =
            (m_httpStreamReader.message().response->statusLine.version == nx::network::http::http_1_1) &&
            (nx::network::http::getHeaderValue(m_httpStreamReader.message().response->headers, "Connection") != "close");
    }

    canUseExistingConnection =
        m_socket &&
        !m_connectionClosed &&
        canUseExistingConnection &&
        (m_remoteEndpointWithProtocol == endpointWithProtocol(m_contentLocationUrl)) &&
        m_lastSysErrorCode == SystemError::noError;

    return canUseExistingConnection;
}

void AsyncClient::initiateTcpConnection()
{
    m_state = State::sInit;

    SocketAddress remoteAddress;
    bool isSecureConnection = false;
    if (m_proxyEndpoint)
    {
        remoteAddress = m_proxyEndpoint.get();
        isSecureConnection = m_isProxySecure;
    }
    else
    {
        remoteAddress = nx::network::url::getEndpoint(m_contentLocationUrl);
        isSecureConnection = m_contentLocationUrl.scheme() == nx::network::http::kSecureUrlSchemeName;
    }

    if (remoteAddress.port == 0)
        remoteAddress.port = nx::network::http::defaultPort(isSecureConnection);

    if (!m_socket)
    {
        const int ipVersion =
            (bool)HostAddress(m_contentLocationUrl.host()).isPureIpV6()
            ? AF_INET6
            : SocketFactory::tcpClientIpVersion();

        m_socket = SocketFactory::createStreamSocket(
            isSecureConnection,
            nx::network::NatTraversalSupport::enabled,
            ipVersion);
        NX_VERBOSE(this, "Opening connection to %1, URL %2, socket %3",
            remoteAddress, m_contentLocationUrl, m_socket->handle());
    }

    m_socket->bindToAioThread(getAioThread());
    m_connectionClosed = false;

    if (!m_socket->setNonBlockingMode(true) ||
        !m_socket->setSendTimeout(m_sendTimeout) ||
        !m_socket->setRecvTimeout(m_responseReadTimeout))
    {
        m_socket->post(
            std::bind(
                &AsyncClient::asyncConnectDone,
                this,
                SystemError::getLastOSErrorCode()));
        return;
    }

    m_state = State::sWaitingConnectToHost;

    if (m_socket->isConnected())
    {
        m_socket->post(
            std::bind(
                &AsyncClient::asyncConnectDone,
                this,
                SystemError::noError));
    }
    else
    {
        m_socket->connectAsync(
            remoteAddress,
            std::bind(&AsyncClient::asyncConnectDone, this, std::placeholders::_1));
    }
}

size_t AsyncClient::parseReceivedBytes(size_t bytesRead)
{
    if (bytesRead == 0)   //connection closed
    {
        //closing connection is a valid HTTP way to signal message end
        //m_state = m_httpStreamReader.state() == HttpStreamReader::messageDone ? sDone : sFailed;
        //TODO #ak check if whole message body is received (if message body size is known)
        m_httpStreamReader.flush();
        if (m_httpStreamReader.state() == HttpStreamReader::readingMessageBody &&
            m_httpStreamReader.contentLength() &&
            m_httpStreamReader.contentLength().get() > m_httpStreamReader.messageBodyBytesRead())
        {
            m_state = State::sFailed;
            m_lastSysErrorCode = SystemError::connectionReset;
        }
        else
        {
            m_state = (m_httpStreamReader.state() == HttpStreamReader::messageDone) ||
                (m_httpStreamReader.state() == HttpStreamReader::pullingLineEndingBeforeMessageBody) ||
                (m_httpStreamReader.state() == HttpStreamReader::readingMessageBody)
                ? State::sDone
                : State::sFailed;
        }

        m_connectionClosed = true;
        return 0;
    }

    // m_httpStreamReader is allowed to process not all bytes from m_responseBuffer.
    std::size_t bytesProcessed = 0;

    if (m_expectOnlyBody)
        m_httpStreamReader.setState(HttpStreamReader::ReadState::readingMessageBody);

    if (!m_httpStreamReader.parseBytes(m_responseBuffer, bytesRead, &bytesProcessed))
    {
        NX_DEBUG(this, lm("Error parsing http response from %1. %2")
            .arg(m_contentLocationUrl).arg(m_httpStreamReader.errorText()));
        m_state = State::sFailed;
        return -1;
    }

    m_totalBytesReadPerRequest += bytesProcessed;

    if (m_httpStreamReader.state() == HttpStreamReader::parseError)
    {
        m_state = State::sFailed;
        return bytesProcessed;
    }

    NX_ASSERT(m_httpStreamReader.currentMessageNumber() <= m_awaitedMessageNumber);
    if (m_httpStreamReader.currentMessageNumber() < m_awaitedMessageNumber)
        return bytesProcessed;   //reading some old message, not changing state in this case

    if (m_httpStreamReader.state() == HttpStreamReader::messageDone)
        m_state = State::sDone;
    return bytesProcessed;
}

void AsyncClient::processReceivedBytes(std::size_t bytesRead)
{
    using namespace std::placeholders;

    for (;;)
    {
        const auto stateBak = m_state;
        const size_t bytesParsed = parseReceivedBytes(bytesRead);
        m_receivedBytesLeft.clear();
        if (bytesParsed != (std::size_t)-1)
            m_receivedBytesLeft = m_responseBuffer.mid((int)bytesParsed);
        m_responseBuffer.resize(0);

        bool continueReceiving = false;
        switch (stateBak)
        {
            case State::sReceivingResponse:
            {
                if (processResponseHeadersBytes(&continueReceiving) == Result::thisDestroyed)
                    return;
                break;
            }

            case State::sReadingMessageBody:
            {
                if (processResponseMessageBodyBytes(bytesParsed, &continueReceiving) ==
                    Result::thisDestroyed)
                {
                    return;
                }
                break;
            }

            default:
            {
                NX_ASSERT(false);
                break;
            }
        }

        if (!continueReceiving)
            break;

        if (m_receivedBytesLeft.isEmpty())
        {
            NX_ASSERT(m_responseBuffer.size() == 0);
            if (!m_readingCeased)
            {
                m_readInvoked = true;
                m_socket->readSomeAsync(
                    &m_responseBuffer,
                    std::bind(&AsyncClient::onSomeBytesReadAsync, this, _1, _2));
            }
            return;
        }

        NX_CRITICAL(bytesParsed != 0 && bytesParsed != (std::size_t)-1);

        m_responseBuffer.swap(m_receivedBytesLeft);
        m_receivedBytesLeft.clear();

        m_responseBuffer.reserve(RESPONSE_BUFFER_SIZE);
        bytesRead = m_responseBuffer.size();
    }
}

void AsyncClient::stopReading()
{
    NX_ASSERT(isInSelfAioThread());
    m_readingCeased = true;
}

void AsyncClient::resumeReading()
{
    NX_ASSERT(isInSelfAioThread());
    NX_ASSERT(!m_readInvoked);
    if (m_readingCeased && !m_readInvoked)
    {
        using namespace std::placeholders;
        m_readInvoked = true;
        m_socket->readSomeAsync(
            &m_responseBuffer,
            std::bind(&AsyncClient::onSomeBytesReadAsync, this, _1, _2));
    }

    m_readingCeased = false;
}

bool AsyncClient::isReading()
{
    NX_ASSERT(isInSelfAioThread());
    return !m_readingCeased;
}

AsyncClient::Result AsyncClient::processResponseHeadersBytes(
    bool* const continueReceiving)
{
    using namespace std::placeholders;

    // TODO: #ak Reconnect in case of error.

    if (m_state == State::sFailed)
        return emitDone();

    // Connection could have already been closed by remote peer.

    if (m_httpStreamReader.currentMessageNumber() < m_awaitedMessageNumber ||   //< Still reading previous message.
        m_httpStreamReader.state() <= HttpStreamReader::readingMessageHeaders)  //< Still reading message headers.
    {
        // Response has not been read yet, reading futher.
        m_responseBuffer.resize(0);
        if (m_connectionClosed)
        {
            if (reconnectIfAppropriate())
                return Result::proceed;

            NX_DEBUG(this, "Failed to read (1) response from %1. %2",
                m_contentLocationUrl, SystemError::connectionReset);
            m_state = State::sFailed;
            return emitDone();
        }
        *continueReceiving = true;
        return Result::proceed;
    }

    // Read http message headers.
    if (m_httpStreamReader.message().type != nx::network::http::MessageType::response)
    {
        NX_DEBUG(this, lm("Unexpectedly received request from %1:%2 while expecting response! Ignoring...")
            .arg(m_contentLocationUrl.host()).arg(m_contentLocationUrl.port()));
        m_state = State::sFailed;
        return emitDone();
    }

    if (isMalformed(*m_httpStreamReader.message().response))
    {
        m_state = State::sFailed;
        return emitDone();
    }

    NX_VERBOSE(this, "Response header from %1 has been successfully read: %2",
        m_contentLocationUrl,
        logTraffic() ? response()->toString() : response()->statusLine.toString());

    if (repeatRequestIfNeeded(*m_httpStreamReader.message().response))
        return Result::proceed;

    m_state = State::sResponseReceived;
    const auto requestSequenceBak = m_requestSequence;
    if (emitResponseReceived() == Result::thisDestroyed)
        return Result::thisDestroyed;
    if (m_terminated ||
        (m_requestSequence != requestSequenceBak))  //< User started new request within responseReceived handler.
    {
        return Result::proceed;
    }

    const bool messageBodyAllowed = Method::isMessageBodyAllowedInResponse(
        m_request.requestLine.method,
        StatusCode::Value(m_httpStreamReader.message().response->statusLine.statusCode));

    const bool messageHasMessageBody =
        (m_httpStreamReader.state() == HttpStreamReader::readingMessageBody) ||
        (m_httpStreamReader.state() == HttpStreamReader::pullingLineEndingBeforeMessageBody) ||
        (m_httpStreamReader.messageBodyBufferSize() > 0);

    if (!messageHasMessageBody || !messageBodyAllowed)
    {
        // No message body: done.
        m_state = m_httpStreamReader.state() == HttpStreamReader::parseError
            ? State::sFailed
            : State::sDone;
        return emitDone();
    }

    return startReadingMessageBody(continueReceiving);
}

AsyncClient::Result AsyncClient::startReadingMessageBody(bool* const continueReceiving)
{
    // Starting reading message body.
    m_state = State::sReadingMessageBody;

    if (m_httpStreamReader.messageBodyBufferSize() > 0 &&   //< Some message body has been read.
        m_state == State::sReadingMessageBody)              //< Client wants to read message body.
    {
        if (emitSomeMessageBodyAvailable() == Result::thisDestroyed)
            return Result::thisDestroyed;
        if (m_terminated)
            return Result::proceed;
        if (m_forcedEof)
        {
            m_forcedEof = false;
            return Result::proceed;
        }
    }

    if (((m_httpStreamReader.state() == HttpStreamReader::readingMessageBody) ||
        (m_httpStreamReader.state() == HttpStreamReader::pullingLineEndingBeforeMessageBody)) &&
        (!m_connectionClosed))
    {
        // Reading more data.
        m_responseBuffer.resize(0);
        if (!m_socket->setRecvTimeout(m_msgBodyReadTimeout))
        {
            NX_DEBUG(this, "Failed to read (1) response body from %1. %2",
                m_contentLocationUrl, SystemError::getLastOSErrorText());

            m_state = State::sFailed;
            return emitDone();
        }
        *continueReceiving = true;
        return Result::proceed;
    }

    // Message body has been received with request.
    NX_ASSERT(
        m_httpStreamReader.state() == HttpStreamReader::messageDone ||
        m_httpStreamReader.state() == HttpStreamReader::parseError);

    m_state = m_httpStreamReader.state() == HttpStreamReader::parseError ? State::sFailed : State::sDone;

    NX_VERBOSE(this, "Finished reading body from %1", m_contentLocationUrl);
    return emitDone();
}

bool AsyncClient::isMalformed(const nx::network::http::Response& response) const
{
    if (response.statusLine.statusCode == StatusCode::switchingProtocols)
    {
        if (nx::network::http::getHeaderValue(response.headers, "Upgrade").isEmpty())
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

            if (!m_authorizationTried && (!m_user.username.isEmpty() || !m_user.authToken.empty()))
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
                (!m_proxyUser.username.isEmpty() || !m_proxyUser.authToken.empty()))
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

    NX_VERBOSE(this, lm("Redirect to location [ %1 ] from [ %2 ]").args(
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

AsyncClient::Result AsyncClient::processResponseMessageBodyBytes(
    std::size_t bytesRead,
    bool* const continueReceiving)
{
    using namespace std::placeholders;

    // TODO: #ak Reconnect in case of error.
    if (bytesRead != (std::size_t)-1)
    {
        if (emitSomeMessageBodyAvailable() == Result::thisDestroyed)
            return Result::thisDestroyed;
        if (m_terminated)
            return Result::proceed;
        if (m_forcedEof)
        {
            m_forcedEof = false;
            return Result::proceed;
        }
    }

    if (m_state != State::sFailed && m_state != State::sDone)
    {
        m_responseBuffer.resize(0);
        *continueReceiving = true;
        return Result::proceed;
    }

    return emitDone();
}

void AsyncClient::composeRequest(const nx::network::http::StringType& httpMethod)
{
    const bool useHttp11 = true;   //TODO #ak check if we need it (e.g. we using keep-alive or requesting live capture)

    prepareRequestLine(useHttp11, httpMethod);

    // Adding user credentials.
    if (!m_contentLocationUrl.userName().isEmpty())
        m_user.username = m_contentLocationUrl.userName();
    else
        m_contentLocationUrl.setUserName(m_user.username);

    if (!m_contentLocationUrl.password().isEmpty())
        m_user.authToken.setPassword(m_contentLocationUrl.password().toUtf8());
    else
        m_contentLocationUrl.setPassword(m_user.authToken.value);

    prepareRequestHeaders(useHttp11, httpMethod);
}

void AsyncClient::prepareRequestLine(bool useHttp11, const nx::network::http::StringType& httpMethod)
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

void AsyncClient::prepareRequestHeaders(bool useHttp11, const nx::network::http::StringType& httpMethod)
{
    nx::network::http::insertOrReplaceHeader(
        &m_request.headers,
        HttpHeader("Date", nx::network::http::formatDateTime(QDateTime::currentDateTime())));
    nx::network::http::insertOrReplaceHeader(
        &m_request.headers,
        HttpHeader(
            "User-Agent",
            m_userAgent.isEmpty() ? nx::network::http::userAgentString() : m_userAgent.toLatin1()));
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
    // Adding X-Nx-User-Name to help server to port data from 2.1 to 2.3 and to 3.0 (generate user's digest after changing realm).
    // TODO: #ak Remove it after version prior to 3.0 support is over.
    if (!m_user.username.isEmpty() &&
        m_request.headers.find(Qn::CUSTOM_USERNAME_HEADER_NAME) == m_request.headers.end())
    {
        nx::network::http::insertOrReplaceHeader(
            &m_request.headers,
            HttpHeader(Qn::CUSTOM_USERNAME_HEADER_NAME, m_user.username.toUtf8()));
    }

    if (m_precalculatedAuthorizationDisabled)
        return;

    // If that url has already been authenticated, adding same authentication info to the request
    // First request per tcp-connection always uses authentication
    //    This is done due to limited AuthInfoCache implementation
    if (m_authCacheItem.url.isEmpty() ||
        !AuthInfoCache::instance()->addAuthorizationHeader(
            m_contentLocationUrl,
            &m_request,
            &m_authCacheItem))
    {
        if (m_authType == AuthType::authBasic && m_user.authToken.type == AuthTokenType::password)
        {
            header::BasicAuthorization basicAuthorization(
                m_user.username.toUtf8(),
                m_user.authToken.value);
            nx::network::http::insertOrReplaceHeader(
                &m_request.headers,
                nx::network::http::HttpHeader(
                    header::Authorization::NAME,
                    basicAuthorization.serialized()));
        }
        else
        {
            // Not using Basic authentication by default, since it is not secure.
            nx::network::http::removeHeader(&m_request.headers, header::Authorization::NAME);
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
            StringType::number((qulonglong)*m_requestBody->contentLength()));
    }
    // TODO: #ak Support chunked encoding & compression.
    m_request.headers.insert(make_pair("Content-Encoding", "identity"));

    // TODO: #ak Add support for any body.
    NX_CRITICAL(
        dynamic_cast<AbstractMsgBodySource*>(m_requestBody.get()) != nullptr,
        "Only fixed request body supported at the moment");
    auto contentLength = m_requestBody->contentLength();
    m_requestBody->readAsync(
        [this](SystemError::ErrorCode /*sysErrorCode*/, nx::network::http::BufferType buffer)
        {
            m_request.messageBody = std::move(buffer);
        });
    if (contentLength)
    {
        NX_ASSERT(static_cast<std::size_t>(m_request.messageBody.size()) == *contentLength);
    }
}

void AsyncClient::addAdditionalHeader(const StringType& key, const StringType& value)
{
    m_additionalHeaders.emplace(key, value);
}

void AsyncClient::removeAdditionalHeader(const StringType& key)
{
    m_additionalHeaders.erase(key);
}

void AsyncClient::setAdditionalHeaders(HttpHeaders additionalHeaders)
{
    m_additionalHeaders = std::move(additionalHeaders);
}

void AsyncClient::addRequestHeaders(const HttpHeaders& headers)
{
    for (HttpHeaders::const_iterator itr = headers.begin(); itr != headers.end(); ++itr)
        m_additionalHeaders.emplace(itr->first, itr->second);
}

void AsyncClient::serializeRequest()
{
    m_requestBuffer.clear();
    m_request.serialize(&m_requestBuffer);
    m_requestBytesSent = 0;
}

bool AsyncClient::reconnectIfAppropriate()
{
    if ((m_connectionClosed || m_state == State::sSendingRequest || m_state == State::sReceivingResponse) &&
        m_totalBytesReadPerRequest == 0 &&
        m_totalRequestsSentViaCurrentConnection > 1)
    {
        //< Reconnect if TCP timeout for keep-alive connections
        m_connectionClosed = true;
        initiateHttpMessageDelivery();
        return true;
    }

    // TODO #ak: We need reconnect and request entity from the point we stopped at.
    return false;
}

QString AsyncClient::endpointWithProtocol(const nx::utils::Url& url)
{
    return lm("%1://%2:%3")
        .arg(url.scheme())
        .arg(url.host())
        .arg(url.port(nx::network::http::defaultPortForScheme(url.scheme().toLatin1())));
}

static std::optional<header::WWWAuthenticate> extractAuthenticateHeader(
    const HttpHeaders& headers, bool isProxy, AuthType authType)
{
    const StringType authenticateHeaderName = isProxy ? "Proxy-Authenticate" : "WWW-Authenticate";

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

    const StringType authorizationHeaderName =
        isProxy ? StringType("Proxy-Authorization") : header::Authorization::NAME;
    const auto credentials = isProxy ? m_proxyUser : m_user;

    auto wwwAuthenticateHeader = extractAuthenticateHeader(response.headers, isProxy, m_authType);
    if(!wwwAuthenticateHeader)
        return false;

    // TODO: #ak MUST add to cache only after OK response.
    m_authCacheItem = AuthInfoCache::Item(
        m_contentLocationUrl,
        m_request.requestLine.method,
        credentials,
        std::move(*wwwAuthenticateHeader));
    AuthInfoCache::instance()->cacheAuthorization(m_authCacheItem);
    if (!AuthInfoCache::instance()->addAuthorizationHeader(
            m_contentLocationUrl,
            &m_request,
            &m_authCacheItem))
    {
        return false;
    }

    doSomeCustomLogic(response, &m_request);

    if (isProxy)
        m_proxyAuthorizationTried = true;
    else
        m_authorizationTried = true;
    initiateHttpMessageDelivery();
    return true;
}

void AsyncClient::doSomeCustomLogic(
    const nx::network::http::Response& response,
    Request* const request)
{
    // TODO: #ak This method is not part of http, so it should not be in this class.

    if (m_user.authToken.type != AuthTokenType::password)
        return;

    auto realmIter = response.headers.find(Qn::REALM_HEADER_NAME);
    if (realmIter == response.headers.end())
        return;

    // Calculating user's digest with new realm.
    const auto newRealmDigest = calcHa1(
        m_user.username.toUtf8(),
        realmIter->second,
        m_user.authToken.value);
    const auto cryptSha512Hash = linuxCryptSha512(
        m_user.authToken.value,
        generateSalt(LINUX_CRYPT_SALT_LENGTH));

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

AuthInfoCache::Item AsyncClient::authCacheItem() const
{
    return m_authCacheItem;
}

void AsyncClient::forceEndOfMsgBody()
{
    m_forcedEof = true;
    m_httpStreamReader.forceEndOfMsgBody();
}

void AsyncClient::setExpectOnlyMessageBodyWithoutHeaders(bool expectOnlyBody)
{
    m_expectOnlyBody = expectOnlyBody;
}

AsyncClient::Result AsyncClient::emitDone()
{
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

    nx::utils::ObjectDestructionFlag::Watcher objectDestructionWatcher(&m_objectDestructionFlag);
    handler(args...);
    return objectDestructionWatcher.objectDestroyed()
        ? Result::thisDestroyed
        : Result::proceed;
}

} // namespace nx
} // namespace network
} // namespace http

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::network::http, AuthType,
    (nx::network::http::AuthType::authBasicAndDigest, "authBasicAndDigest")
    (nx::network::http::AuthType::authDigest, "authDigest")
    (nx::network::http::AuthType::authBasic, "authBasic")
)
