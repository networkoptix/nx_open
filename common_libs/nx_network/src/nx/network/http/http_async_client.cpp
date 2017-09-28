#include "http_async_client.h"

#include <QtCore/QCryptographicHash>
#include <QtCore/QDateTime>

#include <nx/network/buffered_stream_socket.h>
#include <nx/network/socket_factory.h>
#include <nx/network/socket_global.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/utils/log/log.h>
#include <nx/utils/thread/mutex.h>

#include <nx/utils/crypt/linux_passwd_crypt.h>
#include <nx/utils/system_error.h>

#include "auth_tools.h"
#include "buffer_source.h"
#include "custom_headers.h"

using std::make_pair;

namespace {

static bool logTraffic()
{
    return nx::network::SocketGlobals::debugIni().httpClientTraffic;
}

} // namespace

namespace nx_http {

static const size_t RESPONSE_BUFFER_SIZE = 16 * 1024;

constexpr const std::chrono::seconds AsyncClient::Timeouts::kDefaultSendTimeout;
constexpr const std::chrono::seconds AsyncClient::Timeouts::kDefaultResponseReadTimeout;
constexpr const std::chrono::seconds AsyncClient::Timeouts::kDefaultMessageBodyReadTimeout;

constexpr int kMaxNumberOfRedirects = 5;

AsyncClient::Timeouts::Timeouts(
    std::chrono::milliseconds send,
    std::chrono::milliseconds recv,
    std::chrono::milliseconds msgBody)
:
    sendTimeout(send),
    responseReadTimeout(recv),
    messageBodyReadTimeout(msgBody)
{
}


bool AsyncClient::Timeouts::operator==(const Timeouts& rhs) const
{
    return sendTimeout == rhs.sendTimeout
        && responseReadTimeout == rhs.responseReadTimeout
        && messageBodyReadTimeout == rhs.messageBodyReadTimeout;
}

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
        auto bufferedStreamSocket =
            std::make_unique<nx::network::BufferedStreamSocket>(std::move(result));
        BufferType buf;
        buf.swap(m_receivedBytesLeft);
        bufferedStreamSocket->injectRecvData(std::move(buf));
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
    return m_state == State::sFailed || response() == nullptr;
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

void AsyncClient::doGet(const QUrl& url)
{
    NX_ASSERT(!url.host().isEmpty());
    NX_ASSERT(url.isValid());

    resetDataBeforeNewRequest();
    m_requestUrl = url;
    m_contentLocationUrl = url;
    composeRequest(nx_http::Method::get);
    initiateHttpMessageDelivery();
}

void AsyncClient::doGet(
    const QUrl& url,
    nx::utils::MoveOnlyFunc<void()> completionHandler)
{
    m_onDone = std::move(completionHandler);
    doGet(url);
}

void AsyncClient::doPost(const QUrl& url)
{
    NX_ASSERT(url.isValid());

    resetDataBeforeNewRequest();
    m_requestUrl = url;
    m_contentLocationUrl = url;
    composeRequest(nx_http::Method::post);
    addBodyToRequest();

    initiateHttpMessageDelivery();
}

void AsyncClient::doPost(
    const QUrl& url,
    nx::utils::MoveOnlyFunc<void()> completionHandler)
{
    m_onDone = std::move(completionHandler);
    doPost(url);
}

void AsyncClient::doPut(const QUrl& url)
{
    NX_ASSERT(url.isValid());

    resetDataBeforeNewRequest();
    m_requestUrl = url;
    m_contentLocationUrl = url;
    composeRequest(nx_http::Method::put);
    addBodyToRequest();

    initiateHttpMessageDelivery();
}

void AsyncClient::doPut(
    const QUrl& url,
    nx::utils::MoveOnlyFunc<void()> completionHandler)
{
    m_onDone = std::move(completionHandler);
    doPut(url);
}

void AsyncClient::doDelete(const QUrl& url)
{
    NX_ASSERT(!url.host().isEmpty());
    NX_ASSERT(url.isValid());

    resetDataBeforeNewRequest();
    m_requestUrl = url;
    m_contentLocationUrl = url;
    composeRequest(nx_http::Method::delete_);
    initiateHttpMessageDelivery();
}

void AsyncClient::doDelete(
    const QUrl& url,
    nx::utils::MoveOnlyFunc<void()> completionHandler)
{
    m_onDone = std::move(completionHandler);
    doDelete(url);
}

void AsyncClient::doUpgrade(
    const QUrl& url,
    const StringType& protocolToUpgradeTo,
    nx::utils::MoveOnlyFunc<void()> completionHandler)
{
    doUpgrade(
        url,
        nx_http::Method::options,
        protocolToUpgradeTo,
        std::move(completionHandler));
}

void AsyncClient::doUpgrade(
    const QUrl& url,
    nx_http::Method::ValueType method,
    const StringType& protocolToUpgradeTo,
    nx::utils::MoveOnlyFunc<void()> completionHandler)
{
    m_onDone = std::move(completionHandler);

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

const nx_http::Request& AsyncClient::request() const
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
        NX_LOGX(lm("Response message body buffer:\n%1\n\n").arg(buffer), cl_logDEBUG2);

    return buffer;
}

const QUrl& AsyncClient::url() const
{
    return m_requestUrl;
}

const QUrl& AsyncClient::contentLocationUrl() const
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
    m_userName = userName;
}

void AsyncClient::setUserPassword(const QString& userPassword)
{
    m_userPassword = userPassword;
}

void AsyncClient::setProxyUserName(const QString& userName)
{
    m_proxyUserName = userName;
}

void AsyncClient::setProxyUserPassword(const QString& userPassword)
{
    m_proxyUserPassword = userPassword;
}

void AsyncClient::setAuth(const AuthInfo& auth)
{
    setUserName(auth.username);
    setUserPassword(auth.password);
    setProxyUserName(auth.proxyUsername);
    setProxyUserPassword(auth.proxyPassword);
    setProxyVia(auth.proxyEndpoint);
}

void AsyncClient::setProxyVia(const SocketAddress& proxyEndpoint)
{
    if (proxyEndpoint.isNull())
    {
        m_proxyEndpoint.reset();
    }
    else
    {
        NX_ASSERT(proxyEndpoint.port > 0);
        m_proxyEndpoint = proxyEndpoint;
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

void AsyncClient::stopWhileInAioThread()
{
    m_socket.reset();
    m_requestBody.reset();
}

void AsyncClient::asyncConnectDone(SystemError::ErrorCode errorCode)
{
    NX_LOGX(lm("Opened connection to url %1. Result code %2")
        .arg(m_contentLocationUrl).arg(errorCode), cl_logDEBUG2);

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
        NX_LOGX(lm("Sending request to url %1").arg(m_contentLocationUrl), cl_logDEBUG2);

        m_socket->sendAsync(m_requestBuffer, std::bind(&AsyncClient::asyncSendDone, this, _1, _2));
        return;
    }

    NX_LOGX(lm("Failed to establish tcp connection to %1. %2")
        .arg(m_contentLocationUrl).arg(SystemError::toString(errorCode)), cl_logDEBUG1);
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
        NX_LOGX(lm("Error sending (1) http request to %1. %2")
            .arg(m_contentLocationUrl).arg(SystemError::toString(errorCode)), cl_logDEBUG1);
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

    NX_LOGX(lm("Request has been successfully sent to %1: %2")
        .arg(m_contentLocationUrl)
        .args(logTraffic() ? request().toString() : request().requestLine.toString()),
        cl_logDEBUG2);

    const auto requestSequenceBak = m_requestSequence;
    if (emitRequestHasBeenSent(m_authorizationTried) == Result::thisDestroyed)
        return;
    if (m_terminated || //user cancelled futher action
        (m_requestSequence != requestSequenceBak))  //user started new request within responseReceived handler
    {
        return;
    }

    m_state = m_expectOnlyBody ? State::sReadingMessageBody : State::sReceivingResponse;
    m_responseBuffer.resize(0);
    if (!m_socket->setRecvTimeout(m_responseReadTimeout))
    {
        if (reconnectIfAppropriate())
            return;

        const auto sysErrorCode = SystemError::getLastOSErrorCode();
        NX_LOGX(lm("Url %1. Error setting receive timeout to %2 ms. %3")
            .arg(m_contentLocationUrl).arg(m_responseReadTimeout)
            .arg(SystemError::toString(sysErrorCode)),
            cl_logDEBUG1);
        m_state = State::sFailed;
        const auto requestSequenceBak = m_requestSequence;
        if (emitDone() == Result::thisDestroyed)
            return;
        if (m_requestSequence == requestSequenceBak)
            m_socket.reset();   //< Closing failed socket so that it is not reused.
        return;
    }

    m_socket->readSomeAsync(
        &m_responseBuffer,
        std::bind(&AsyncClient::onSomeBytesReadAsync, this, _1, _2));
}

void AsyncClient::onSomeBytesReadAsync(
    SystemError::ErrorCode errorCode,
    size_t bytesRead)
{
    using namespace std::placeholders;

    if (m_terminated)
        return;

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

        NX_LOGX(lm("Error reading (state %1) http response from %2. %3")
            .arg(toString(stateBak)).arg(m_contentLocationUrl).arg(SystemError::toString(errorCode)),
            cl_logDEBUG1);
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
    m_request = nx_http::Request();
}

void AsyncClient::initiateHttpMessageDelivery()
{
    using namespace std::placeholders;
    bool canUseExistingConnection = false;
    if (m_httpStreamReader.message().type == nx_http::MessageType::response)
    {
        canUseExistingConnection =
            (m_httpStreamReader.message().response->statusLine.version == nx_http::http_1_1) &&
            (nx_http::getHeaderValue(m_httpStreamReader.message().response->headers, "Connection") != "close");
    }

    canUseExistingConnection =
        m_socket &&
        !m_connectionClosed &&
        canUseExistingConnection &&
        (m_remoteEndpointWithProtocol == endpointWithProtocol(m_contentLocationUrl)) &&
        m_lastSysErrorCode == SystemError::noError;

    if (!canUseExistingConnection)
    {
        m_httpStreamReader.resetState();
        m_awaitedMessageNumber = 0;
        m_lastSysErrorCode = SystemError::noError;
        m_totalRequestsSentViaCurrentConnection = 0;
    }
    ++m_totalRequestsSentViaCurrentConnection;
    m_totalBytesReadPerRequest = 0;

    m_state = State::sInit;

    dispatch(
        [this, canUseExistingConnection]()
        {
            if (canUseExistingConnection)
            {
                NX_ASSERT(m_socket);
                m_socket->cancelIOSync(nx::network::aio::etNone);
                ++m_awaitedMessageNumber;   //current message will be skipped

                serializeRequest();
                m_state = State::sSendingRequest;
                NX_LOGX(lm("Sending request to url %1").arg(m_contentLocationUrl), cl_logDEBUG2);
                m_socket->sendAsync(
                    m_requestBuffer,
                    std::bind(&AsyncClient::asyncSendDone, this, _1, _2));
                return;
            }

            m_socket.reset();

            initiateTcpConnection();
        });
}

void AsyncClient::initiateTcpConnection()
{
    const SocketAddress remoteAddress =
        m_proxyEndpoint
        ? m_proxyEndpoint.get()
        : SocketAddress(
            m_contentLocationUrl.host(),
            m_contentLocationUrl.port(nx_http::defaultPortForScheme(m_contentLocationUrl.scheme().toLatin1())));

    m_state = State::sInit;

    m_socket = SocketFactory::createStreamSocket(m_contentLocationUrl.scheme() == lm("https"));

    NX_LOGX(lm("Opening connection to %1. url %2, socket %3")
        .arg(remoteAddress).arg(m_contentLocationUrl).arg(m_socket->handle()), cl_logDEBUG2);

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

    //starting async connect
    m_socket->connectAsync(
        remoteAddress,
        std::bind(&AsyncClient::asyncConnectDone, this, std::placeholders::_1));
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
        NX_LOGX(lm("Error parsing http response from %1. %2")
            .arg(m_contentLocationUrl).arg(m_httpStreamReader.errorText()), cl_logDEBUG1);
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
            m_socket->readSomeAsync(
                &m_responseBuffer,
                std::bind(&AsyncClient::onSomeBytesReadAsync, this, _1, _2));
            return;
        }

        NX_CRITICAL(bytesParsed != 0 && bytesParsed != (std::size_t)-1);

        m_responseBuffer.swap(m_receivedBytesLeft);
        m_receivedBytesLeft.clear();

        m_responseBuffer.reserve(RESPONSE_BUFFER_SIZE);
        bytesRead = m_responseBuffer.size();
    }
}

AsyncClient::Result AsyncClient::processResponseHeadersBytes(
    bool* const continueReceiving)
{
    using namespace std::placeholders;

    //TODO/IMPL reconnect in case of error

    if (m_state == State::sFailed)
        return emitDone();

    // Connection could have already been closed by remote peer.

    if (m_httpStreamReader.currentMessageNumber() < m_awaitedMessageNumber ||       //still reading previous message
        m_httpStreamReader.state() <= HttpStreamReader::readingMessageHeaders)     //still reading message headers
    {
        //response has not been read yet, reading futher
        m_responseBuffer.resize(0);
        if (m_connectionClosed)
        {
            if (reconnectIfAppropriate())
                return Result::proceed;

            NX_LOGX(lm("Failed to read (1) response from %1. %2")
                .arg(m_contentLocationUrl).arg(SystemError::connectionReset), cl_logDEBUG1);
            m_state = State::sFailed;
            return emitDone();
        }
        *continueReceiving = true;
        return Result::proceed;
    }

    //read http message headers
    if (m_httpStreamReader.message().type != nx_http::MessageType::response)
    {
        NX_LOGX(lm("Unexpectedly received request from %1:%2 while expecting response! Ignoring...")
            .arg(m_contentLocationUrl.host()).arg(m_contentLocationUrl.port()), cl_logDEBUG1);
        m_state = State::sFailed;
        return emitDone();
    }

    if (isMalformed(*m_httpStreamReader.message().response))
    {
        m_state = State::sFailed;
        return emitDone();
    }

    NX_LOGX(lm("Response from %1 has been successfully read: %2")
        .arg(m_contentLocationUrl)
        .arg(logTraffic() ? response()->toString() : response()->statusLine.toString()),
        cl_logDEBUG2);

    if (repeatRequestIfNeeded(*m_httpStreamReader.message().response))
        return Result::proceed;

    const bool messageHasMessageBody =
        (m_httpStreamReader.state() == HttpStreamReader::readingMessageBody) ||
        (m_httpStreamReader.state() == HttpStreamReader::pullingLineEndingBeforeMessageBody) ||
        (m_httpStreamReader.messageBodyBufferSize() > 0);

    m_state = State::sResponseReceived;
    const auto requestSequenceBak = m_requestSequence;
    if (emitResponseReceived() == Result::thisDestroyed)
        return Result::thisDestroyed;
    if (m_terminated ||
        (m_requestSequence != requestSequenceBak))  //user started new request within responseReceived handler
    {
        return Result::proceed;
    }

    //does message body follow?
    if (!messageHasMessageBody)
    {
        //no message body: done
        m_state = m_httpStreamReader.state() == HttpStreamReader::parseError ? State::sFailed : State::sDone;
        return emitDone();
    }

    //starting reading message body
    m_state = State::sReadingMessageBody;

    if (m_httpStreamReader.messageBodyBufferSize() > 0 &&   //some message body has been read
        m_state == State::sReadingMessageBody)                    //client wants to read message body
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
        //reading more data
        m_responseBuffer.resize(0);
        if (!m_socket->setRecvTimeout(m_msgBodyReadTimeout))
        {
            NX_LOGX(lm("Failed to read (1) response from %1. %2")
                .arg(m_contentLocationUrl).arg(SystemError::getLastOSErrorText()),
                cl_logDEBUG1);

            m_state = State::sFailed;
            return emitDone();
        }
        *continueReceiving = true;
        return Result::proceed;
    }

    //message body has been received with request
    NX_ASSERT(
        m_httpStreamReader.state() == HttpStreamReader::messageDone ||
        m_httpStreamReader.state() == HttpStreamReader::parseError);

    m_state = m_httpStreamReader.state() == HttpStreamReader::parseError ? State::sFailed : State::sDone;
    return emitDone();
}

bool AsyncClient::isMalformed(const nx_http::Response& response) const
{
    if (response.statusLine.statusCode == StatusCode::switchingProtocols)
    {
        if (nx_http::getHeaderValue(response.headers, "Upgrade").isEmpty())
        {
            NX_LOGX(lm("Received malformed response from %1. "
                "Status code is %2 and no Upgrade header present")
                .arg(m_contentLocationUrl).arg(response.statusLine.statusCode),
                cl_logDEBUG1);
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

            if (!m_authorizationTried && (!m_userName.isEmpty() || !m_userPassword.isEmpty()))
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
                (!m_proxyUserName.isEmpty() || !m_proxyUserPassword.isEmpty()))
            {
                if (resendRequestWithAuthorization(response, true))
                    return true;
            }
            break;
        }

        case StatusCode::found:
        case StatusCode::movedPermanently:
            return sendRequestToNewLocation(response);

        default:
            break;
    }

    return false;
}

bool AsyncClient::sendRequestToNewLocation(const Response& response)
{
    if (m_numberOfRedirectsTried >= kMaxNumberOfRedirects)
        return false;
    ++m_numberOfRedirectsTried;

    // For now, using first Location if many have been provided in response.
    const auto locationIter = response.headers.find("Location");
    if (locationIter == response.headers.end())
        return false;

    m_authorizationTried = false;
    m_ha1RecalcTried = false;

    m_contentLocationUrl = QUrl(QLatin1String(locationIter->second));

    composeRequest(m_request.requestLine.method);
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

void AsyncClient::composeRequest(const nx_http::StringType& httpMethod)
{
    const bool useHttp11 = true;   //TODO #ak check if we need it (e.g. we using keep-alive or requesting live capture)

    m_request.requestLine.method = httpMethod;

    if (m_proxyEndpoint)
    {
        m_request.requestLine.url = m_contentLocationUrl;
    }
    else    //if no proxy specified then erasing http://host:port from request url
    {
        if (httpMethod == nx_http::Method::options && m_contentLocationUrl.path().isEmpty())
            m_request.requestLine.url = "*";
        else
            m_request.requestLine.url = m_contentLocationUrl.path();
        m_request.requestLine.url.setQuery(m_contentLocationUrl.query());
        m_request.requestLine.url.setFragment(m_contentLocationUrl.fragment());
    }
    m_request.requestLine.version = useHttp11 ? nx_http::http_1_1 : nx_http::http_1_0;

    nx_http::insertOrReplaceHeader(
        &m_request.headers,
        HttpHeader("Date", nx_http::formatDateTime(QDateTime::currentDateTime())));
    m_request.headers.emplace(
        "User-Agent",
        m_userAgent.isEmpty() ? nx_http::userAgentString() : m_userAgent.toLatin1());
    if (useHttp11)
    {
        if (httpMethod == nx_http::Method::get || httpMethod == nx_http::Method::head)
        {
            //m_request.headers.insert( std::make_pair("Accept", "*/*") );
            if (m_contentEncodingUsed)
                m_request.headers.insert(std::make_pair("Accept-Encoding", "gzip"));
            //else
            //    m_request.headers.insert( std::make_pair("Accept-Encoding", "identity;q=1.0, *;q=0") );
        }
        //m_request.headers.insert( std::make_pair("Cache-Control", "max-age=0") );

        if (m_additionalHeaders.count("Connection") == 0)
            m_request.headers.insert(std::make_pair("Connection", "keep-alive"));

        if (m_additionalHeaders.count("Host") == 0)
        {
            m_request.headers.emplace(
                "Host",
                nx::network::url::getEndpoint(m_contentLocationUrl).toString().toUtf8());
        }
    }

    m_request.headers.insert(m_additionalHeaders.cbegin(), m_additionalHeaders.cend());

    //adding user credentials
    if (!m_contentLocationUrl.userName().isEmpty())
        m_userName = m_contentLocationUrl.userName();
    if (!m_contentLocationUrl.password().isEmpty())
        m_userPassword = m_contentLocationUrl.password();
    m_contentLocationUrl.setUserName(m_userName);
    m_contentLocationUrl.setPassword(m_userPassword);

    //adding X-Nx-User-Name to help server to port data from 2.1 to 2.3 and from 2.3 to 2.4 (generate user's digest)
    //TODO #ak remove it after 2.3 support is over
    if (!m_userName.isEmpty() &&
        m_request.headers.find(Qn::CUSTOM_USERNAME_HEADER_NAME) == m_request.headers.end())
    {
        nx_http::insertOrReplaceHeader(
            &m_request.headers,
            HttpHeader(Qn::CUSTOM_USERNAME_HEADER_NAME, m_userName.toUtf8()));
    }

    if (m_precalculatedAuthorizationDisabled)
        return;

    //if that url has already been authenticated, adding same authentication info to the request
    //first request per tcp-connection always uses authentication
    //    This is done due to limited AuthInfoCache implementation
    if (m_authCacheItem.url.isEmpty() ||
        !AuthInfoCache::instance()->addAuthorizationHeader(
            m_contentLocationUrl,
            &m_request,
            &m_authCacheItem))
    {
        if (m_authType == AuthType::authBasic)
        {
            header::BasicAuthorization basicAuthorization(m_userName.toLatin1(), m_userPassword.toLatin1());
            nx_http::insertOrReplaceHeader(
                &m_request.headers,
                nx_http::HttpHeader(
                    header::Authorization::NAME,
                    basicAuthorization.serialized()));
        }
        else
        {
            // Not using Basic authentication by default, since it is not secure.
            nx_http::removeHeader(&m_request.headers, header::Authorization::NAME);
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
        dynamic_cast<BufferSource*>(m_requestBody.get()) != nullptr,
        "Only fixed request body supported at the moment");
    auto contentLength = m_requestBody->contentLength();
    m_requestBody->readAsync(
        [this](SystemError::ErrorCode /*sysErrorCode*/, nx_http::BufferType buffer)
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

QString AsyncClient::endpointWithProtocol(const QUrl& url)
{
    return lm("%1://%2:%3")
        .arg(url.scheme())
        .arg(url.host())
        .arg(url.port(nx_http::defaultPortForScheme(url.scheme().toLatin1())));
}

bool AsyncClient::resendRequestWithAuthorization(
    const nx_http::Response& response,
    bool isProxy)
{
    const StringType authenticateHeaderName = isProxy ? "Proxy-Authenticate" : "WWW-Authenticate";
    const StringType authorizationHeaderName = isProxy ? StringType("Proxy-Authorization") : header::Authorization::NAME;
    const QString userName = isProxy ? m_proxyUserName : m_userName;
    const QString userPassword = isProxy ? m_proxyUserPassword : m_userPassword;

    //if response contains WWW-Authenticate with Digest authentication, generating "Authorization: Digest" header and adding it to custom headers
    NX_ASSERT(response.statusLine.statusCode == StatusCode::unauthorized ||
                response.statusLine.statusCode == StatusCode::proxyAuthenticationRequired);

    HttpHeaders::const_iterator wwwAuthenticateIter = response.headers.find(authenticateHeaderName);
    if (wwwAuthenticateIter == response.headers.end())
        return false;

    header::WWWAuthenticate wwwAuthenticateHeader;
    wwwAuthenticateHeader.parse(wwwAuthenticateIter->second);
    if (wwwAuthenticateHeader.authScheme == header::AuthScheme::basic)
    {
        header::BasicAuthorization basicAuthorization(userName.toLatin1(), userPassword.toLatin1());
        nx_http::insertOrReplaceHeader(
            &m_request.headers,
            nx_http::HttpHeader(
                authorizationHeaderName,
                basicAuthorization.serialized()));
        //TODO #ak MUST add to cache only after OK response
        m_authCacheItem = AuthInfoCache::AuthorizationCacheItem(
            m_contentLocationUrl,
            m_request.requestLine.method,
            userName.toLatin1(),
            userPassword.toLatin1(),
            boost::optional<BufferType>(),
            std::move(wwwAuthenticateHeader),
            std::move(basicAuthorization));
        AuthInfoCache::instance()->cacheAuthorization(m_authCacheItem);
    }
    else if (wwwAuthenticateHeader.authScheme == header::AuthScheme::digest)
    {
        header::DigestAuthorization digestAuthorizationHeader;
        if (!calcDigestResponse(
                m_request.requestLine.method,
                userName.toUtf8(),
                m_authType != AuthType::authDigestWithPasswordHash
                    ? userPassword.toUtf8()
                    : boost::optional<nx_http::BufferType>(),
                m_authType == AuthType::authDigestWithPasswordHash
                    ? userPassword.toLatin1()
                    : boost::optional<nx_http::BufferType>(),
                m_contentLocationUrl.toString(QUrl::RemoveScheme | QUrl::RemovePort | QUrl::RemoveAuthority).toUtf8(),
                wwwAuthenticateHeader,
                &digestAuthorizationHeader))
        {
            return false;
        }
        BufferType authorizationStr;
        digestAuthorizationHeader.serialize(&authorizationStr);

        nx_http::insertOrReplaceHeader(
            &m_request.headers,
            nx_http::HttpHeader(authorizationHeaderName, authorizationStr));
        //TODO #ak MUST add to cache only after OK response
        m_authCacheItem = AuthInfoCache::AuthorizationCacheItem(
            m_contentLocationUrl,
            m_request.requestLine.method,
            userName.toLatin1(),
            m_authType == AuthType::authDigestWithPasswordHash
                ? boost::optional<BufferType>()
                : boost::optional<BufferType>(userPassword.toLatin1()),
            m_authType == AuthType::authDigestWithPasswordHash
                ? boost::optional<BufferType>(userPassword.toLatin1())
                : boost::optional<BufferType>(),
            std::move(wwwAuthenticateHeader),
            std::move(digestAuthorizationHeader));
        AuthInfoCache::instance()->cacheAuthorization(m_authCacheItem);
    }
    else
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
    const nx_http::Response& response,
    Request* const request)
{
    //TODO #ak this method is not part of http, so it should not be in this class

    if (m_authType == AuthType::authDigestWithPasswordHash)
        return;

    auto realmIter = response.headers.find(Qn::REALM_HEADER_NAME);
    if (realmIter == response.headers.end())
        return;

    //calculating user's digest with new realm
    const auto newRealmDigest = calcHa1(m_userName.toUtf8(), realmIter->second, m_userPassword.toUtf8());
    const auto cryptSha512Hash = linuxCryptSha512(
        m_userPassword.toUtf8(),
        generateSalt(LINUX_CRYPT_SALT_LENGTH));

    nx_http::insertOrReplaceHeader(
        &request->headers,
        HttpHeader(Qn::HA1_DIGEST_HEADER_NAME, newRealmDigest));
    nx_http::insertOrReplaceHeader(
        &request->headers,
        HttpHeader(Qn::CRYPT_SHA512_HASH_HEADER_NAME, cryptSha512Hash));
    nx_http::insertOrReplaceHeader(
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

AuthInfoCache::AuthorizationCacheItem AsyncClient::authCacheItem() const
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

} // namespace nx_http
