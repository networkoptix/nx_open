#include "asynchttpclient.h"

#include <condition_variable>
#include <mutex>

#include <QtCore/QCryptographicHash>
#include <QtCore/QDateTime>

#include <nx/network/url/url_parse_helper.h>
#include <nx/network/socket_factory.h>
#include <nx/network/socket_global.h>
#include <nx/utils/log/log.h>
#include <nx/utils/thread/mutex.h>

#include <nx/utils/crypt/linux_passwd_crypt.h>
#include <nx/utils/system_error.h>

#include "auth_tools.h"
#include "custom_headers.h"

static const int DEFAULT_SEND_TIMEOUT = 3000;
static const int DEFAULT_RESPONSE_READ_TIMEOUT = 3000;

using std::make_pair;

namespace {

static bool logTraffic()
{
    return nx::network::SocketGlobals::debugIni().httpClientTraffic;
}

} // namespace

namespace nx_http {

static const size_t RESPONSE_BUFFER_SIZE = 16 * 1024;

constexpr const std::chrono::seconds AsyncHttpClient::Timeouts::kDefaultSendTimeout;
constexpr const std::chrono::seconds AsyncHttpClient::Timeouts::kDefaultResponseReadTimeout;
constexpr const std::chrono::seconds AsyncHttpClient::Timeouts::kDefaultMessageBodyReadTimeout;
    
constexpr int kMaxNumberOfRedirects = 5;

AsyncHttpClient::Timeouts::Timeouts(
    std::chrono::milliseconds send,
    std::chrono::milliseconds recv,
    std::chrono::milliseconds msgBody)
:
    sendTimeout(send),
    responseReadTimeout(recv),
    messageBodyReadTimeout(msgBody)
{
}


bool AsyncHttpClient::Timeouts::operator==(const Timeouts& rhs) const
{
    return sendTimeout == rhs.sendTimeout
        && responseReadTimeout == rhs.responseReadTimeout
        && messageBodyReadTimeout == rhs.messageBodyReadTimeout;
}

AsyncHttpClient::AsyncHttpClient():
    m_state(sInit),
    m_connectionClosed(false),
    m_requestBytesSent(0),
    m_authorizationTried(false),
    m_proxyAuthorizationTried(false),
    m_ha1RecalcTried(false),
    m_terminated(false),
    m_bytesRead(0),
    m_totalRequests(0),
    m_contentEncodingUsed(true),
    m_sendTimeoutMs(DEFAULT_SEND_TIMEOUT),
    m_responseReadTimeoutMs(DEFAULT_RESPONSE_READ_TIMEOUT),
    m_msgBodyReadTimeoutMs(0),
    m_authType(authBasicAndDigest),
    m_awaitedMessageNumber(0),
    m_lastSysErrorCode(SystemError::noError),
    m_requestSequence(0),
    m_forcedEof(false),
    m_precalculatedAuthorizationDisabled(false),
    m_numberOfRedirectsTried(0),
    m_expectOnlyBody(false)
{
    m_responseBuffer.reserve(RESPONSE_BUFFER_SIZE);
}

AsyncHttpClient::~AsyncHttpClient()
{
    pleaseStopSync(false);
}

const std::unique_ptr<AbstractStreamSocket>& AsyncHttpClient::socket()
{
    return m_socket;
}

std::unique_ptr<AbstractStreamSocket> AsyncHttpClient::takeSocket()
{
    NX_ASSERT(m_aioThreadBinder.isInSelfAioThread());

    m_terminated = true;

    std::unique_ptr<AbstractStreamSocket> result;
    result.swap(m_socket);
    if (result)
        result->cancelIOSync(nx::network::aio::etNone);
    return result;
}

//TODO #ak move pleaseStop and pleaseStopSync to some common base class

void AsyncHttpClient::pleaseStop(nx::utils::MoveOnlyFunc<void()> completionHandler)
{
    m_aioThreadBinder.post(
        [this, completionHandler = std::move(completionHandler)]() mutable
    {
        stopWhileInAioThread();
        completionHandler();
    });
}

void AsyncHttpClient::pleaseStopSync(bool checkForLocks)
{
    if (m_aioThreadBinder.isInSelfAioThread())
    {
        stopWhileInAioThread();
    }
    else
    {
        NX_ASSERT(!nx::network::SocketGlobals::aioService().isInAnyAioThread());
        QnStoppableAsync::pleaseStopSync(checkForLocks);
    }
}

void AsyncHttpClient::stopWhileInAioThread()
{
    m_terminated = true;
    if (m_socket)
        m_socket->pleaseStopSync(false);
}

nx::network::aio::AbstractAioThread* AsyncHttpClient::getAioThread() const
{
    return m_aioThreadBinder.getAioThread();
}

void AsyncHttpClient::bindToAioThread(nx::network::aio::AbstractAioThread* aioThread)
{
    m_aioThreadBinder.bindToAioThread(aioThread);
    if (m_socket)
        m_socket->bindToAioThread(aioThread);
}

void AsyncHttpClient::post(nx::utils::MoveOnlyFunc<void()> func)
{
    m_aioThreadBinder.post(std::move(func));
}

void AsyncHttpClient::dispatch(nx::utils::MoveOnlyFunc<void()> func)
{
    m_aioThreadBinder.dispatch(std::move(func));
}

AsyncHttpClient::State AsyncHttpClient::state() const
{
    return m_state;
}

bool AsyncHttpClient::failed() const
{
    return m_state == sFailed || response() == nullptr;
}

SystemError::ErrorCode AsyncHttpClient::lastSysErrorCode() const
{
    if (m_lastSysErrorCode != SystemError::noError)
        return m_lastSysErrorCode;
    // Ensuring system error code is always non-zero in case of failure 
    //  to simplify AsyncHttpClient user's life.
    return failed() ? SystemError::connectionReset : SystemError::noError;
}

void AsyncHttpClient::doGet(const QUrl& url)
{
    NX_ASSERT(!url.host().isEmpty());
    NX_ASSERT(url.isValid());

    resetDataBeforeNewRequest();
    m_requestUrl = url;
    m_contentLocationUrl = url;
    composeRequest(nx_http::Method::get);
    initiateHttpMessageDelivery();
}

void AsyncHttpClient::doGet(
    const QUrl& url,
    nx::utils::MoveOnlyFunc<void(AsyncHttpClientPtr)> completionHandler)
{
    doHttpOperation<const QUrl&>(
        std::move(completionHandler),
        static_cast<void(AsyncHttpClient::*)(const QUrl&)>(&AsyncHttpClient::doGet),
        url);
}

void AsyncHttpClient::doPost(
    const QUrl& url,
    const nx_http::StringType& contentType,
    nx_http::StringType messageBody,
    bool includeContentLength)
{
    NX_ASSERT(url.isValid());

    resetDataBeforeNewRequest();
    m_requestUrl = url;
    m_contentLocationUrl = url;
    composeRequest(nx_http::Method::post);
    if (!contentType.isEmpty())
        m_request.headers.insert(make_pair("Content-Type", contentType));
    if (includeContentLength)
        m_request.headers.insert(make_pair("Content-Length", StringType::number(messageBody.size())));
    //TODO #ak support chunked encoding & compression
    m_request.headers.insert(make_pair("Content-Encoding", "identity"));
    m_request.messageBody = std::move(messageBody);
    initiateHttpMessageDelivery();
}

void AsyncHttpClient::doPost(
    const QUrl& url,
    const nx_http::StringType& contentType,
    nx_http::StringType messageBody,
    bool includeContentLength,
    nx::utils::MoveOnlyFunc<void(AsyncHttpClientPtr)> completionHandler)
{
    typedef void(AsyncHttpClient::*FuncToCallType)(
        const QUrl& /*url*/,
        const nx_http::StringType& /*contentType*/,
        nx_http::StringType /*messageBody*/,
        bool /*includeContentLength*/);

    doHttpOperation<const QUrl&, const nx_http::StringType&, nx_http::StringType, bool>(
        std::move(completionHandler),
        static_cast<FuncToCallType>(&AsyncHttpClient::doPost),
        url,
        contentType,
        std::move(messageBody),
        includeContentLength);
}

void AsyncHttpClient::doPut(
    const QUrl& url,
    const nx_http::StringType& contentType,
    nx_http::StringType messageBody)
{
    NX_ASSERT(url.isValid());

    resetDataBeforeNewRequest();
    m_requestUrl = url;
    m_contentLocationUrl = url;
    composeRequest(nx_http::Method::put);
    m_request.headers.insert(make_pair("Content-Type", contentType));
    m_request.headers.insert(make_pair("Content-Length", StringType::number(messageBody.size())));
    //TODO #ak support chunked encoding & compression
    m_request.messageBody = std::move(messageBody);
    initiateHttpMessageDelivery();
}

void AsyncHttpClient::doPut(
    const QUrl& url,
    const nx_http::StringType& contentType,
    nx_http::StringType messageBody,
    nx::utils::MoveOnlyFunc<void(AsyncHttpClientPtr)> completionHandler)
{
    typedef void(AsyncHttpClient::*FuncToCallType)(
        const QUrl& /*url*/,
        const nx_http::StringType& /*contentType*/,
        nx_http::StringType /*messageBody*/);

    doHttpOperation<const QUrl&, const nx_http::StringType&, nx_http::StringType>(
        std::move(completionHandler),
        static_cast<FuncToCallType>(&AsyncHttpClient::doPut),
        url,
        contentType,
                std::move(messageBody));
}

void AsyncHttpClient::doDelete(const QUrl& url)
{
    NX_ASSERT(url.isValid());

    resetDataBeforeNewRequest();
    m_requestUrl = url;
    m_contentLocationUrl = url;
    m_contentLocationUrl.setPath(QLatin1String("*"));
    composeRequest(nx_http::Method::delete_);
    initiateHttpMessageDelivery();
}

void AsyncHttpClient::doDelete(
    const QUrl& url, nx::utils::MoveOnlyFunc<void (AsyncHttpClientPtr)> completionHandler)
{
    typedef void(AsyncHttpClient::*FuncToCallType)(const QUrl& /*url*/);

    doHttpOperation<const QUrl&>(
        std::move(completionHandler),
        static_cast<FuncToCallType>(&AsyncHttpClient::doDelete),
        url);
}

void AsyncHttpClient::doOptions(const QUrl& url)
{
    NX_ASSERT(url.isValid());

    resetDataBeforeNewRequest();
    m_requestUrl = url;
    m_contentLocationUrl = url;
    m_contentLocationUrl.setPath(QLatin1String("*"));
    composeRequest(nx_http::Method::options);
    initiateHttpMessageDelivery();
}

void AsyncHttpClient::doOptions(
    const QUrl& url,
    nx::utils::MoveOnlyFunc<void(AsyncHttpClientPtr)> completionHandler)
{
    typedef void(AsyncHttpClient::*FuncToCallType)(const QUrl& /*url*/);

    doHttpOperation<const QUrl&>(
        std::move(completionHandler),
        static_cast<FuncToCallType>(&AsyncHttpClient::doOptions),
        url);
}

void AsyncHttpClient::doUpgrade(
    const QUrl& url,
    const StringType& protocolToUpgradeTo)
{
    NX_ASSERT(url.isValid());

    resetDataBeforeNewRequest();
    m_requestUrl = url;
    m_contentLocationUrl = url;
    m_additionalHeaders.emplace("Connection", "Upgrade");
    m_additionalHeaders.emplace("Upgrade", protocolToUpgradeTo);
    composeRequest(nx_http::Method::options);
    initiateHttpMessageDelivery();
}

void AsyncHttpClient::doUpgrade(
    const QUrl& url,
    const StringType& protocolToUpgradeTo,
    nx::utils::MoveOnlyFunc<void(AsyncHttpClientPtr)> completionHandler)
{
    typedef void(AsyncHttpClient::*FuncToCallType)(
        const QUrl& /*url*/,
        const StringType& /*protocolToUpgradeTo*/);

    doHttpOperation<const QUrl&, const StringType&>(
        std::move(completionHandler),
        static_cast<FuncToCallType>(&AsyncHttpClient::doUpgrade),
        url,
        protocolToUpgradeTo);
}

const nx_http::Request& AsyncHttpClient::request() const
{
    return m_request;
}

const Response* AsyncHttpClient::response() const
{
    const Message& httpMsg = m_httpStreamReader.message();
    return httpMsg.type == MessageType::response ? httpMsg.response : NULL;
}

StringType AsyncHttpClient::contentType() const
{
    const Message& httpMsg = m_httpStreamReader.message();
    if (httpMsg.type == MessageType::none)
        return StringType();
    HttpHeaders::const_iterator contentTypeIter = httpMsg.headers().find("Content-Type");
    if (contentTypeIter == httpMsg.headers().end())
        return StringType();
    return contentTypeIter->second;
}

bool AsyncHttpClient::hasRequestSuccesed() const
{
    if (state() == sFailed)
        return false;

    if (auto resp = response())
    {
        auto status = resp->statusLine.statusCode;
        return status >= 200 && status < 300; // SUCCESS codes 2XX
    }
    return false;
}

BufferType AsyncHttpClient::fetchMessageBodyBuffer()
{
    const auto buffer = m_httpStreamReader.fetchMessageBody();
    if (logTraffic())
        NX_LOGX(lm("Response message body buffer:\n%1\n\n").arg(buffer), cl_logDEBUG2);

    return buffer;
}

const QUrl& AsyncHttpClient::url() const
{
    return m_requestUrl;
}

const QUrl& AsyncHttpClient::contentLocationUrl() const
{
    return m_contentLocationUrl;
}

quint64 AsyncHttpClient::bytesRead() const
{
    return m_bytesRead;
}

void AsyncHttpClient::setUseCompression(bool toggleUseEntityEncoding)
{
    m_contentEncodingUsed = toggleUseEntityEncoding;
}

void AsyncHttpClient::setSubsequentReconnectTries(int /*reconnectTries*/)
{
    //TODO #ak
}

void AsyncHttpClient::setTotalReconnectTries(int /*reconnectTries*/)
{
    //TODO #ak
}

void AsyncHttpClient::setUserAgent(const QString& userAgent)
{
    m_userAgent = userAgent;
}

void AsyncHttpClient::setUserName(const QString& userName)
{
    m_userName = userName;
}

void AsyncHttpClient::setUserPassword(const QString& userPassword)
{
    m_userPassword = userPassword;
}

void AsyncHttpClient::setProxyUserName(const QString& userName)
{
    m_proxyUserName = userName;
}

void AsyncHttpClient::setProxyUserPassword(const QString& userPassword)
{
    m_proxyUserPassword = userPassword;
}

void AsyncHttpClient::setAuth(const AuthInfo& auth)
{
    setUserName(auth.username);
    setUserPassword(auth.password);
    setProxyUserName(auth.proxyUsername);
    setProxyUserPassword(auth.proxyPassword);
    setProxyVia(auth.proxyEndpoint);
}

void AsyncHttpClient::setProxyVia(const SocketAddress& proxyEndpoint)
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

void AsyncHttpClient::setDisablePrecalculatedAuthorization(bool val)
{
    m_precalculatedAuthorizationDisabled = val;
}

void AsyncHttpClient::setSendTimeoutMs(unsigned int sendTimeoutMs)
{
    m_sendTimeoutMs = sendTimeoutMs;
}

void AsyncHttpClient::setResponseReadTimeoutMs(unsigned int _responseReadTimeoutMs)
{
    m_responseReadTimeoutMs = _responseReadTimeoutMs;
}

void AsyncHttpClient::setMessageBodyReadTimeoutMs(unsigned int messageBodyReadTimeoutMs)
{
    m_msgBodyReadTimeoutMs = messageBodyReadTimeoutMs;
}

void AsyncHttpClient::asyncConnectDone(SystemError::ErrorCode errorCode)
{
    NX_LOGX(lm("Opened connection to url %1. Result code %2")
        .arg(m_contentLocationUrl).arg(errorCode), cl_logDEBUG2);

    std::shared_ptr<AsyncHttpClient> sharedThis(shared_from_this());

    if (m_terminated)
        return;

    if (m_state != sWaitingConnectToHost)
    {
        NX_ASSERT(false);
        return;
    }

    if (errorCode == SystemError::noError)
    {
        //connect successful
        m_remoteEndpointWithProtocol = endpointWithProtocol(m_contentLocationUrl);
        serializeRequest();
        m_state = sSendingRequest;
        emit tcpConnectionEstablished(sharedThis);
        using namespace std::placeholders;
        NX_LOGX(lm("Sending request to url %1").arg(m_contentLocationUrl), cl_logDEBUG2);

        m_socket->sendAsync(m_requestBuffer, std::bind(&AsyncHttpClient::asyncSendDone, this, _1, _2));
        return;
    }

    NX_LOGX(lm("Failed to establish tcp connection to %1. %2")
        .arg(m_contentLocationUrl).arg(SystemError::toString(errorCode)), cl_logDEBUG1);
    m_lastSysErrorCode = errorCode;

    m_state = sFailed;
    const auto requestSequenceBak = m_requestSequence;
    emit done(sharedThis);
    if (m_requestSequence == requestSequenceBak)
        m_socket.reset();   //< Closing failed socket so that it is not reused.
}

void AsyncHttpClient::asyncSendDone(SystemError::ErrorCode errorCode, size_t bytesWritten)
{
    std::shared_ptr<AsyncHttpClient> sharedThis(shared_from_this());

    if (m_terminated)
        return;

    if (m_state != sSendingRequest)
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
        m_state = sFailed;
        m_lastSysErrorCode = errorCode;
        const auto requestSequenceBak = m_requestSequence;
        emit done(sharedThis);
        if (m_requestSequence == requestSequenceBak)
            m_socket.reset();
        return;
    }

    //TODO #ak recheck connection closure situation

    using namespace std::placeholders;

    m_requestBytesSent += bytesWritten;
    if ((int)m_requestBytesSent < m_requestBuffer.size())
    {
        m_socket->sendAsync(m_requestBuffer, std::bind(&AsyncHttpClient::asyncSendDone, this, _1, _2));
        return;
    }

    NX_LOGX(lm("Request has been successfully sent to %1: %2")
        .arg(m_contentLocationUrl)
        .args(logTraffic() ? request().toString() : request().requestLine.toString()),
        cl_logDEBUG2);

    const auto requestSequenceBak = m_requestSequence;
    emit requestHasBeenSent(sharedThis, m_authorizationTried);
    if (m_terminated || //user cancelled futher action
        (m_requestSequence != requestSequenceBak))  //user started new request within responseReceived handler
    {
        return;
    }

    m_state = m_expectOnlyBody ? sReadingMessageBody : sReceivingResponse;
    m_responseBuffer.resize(0);
    if (!m_socket->setRecvTimeout(m_responseReadTimeoutMs))
    {
        if (reconnectIfAppropriate())
            return;

        const auto sysErrorCode = SystemError::getLastOSErrorCode();
        NX_LOGX(lm("Url %1. Error setting receive timeout to %2 ms. %3")
            .arg(m_contentLocationUrl).arg(m_responseReadTimeoutMs)
            .arg(SystemError::toString(sysErrorCode)),
            cl_logDEBUG1);
        m_state = sFailed;
        const auto requestSequenceBak = m_requestSequence;
        emit done(sharedThis);
        if (m_requestSequence == requestSequenceBak)
            m_socket.reset();   //< Closing failed socket so that it is not reused.
        return;
    }

    m_socket->readSomeAsync(
        &m_responseBuffer,
        std::bind(&AsyncHttpClient::onSomeBytesReadAsync, this, _1, _2));
}

void AsyncHttpClient::onSomeBytesReadAsync(
    SystemError::ErrorCode errorCode,
    size_t bytesRead)
{
    using namespace std::placeholders;

    std::shared_ptr<AsyncHttpClient> sharedThis(shared_from_this());

    if (m_terminated)
        return;

    if (errorCode != SystemError::noError)
    {
        const auto stateBak = m_state;
        if ((m_httpStreamReader.state() == HttpStreamReader::messageDone) &&
                m_httpStreamReader.currentMessageNumber() == m_awaitedMessageNumber)
        {
            m_state = sDone;
        }
        else
        {
            // Reconnecting only in case of failure.
            if (reconnectIfAppropriate())
                return;
            m_state = sFailed;
        }

        NX_LOGX(lm("Error reading (state %1) http response from %2. %3")
            .arg(stateBak).arg(m_contentLocationUrl).arg(SystemError::toString(errorCode)),
            cl_logDEBUG1);
        m_lastSysErrorCode = errorCode;
        const auto requestSequenceBak = m_requestSequence;
        emit done(sharedThis);
        if (m_requestSequence == requestSequenceBak)
            m_socket.reset();   //< Closing failed socket so that it is not reused.
        return;
    }

    processReceivedBytes(std::move(sharedThis), bytesRead);
}

void AsyncHttpClient::resetDataBeforeNewRequest()
{
    ++m_requestSequence;
    m_authorizationTried = false;
    m_ha1RecalcTried = false;
    m_numberOfRedirectsTried = 0;
    m_request = nx_http::Request();
}

void AsyncHttpClient::initiateHttpMessageDelivery()
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
        m_totalRequests = 0;
    }
    ++m_totalRequests;
    m_bytesRead = 0;

    m_state = sInit;

    dispatch(
        [this, canUseExistingConnection]()
        {
            if (canUseExistingConnection)
            {
                NX_ASSERT(m_socket);
                m_socket->cancelIOSync(nx::network::aio::etNone);
                ++m_awaitedMessageNumber;   //current message will be skipped

                serializeRequest();
                m_state = sSendingRequest;
                NX_LOGX(lm("Sending request to url %1").arg(m_contentLocationUrl), cl_logDEBUG2);
                m_socket->sendAsync(
                    m_requestBuffer,
                    std::bind(&AsyncHttpClient::asyncSendDone, this, _1, _2));
                return;
            }

            m_socket.reset();

            initiateTcpConnection();
        });
}

void AsyncHttpClient::initiateTcpConnection()
{
    const SocketAddress remoteAddress =
        m_proxyEndpoint
        ? m_proxyEndpoint.get()
        : SocketAddress(
            m_contentLocationUrl.host(),
            m_contentLocationUrl.port(nx_http::defaultPortForScheme(m_contentLocationUrl.scheme().toLatin1())));

    m_state = sInit;

    m_socket = SocketFactory::createStreamSocket(m_contentLocationUrl.scheme() == lm("https"));

    NX_LOGX(lm("Opening connection to %1. url %2, socket %3")
        .arg(remoteAddress).arg(m_contentLocationUrl).arg(m_socket->handle()), cl_logDEBUG2);

    m_socket->bindToAioThread(m_aioThreadBinder.getAioThread());
    m_connectionClosed = false;
    if (!m_socket->setNonBlockingMode(true) ||
        !m_socket->setSendTimeout(m_sendTimeoutMs) ||
        !m_socket->setRecvTimeout(m_responseReadTimeoutMs))
    {
        m_socket->post(
            std::bind(
                &AsyncHttpClient::asyncConnectDone,
                this,
                SystemError::getLastOSErrorCode()));
        return;
    }

    m_state = sWaitingConnectToHost;

    //starting async connect
    m_socket->connectAsync(
        remoteAddress,
        std::bind(&AsyncHttpClient::asyncConnectDone, this, std::placeholders::_1));
}

size_t AsyncHttpClient::parseReceivedBytes(size_t bytesRead)
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
            m_state = sFailed;
            m_lastSysErrorCode = SystemError::connectionReset;
        }
        else
        {
            m_state = (m_httpStreamReader.state() == HttpStreamReader::messageDone) ||
                (m_httpStreamReader.state() == HttpStreamReader::pullingLineEndingBeforeMessageBody) ||
                (m_httpStreamReader.state() == HttpStreamReader::readingMessageBody)
                ? sDone
                : sFailed;
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
        m_state = sFailed;
        return -1;
    }

    m_bytesRead += bytesProcessed;

    if (m_httpStreamReader.state() == HttpStreamReader::parseError)
    {
        m_state = sFailed;
        return bytesProcessed;
    }

    NX_ASSERT(m_httpStreamReader.currentMessageNumber() <= m_awaitedMessageNumber);
    if (m_httpStreamReader.currentMessageNumber() < m_awaitedMessageNumber)
        return bytesProcessed;   //reading some old message, not changing state in this case

    if (m_httpStreamReader.state() == HttpStreamReader::messageDone)
        m_state = sDone;
    return bytesProcessed;
}

void AsyncHttpClient::processReceivedBytes(
    std::shared_ptr<AsyncHttpClient> sharedThis,
    std::size_t bytesRead)
{
    using namespace std::placeholders;

    for (;;)
    {
        const auto stateBak = m_state;
        const size_t bytesParsed = parseReceivedBytes(bytesRead);
        QByteArray receivedBytesLeft;
        if (bytesParsed != (std::size_t)-1)
            receivedBytesLeft = m_responseBuffer.mid((int)bytesParsed);
        m_responseBuffer.resize(0);

        bool continueReceiving = false;
        switch (stateBak)
        {
            case sReceivingResponse:
            {
                processResponseHeadersBytes(
                    sharedThis,
                    &continueReceiving);
                break;
            }

            case sReadingMessageBody:
            {
                processResponseMessageBodyBytes(
                    sharedThis,
                    bytesParsed,
                    &continueReceiving);
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

        if (receivedBytesLeft.isEmpty())
        {
            NX_ASSERT(m_responseBuffer.size() == 0);
            m_socket->readSomeAsync(
                &m_responseBuffer,
                std::bind(&AsyncHttpClient::onSomeBytesReadAsync, this, _1, _2));
            return;
        }

        NX_CRITICAL(bytesParsed != 0 && bytesParsed != (std::size_t)-1);
        m_responseBuffer = std::move(receivedBytesLeft);
        m_responseBuffer.reserve(RESPONSE_BUFFER_SIZE);
        bytesRead = m_responseBuffer.size();
    }
}

void AsyncHttpClient::processResponseHeadersBytes(
    std::shared_ptr<AsyncHttpClient> sharedThis,
    bool* const continueReceiving)
{
    using namespace std::placeholders;

    //TODO/IMPL reconnect in case of error

    if (m_state == sFailed)
    {
        emit done(sharedThis);
        return;
    }

    // Connection could have already been closed by remote peer.

    if (m_httpStreamReader.currentMessageNumber() < m_awaitedMessageNumber ||       //still reading previous message
        m_httpStreamReader.state() <= HttpStreamReader::readingMessageHeaders)     //still reading message headers
    {
        //response has not been read yet, reading futher
        m_responseBuffer.resize(0);
        if (m_connectionClosed)
        {
            if (reconnectIfAppropriate())
                return;

            NX_LOGX(lm("Failed to read (1) response from %1. %2")
                .arg(m_contentLocationUrl).arg(SystemError::connectionReset), cl_logDEBUG1);
            m_state = sFailed;
            emit done(sharedThis);
            return;
        }
        *continueReceiving = true;
        return;
    }

    //read http message headers
    if (m_httpStreamReader.message().type != nx_http::MessageType::response)
    {
        NX_LOGX(lm("Unexpectedly received request from %1:%2 while expecting response! Ignoring...")
            .arg(m_contentLocationUrl.host()).arg(m_contentLocationUrl.port()), cl_logDEBUG1);
        m_state = sFailed;
        emit done(sharedThis);
        return;
    }

    NX_LOGX(lm("Response from %1 has been successfully read: %2")
        .arg(m_contentLocationUrl)
        .arg(logTraffic() ? response()->toString() : response()->statusLine.toString()),
        cl_logDEBUG2);

    if (repeatRequestIfNeeded(*m_httpStreamReader.message().response))
        return;

    const bool messageHasMessageBody =
        (m_httpStreamReader.state() == HttpStreamReader::readingMessageBody) ||
        (m_httpStreamReader.state() == HttpStreamReader::pullingLineEndingBeforeMessageBody) ||
        (m_httpStreamReader.messageBodyBufferSize() > 0);

    m_state = sResponseReceived;
    const auto requestSequenceBak = m_requestSequence;
    emit responseReceived(sharedThis);
    if (m_terminated ||
        (m_requestSequence != requestSequenceBak))  //user started new request within responseReceived handler
    {
        return;
    }

    //does message body follow?
    if (!messageHasMessageBody)
    {
        //no message body: done
        m_state = m_httpStreamReader.state() == HttpStreamReader::parseError ? sFailed : sDone;
        emit done(sharedThis);
        return;
    }

    //starting reading message body
    m_state = sReadingMessageBody;

    if (m_httpStreamReader.messageBodyBufferSize() > 0 &&   //some message body has been read
        m_state == sReadingMessageBody)                    //client wants to read message body
    {
        emit someMessageBodyAvailable(sharedThis);
        if (m_terminated)
            return;
        if (m_forcedEof)
        {
            m_forcedEof = false;
            return;
        }
    }

    if (((m_httpStreamReader.state() == HttpStreamReader::readingMessageBody) ||
        (m_httpStreamReader.state() == HttpStreamReader::pullingLineEndingBeforeMessageBody)) &&
        (!m_connectionClosed))
    {
        //reading more data
        m_responseBuffer.resize(0);
        if (!m_socket->setRecvTimeout(m_msgBodyReadTimeoutMs))
        {
            NX_LOGX(lm("Failed to read (1) response from %1. %2")
                .arg(m_contentLocationUrl).arg(SystemError::getLastOSErrorText()),
                cl_logDEBUG1);

            m_state = sFailed;
            emit done(sharedThis);
            return;
        }
        *continueReceiving = true;
        return;
    }

    //message body has been received with request
    NX_ASSERT(
        m_httpStreamReader.state() == HttpStreamReader::messageDone ||
        m_httpStreamReader.state() == HttpStreamReader::parseError);

    m_state = m_httpStreamReader.state() == HttpStreamReader::parseError ? sFailed : sDone;
    emit done(sharedThis);
}

bool AsyncHttpClient::repeatRequestIfNeeded(const Response& response)
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

bool AsyncHttpClient::sendRequestToNewLocation(const Response& response)
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

    const auto method = m_request.requestLine.method;
    composeRequest(method);
    initiateHttpMessageDelivery();
    return true;
}

void AsyncHttpClient::processResponseMessageBodyBytes(
    std::shared_ptr<AsyncHttpClient> sharedThis,
    std::size_t bytesRead,
    bool* const continueReceiving)
{
    using namespace std::placeholders;

    //TODO #ak reconnect in case of error
    if (bytesRead != (std::size_t)-1)
    {
        emit someMessageBodyAvailable(sharedThis);
        if (m_terminated)
            return;
        if (m_forcedEof)
        {
            m_forcedEof = false;
            return;
        }
    }

    if (m_state != sFailed && m_state != sDone)
    {
        m_responseBuffer.resize(0);
        *continueReceiving = true;
        return;
    }

    emit done(sharedThis);
}

void AsyncHttpClient::composeRequest(const nx_http::StringType& httpMethod)
{
    const bool useHttp11 = true;   //TODO #ak check if we need it (e.g. we using keep-alive or requesting live capture)

    m_request.requestLine.method = httpMethod;

    if (m_proxyEndpoint)
    {
        m_request.requestLine.url = m_contentLocationUrl;
    }
    else    //if no proxy specified then erasing http://host:port from request url
    {
        m_request.requestLine.url = m_contentLocationUrl.path();
        m_request.requestLine.url.setQuery(m_contentLocationUrl.query());
        m_request.requestLine.url.setFragment(m_contentLocationUrl.fragment());
    }
    m_request.requestLine.version = useHttp11 ? nx_http::http_1_1 : nx_http::http_1_0;

    nx_http::insertOrReplaceHeader(
        &m_request.headers,
        HttpHeader("Date", nx_http::formatDateTime(QDateTime::currentDateTime())));
    nx_http::insertOrReplaceHeader(
        &m_request.headers,
        HttpHeader(
            "User-Agent",
            m_userAgent.isEmpty() ? nx_http::userAgentString() : m_userAgent.toLatin1()));
    if (useHttp11)
    {
        if (httpMethod == nx_http::Method::get || httpMethod == nx_http::Method::head)
        {
            if (m_contentEncodingUsed)
                m_request.headers.insert(std::make_pair("Accept-Encoding", "gzip"));
        }

        if (m_additionalHeaders.count("Connection") == 0)
        {
            nx_http::insertOrReplaceHeader(
                &m_request.headers,
                HttpHeader("Connection", "keep-alive"));
        }

        if (m_additionalHeaders.count("Host") == 0)
        {
            nx_http::insertOrReplaceHeader(
                &m_request.headers,
                HttpHeader(
                    "Host",
                    nx::network::url::getEndpoint(m_contentLocationUrl).toString().toUtf8()));
        }
    }

    // It is not correct just to replace headers because there 
    // could be multiple headers with same name in m_additionalHeaders.
    for (const auto& header: m_additionalHeaders)
        m_request.headers.erase(header.first);
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
            //not using Basic authentication by default, since it is not secure
            nx_http::removeHeader(&m_request.headers, header::Authorization::NAME);
        }
    }
}

void AsyncHttpClient::addAdditionalHeader(const StringType& key, const StringType& value)
{
    m_additionalHeaders.emplace(key, value);
}

void AsyncHttpClient::removeAdditionalHeader(const StringType& key)
{
    m_additionalHeaders.erase(key);
}

void AsyncHttpClient::addRequestHeaders(const HttpHeaders& headers)
{
    for (HttpHeaders::const_iterator itr = headers.begin(); itr != headers.end(); ++itr)
        m_additionalHeaders.emplace(itr->first, itr->second);
}

void AsyncHttpClient::serializeRequest()
{
    m_requestBuffer.clear();
    m_request.serialize(&m_requestBuffer);
    m_requestBytesSent = 0;
}

bool AsyncHttpClient::reconnectIfAppropriate()
{
    if ((m_connectionClosed || m_state == sSendingRequest || m_state == sReceivingResponse) &&
        m_bytesRead == 0 &&
        m_totalRequests > 1)
    {
        //< Reconnect if TCP timeout for keep-alive connections
        m_connectionClosed = true;
        initiateHttpMessageDelivery();
        return true;
    }

    //TODO #ak we need reconnect and request entity from the point we stopped at
    return false;
}

AsyncHttpClientPtr AsyncHttpClient::create()
{
    return AsyncHttpClientPtr(std::shared_ptr<AsyncHttpClient>(new AsyncHttpClient()));
}

QString AsyncHttpClient::endpointWithProtocol(const QUrl& url)
{
    return lm("%1://%2:%3")
        .arg(url.scheme())
        .arg(url.host())
        .arg(url.port(nx_http::defaultPortForScheme(url.scheme().toLatin1())));
}

bool AsyncHttpClient::resendRequestWithAuthorization(
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
                m_authType != authDigestWithPasswordHash
                    ? userPassword.toUtf8()
                    : boost::optional<nx_http::BufferType>(),
                m_authType == authDigestWithPasswordHash
                    ? userPassword.toLatin1()
                    : boost::optional<nx_http::BufferType>(),
                m_contentLocationUrl.toString(QUrl::RemoveScheme | QUrl::RemovePort | QUrl::RemoveAuthority | QUrl::FullyEncoded).toUtf8(),
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
            m_authType == authDigestWithPasswordHash
                ? boost::optional<BufferType>()
                : boost::optional<BufferType>(userPassword.toLatin1()),
            m_authType == authDigestWithPasswordHash
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

void AsyncHttpClient::doSomeCustomLogic(
    const nx_http::Response& response,
    Request* const request)
{
    //TODO #ak this method is not part of http, so it should not be in this class

    if (m_authType == authDigestWithPasswordHash)
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

namespace {

struct SharedState
{
    QMetaObject::Connection qtConnection;
    nx::utils::MoveOnlyFunc<void(AsyncHttpClientPtr)> completionHandler;
};

} // namespace 

template<typename ... Args>
void AsyncHttpClient::doHttpOperation(
    nx::utils::MoveOnlyFunc<void(AsyncHttpClientPtr)> completionHandler,
    void(AsyncHttpClient::*func)(Args...),
    Args... args)
{
    auto sharedState = std::make_shared<SharedState>();
    sharedState->completionHandler = std::move(completionHandler);
    auto* qtConnectionPtr = &sharedState->qtConnection;
    *qtConnectionPtr = QObject::connect(
        this, &AsyncHttpClient::done,
        [this, sharedState = std::move(sharedState)](
            AsyncHttpClientPtr httpClient)
        {
            sharedState->completionHandler(httpClient);
            disconnect(sharedState->qtConnection);
        });
    (this->*func)(args...);
}

const char* AsyncHttpClient::toString(State state)
{
    switch (state)
    {
    case sInit:
        return "init";
    case sWaitingConnectToHost:
        return "waitingConnectToHost";
    case sSendingRequest:
        return "sendingRequest";
    case sReceivingResponse:
        return "receivingResponse";
    case sResponseReceived:
        return "responseReceived";
    case sReadingMessageBody:
        return "readingMessageBody";
    case sFailed:
        return "failed";
    case sDone:
        return "done";
    default:
        return "unknown";
    }
}

void AsyncHttpClient::setAuthType(AuthType value)
{
    m_authType = value;
}

AuthInfoCache::AuthorizationCacheItem AsyncHttpClient::authCacheItem() const
{
    return m_authCacheItem;
}

void AsyncHttpClient::forceEndOfMsgBody()
{
    m_forcedEof = true;
    m_httpStreamReader.forceEndOfMsgBody();
}

void AsyncHttpClient::setExpectOnlyMessageBodyWithoutHeaders(bool expectOnlyBody)
{
    m_expectOnlyBody = expectOnlyBody;
}

//-------------------------------------------------------------------------------------------------
// Utilities.

void downloadFileAsyncEx(
    const QUrl& url,
    std::function<void(SystemError::ErrorCode, int, nx_http::StringType, nx_http::BufferType)> completionHandler,
    nx_http::AsyncHttpClientPtr httpClientCaptured)
{
    auto requestCompletionFunc = [httpClientCaptured, completionHandler]
        (nx_http::AsyncHttpClientPtr httpClient) mutable
    {
        httpClientCaptured->disconnect(nullptr, (const char*)nullptr);
        httpClientCaptured.reset();

        if (httpClient->failed())
        {
            completionHandler(
                SystemError::connectionReset,
                nx_http::StatusCode::ok,
                nx_http::StringType(),
                nx_http::BufferType());
            return;
        }

        if (httpClient->response()->statusLine.statusCode != nx_http::StatusCode::ok &&
            httpClient->response()->statusLine.statusCode != nx_http::StatusCode::partialContent)
        {
            completionHandler(
                SystemError::noError,
                httpClient->response()->statusLine.statusCode,
                nx_http::StringType(),
                nx_http::BufferType());
            return;
        }

        completionHandler(
            SystemError::noError,
            httpClient->response()->statusLine.statusCode,
            httpClient->contentType(),
            httpClient->fetchMessageBodyBuffer());
    };
    QObject::connect(
        httpClientCaptured.get(), &nx_http::AsyncHttpClient::done,
        httpClientCaptured.get(), requestCompletionFunc,
        Qt::DirectConnection);

    httpClientCaptured->doGet(url);
}

void downloadFileAsync(
    const QUrl& url,
    std::function<void(SystemError::ErrorCode, int, nx_http::BufferType)> completionHandler,
    const nx_http::HttpHeaders& extraHeaders,
    AsyncHttpClient::AuthType authType,
    AsyncHttpClient::Timeouts timeouts)
{
    auto handler = [completionHandler](
        SystemError::ErrorCode osErrorCode,
        int statusCode,
        nx_http::StringType,
        nx_http::BufferType msgBody)
    {
        completionHandler(osErrorCode, statusCode, msgBody);
    };
    downloadFileAsyncEx(url, handler, extraHeaders, authType, timeouts);
}

void downloadFileAsyncEx(
    const QUrl& url,
    std::function<void(SystemError::ErrorCode, int, nx_http::StringType, nx_http::BufferType)> completionHandler,
    const nx_http::HttpHeaders& extraHeaders,
    AsyncHttpClient::AuthType authType,
    AsyncHttpClient::Timeouts timeouts)
{
    nx_http::AsyncHttpClientPtr httpClient = nx_http::AsyncHttpClient::create();
    httpClient->setAdditionalHeaders(extraHeaders);
    httpClient->setAuthType(authType);
    httpClient->setSendTimeoutMs(timeouts.sendTimeout.count());
    httpClient->setResponseReadTimeoutMs(timeouts.responseReadTimeout.count());
    httpClient->setMessageBodyReadTimeoutMs(timeouts.messageBodyReadTimeout.count());
    downloadFileAsyncEx(url, completionHandler, std::move(httpClient));
}

SystemError::ErrorCode downloadFileSync(
    const QUrl& url,
    int* const statusCode,
    nx_http::BufferType* const msgBody)
{
    bool done = false;
    SystemError::ErrorCode resultingErrorCode = SystemError::noError;
    std::mutex mtx;
    std::condition_variable condVar;
    downloadFileAsync(
        url,
        [&resultingErrorCode, statusCode, msgBody, &mtx, &condVar, &done]
    (SystemError::ErrorCode errorCode, int _statusCode, const nx_http::BufferType& _msgBody) {
        resultingErrorCode = errorCode;
        *statusCode = _statusCode;
        *msgBody = _msgBody;
        std::unique_lock<std::mutex> lk(mtx);
        done = true;
        condVar.notify_all();
    });

    std::unique_lock<std::mutex> lk(mtx);
    while (!done)
        condVar.wait(lk);

    return resultingErrorCode;
}

void uploadDataAsync(const QUrl &url
    , const QByteArray &data
    , const QByteArray &contentType
    , const nx_http::HttpHeaders &extraHeaders
    , const UploadCompletionHandler &callback
    , const AsyncHttpClient::AuthType authType
    , const QString &user
    , const QString &password)
{
    nx_http::AsyncHttpClientPtr httpClientHolder = nx_http::AsyncHttpClient::create();
    httpClientHolder->setAdditionalHeaders(extraHeaders);
    if (!user.isEmpty())
        httpClientHolder->setUserName(user);
    if (!password.isEmpty())
        httpClientHolder->setUserPassword(password);

    httpClientHolder->setAuthType(authType);

    auto completionFunc = [callback, httpClientHolder]
        (nx_http::AsyncHttpClientPtr httpClient) mutable
    {
        httpClientHolder->disconnect(nullptr, static_cast<const char *>(nullptr));
        httpClientHolder.reset();

        if (httpClient->failed())
            return callback(SystemError::connectionReset, nx_http::StatusCode::ok);

        const auto response = httpClient->response();
        const auto statusLine = response->statusLine;
        if ((statusLine.statusCode != nx_http::StatusCode::ok)
            && (statusLine.statusCode != nx_http::StatusCode::partialContent))
        {
            return callback(SystemError::noError, statusLine.statusCode);
        }

        callback(SystemError::noError, statusLine.statusCode);
    };

    QObject::connect(httpClientHolder.get(), &nx_http::AsyncHttpClient::done,
        httpClientHolder.get(), completionFunc, Qt::DirectConnection);

    httpClientHolder->doPost(url, contentType, data);
}

SystemError::ErrorCode uploadDataSync(const QUrl &url
    , const QByteArray &data
    , const QByteArray &contentType
    , const QString &user
    , const QString &password
    , const AsyncHttpClient::AuthType authType
    , nx_http::StatusCode::Value *httpCode)
{
    bool done = false;
    SystemError::ErrorCode result = SystemError::noError;
    std::mutex mutex;
    std::condition_variable waiter;


    const auto callback = [&result, &mutex, &waiter, &done, httpCode]
        (SystemError::ErrorCode errorCode, int statusCode)
    {
        result = errorCode;
        if (httpCode)
        {
            *httpCode = (errorCode == SystemError::noError
                ? static_cast<nx_http::StatusCode::Value>(statusCode)
                : nx_http::StatusCode::internalServerError);
        }

        std::unique_lock<std::mutex> lk(mutex);
        done = true;
        waiter.notify_all();
    };

    uploadDataAsync(url, data, contentType
        , nx_http::HttpHeaders(), callback, authType, user, password);

    std::unique_lock<std::mutex> guard(mutex);
    while (!done)
        waiter.wait(guard);

    return result;
}

} // namespace nx_http
