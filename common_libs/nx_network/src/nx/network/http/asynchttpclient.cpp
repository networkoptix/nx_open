/**********************************************************
* 22 nov 2012
* a.kolesnikov
***********************************************************/

#include "asynchttpclient.h"

#include <condition_variable>
#include <mutex>

#include <QtCore/QCryptographicHash>
#include <QtCore/QDateTime>
#include <nx/utils/thread/mutex.h>

#include <http/custom_headers.h>
#include <utils/crypt/linux_passwd_crypt.h>
#include <nx/utils/log/log.h>
#include <utils/common/util.h>
#include <nx/network/socket_factory.h>
#include <utils/common/systemerror.h>

#include "auth_tools.h"

//TODO: #ak persistent connection support
//TODO: #ak MUST call cancelAsyncIO with 1st parameter set to false
//TODO: #ak reconnect support

static const int DEFAULT_SEND_TIMEOUT = 3000;
static const int DEFAULT_RESPONSE_READ_TIMEOUT = 3000;
//static const int DEFAULT_HTTP_PORT = 80;

using std::make_pair;

namespace nx_http
{
    static const size_t RESPONSE_BUFFER_SIZE = 16 * 1024;

    constexpr const std::chrono::seconds AsyncHttpClient::Timeouts::kDefaultSendTimeout;
    constexpr const std::chrono::seconds AsyncHttpClient::Timeouts::kDefaultResponseReadTimeout;
    constexpr const std::chrono::seconds AsyncHttpClient::Timeouts::kDefaultMessageBodyReadTimeout;

    AsyncHttpClient::Timeouts::Timeouts()
        :
        sendTimeout(kDefaultSendTimeout),
        responseReadTimeout(kDefaultResponseReadTimeout),
        messageBodyReadTimeout(kDefaultMessageBodyReadTimeout)
    {
    }

    AsyncHttpClient::AsyncHttpClient()
        :
        m_state(sInit),
        m_connectionClosed(false),
        m_requestBytesSent(0),
        m_authorizationTried(false),
        m_proxyAuthorizationTried(false),
        m_ha1RecalcTried(false),
        m_terminated(false),
        m_totalBytesRead(0),
        m_contentEncodingUsed(true),
        m_sendTimeoutMs(DEFAULT_SEND_TIMEOUT),
        m_responseReadTimeoutMs(DEFAULT_RESPONSE_READ_TIMEOUT),
        m_msgBodyReadTimeoutMs(0),
        m_authType(authBasicAndDigest),
        m_awaitedMessageNumber(0),
        m_lastSysErrorCode(SystemError::noError),
        m_requestSequence(0),
        m_forcedEof(false),
        m_precalculatedAuthorizationDisabled(false)
    {
        m_responseBuffer.reserve(RESPONSE_BUFFER_SIZE);
    }

    AsyncHttpClient::~AsyncHttpClient()
    {
        terminate();
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

    void AsyncHttpClient::terminate()
    {
        pleaseStopSync();
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

    void AsyncHttpClient::pleaseStopSync()
    {
        if (m_aioThreadBinder.isInSelfAioThread())
            stopWhileInAioThread();
        else
            QnStoppableAsync::pleaseStopSync();
    }

    void AsyncHttpClient::stopWhileInAioThread()
    {
        m_terminated = true;
        if (m_socket)
            m_socket->pleaseStopSync();
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
        return m_state == sFailed;
    }

    SystemError::ErrorCode AsyncHttpClient::lastSysErrorCode() const
    {
        return m_lastSysErrorCode;
    }

    //!Start request to \a url
    /*!
    \return true, if socket is created and async connect is started. false otherwise
    To get error description use SystemError::getLastOSErrorCode()
    */
    void AsyncHttpClient::doGet(const QUrl& url)
    {
        NX_ASSERT(url.isValid());

        resetDataBeforeNewRequest();
        m_url = url;
        composeRequest(nx_http::Method::GET);
        initiateHttpMessageDelivery(url);
    }

    void AsyncHttpClient::doPost(
        const QUrl& url,
        const nx_http::StringType& contentType,
        nx_http::StringType messageBody,
        bool includeContentLength)
    {
        NX_ASSERT(url.isValid());

        resetDataBeforeNewRequest();
        m_url = url;
        composeRequest(nx_http::Method::POST);
        m_request.headers.insert(make_pair("Content-Type", contentType));
        if (includeContentLength)
            m_request.headers.insert(make_pair("Content-Length", StringType::number(messageBody.size())));
        //TODO #ak support chunked encoding & compression
        m_request.headers.insert(make_pair("Content-Encoding", "identity"));
        m_request.messageBody = std::move(messageBody);
        initiateHttpMessageDelivery(url);
    }

    void AsyncHttpClient::doPut(
        const QUrl& url,
        const nx_http::StringType& contentType,
        nx_http::StringType messageBody)
    {
        NX_ASSERT(url.isValid());

        resetDataBeforeNewRequest();
        m_url = url;
        composeRequest(nx_http::Method::PUT);
        m_request.headers.insert(make_pair("Content-Type", contentType));
        m_request.headers.insert(make_pair("Content-Length", StringType::number(messageBody.size())));
        //TODO #ak support chunked encoding & compression
        m_request.messageBody = std::move(messageBody);
        initiateHttpMessageDelivery(url);
    }

    const nx_http::Request& AsyncHttpClient::request() const
    {
        return m_request;
    }

    /*!
    Response is valid only after signal \a responseReceived() has been emitted
    */
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

    //!Returns current message body buffer, clearing it
    BufferType AsyncHttpClient::fetchMessageBodyBuffer()
    {
        return m_httpStreamReader.fetchMessageBody();
    }

    const QUrl& AsyncHttpClient::url() const
    {
        return m_url;
    }

    quint64 AsyncHttpClient::totalBytesRead() const
    {
        return m_totalBytesRead;
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

    void AsyncHttpClient::setEffectiveUserName(const QString& effectiveUserName)
    {
        m_effectiveUserName = effectiveUserName;
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

    void AsyncHttpClient::asyncConnectDone(AbstractSocket* sock, SystemError::ErrorCode errorCode)
    {
        std::shared_ptr<AsyncHttpClient> sharedThis(shared_from_this());

        if (m_terminated)
            return;

        NX_ASSERT(sock == m_socket.get());

        if (m_state != sWaitingConnectToHost)
        {
            NX_ASSERT(false);
            return;
        }

        if (errorCode == SystemError::noError)
        {
            //connect successful
            m_remoteEndpoint = SocketAddress(m_url.host(), m_url.port(nx_http::DEFAULT_HTTP_PORT));
            serializeRequest();
            m_state = sSendingRequest;
            emit tcpConnectionEstablished(sharedThis);
            using namespace std::placeholders;
            m_socket->sendAsync(m_requestBuffer, std::bind(&AsyncHttpClient::asyncSendDone, this, sock, _1, _2));
            return;
        }

        NX_LOGX(lit("Failed to establish tcp connection to %1. %2").
            arg(m_url.toString()).arg(SystemError::toString(errorCode)), cl_logDEBUG1);
        m_lastSysErrorCode = errorCode;
        if (reconnectIfAppropriate())
            return;

        m_state = sFailed;
        emit done(sharedThis);
        m_socket.reset();   //closing failed socket so that it is not reused
    }

    void AsyncHttpClient::asyncSendDone(AbstractSocket* sock, SystemError::ErrorCode errorCode, size_t bytesWritten)
    {
        std::shared_ptr<AsyncHttpClient> sharedThis(shared_from_this());

        if (m_terminated)
            return;

        NX_ASSERT(sock == m_socket.get());

        if (m_state != sSendingRequest)
        {
            NX_ASSERT(false);
            return;
        }

        if (errorCode != SystemError::noError)
        {
            if (reconnectIfAppropriate())
                return;
            NX_LOGX(lit("Error sending (1) http request to %1. %2").arg(m_url.toString()).arg(SystemError::toString(errorCode)), cl_logDEBUG1);
            m_state = sFailed;
            m_lastSysErrorCode = errorCode;
            emit done(sharedThis);
            m_socket.reset();
            return;
        }

        //TODO #ak recheck connection closure situation

        using namespace std::placeholders;

        m_requestBytesSent += bytesWritten;
        if ((int)m_requestBytesSent < m_requestBuffer.size())
        {
            m_socket->sendAsync(m_requestBuffer, std::bind(&AsyncHttpClient::asyncSendDone, this, sock, _1, _2));
            return;
        }

        NX_LOGX(lit("Http request has been successfully sent to %1").arg(m_url.toString()), cl_logDEBUG2);

        const auto requestSequenceBak = m_requestSequence;
        emit requestHasBeenSent(sharedThis, m_authorizationTried);
        if (m_terminated || //user cancelled futher action
            (m_requestSequence != requestSequenceBak))  //user started new request within responseReceived handler
        {
            return;
        }

        m_state = sReceivingResponse;
        m_responseBuffer.resize(0);
        if (!m_socket->setRecvTimeout(m_responseReadTimeoutMs))
        {
            NX_LOGX(lit("Error reading (1) http response from %1. %2").arg(m_url.toString()).arg(SystemError::getLastOSErrorText()), cl_logDEBUG1);
            m_state = sFailed;
            emit done(sharedThis);
            m_socket.reset();
            return;
        }

        m_socket->readSomeAsync(
            &m_responseBuffer,
            std::bind(&AsyncHttpClient::onSomeBytesReadAsync, this, sock, _1, _2));
    }

    void AsyncHttpClient::onSomeBytesReadAsync(AbstractSocket* sock, SystemError::ErrorCode errorCode, size_t bytesRead)
    {
        using namespace std::placeholders;

        std::shared_ptr<AsyncHttpClient> sharedThis(shared_from_this());

        if (m_terminated)
            return;

        NX_ASSERT(sock == m_socket.get());

        if (errorCode != SystemError::noError)
        {
            if (reconnectIfAppropriate())
                return;
            NX_LOGX(lit("Error reading (state %1) http response from %2. %3").arg(m_state).arg(m_url.toString()).arg(SystemError::toString(errorCode)), cl_logDEBUG1);
            m_state =
                ((m_httpStreamReader.state() == HttpStreamReader::messageDone) &&
                    m_httpStreamReader.currentMessageNumber() == m_awaitedMessageNumber)
                ? sDone
                : sFailed;
            m_lastSysErrorCode = errorCode;
            emit done(sharedThis);
            m_socket.reset();
            return;
        }

        switch (m_state)
        {
        case sReceivingResponse:
        {
            readAndParseHttp(bytesRead);
            //TODO/IMPL reconnect in case of error

            if (m_state == sFailed)
            {
                emit done(sharedThis);
                break;
            }

            //connection could be closed by remote peer already

            if (m_httpStreamReader.currentMessageNumber() < m_awaitedMessageNumber ||       //still reading previous message
                m_httpStreamReader.state() <= HttpStreamReader::readingMessageHeaders)     //still reading message headers
            {
                //response has not been read yet, reading futher
                m_responseBuffer.resize(0);
                if (m_connectionClosed)
                {
                    NX_LOGX(lit("Failed to read (1) response from %1. %2").
                        arg(m_url.toString()).arg(SystemError::connectionReset), cl_logDEBUG1);
                    m_state = sFailed;
                    emit done(sharedThis);
                    return;
                }
                m_socket->readSomeAsync(
                    &m_responseBuffer,
                    std::bind(&AsyncHttpClient::onSomeBytesReadAsync, this, sock, _1, _2));
                return;
            }

            //read http message headers
            if (m_httpStreamReader.message().type != nx_http::MessageType::response)
            {
                NX_LOGX(lit("Unexpectedly received request from %1:%2 while expecting response! Ignoring...").
                    arg(m_url.host()).arg(m_url.port()), cl_logDEBUG1);
                m_state = sFailed;
                emit done(sharedThis);
                return;
            }

            //response read
            NX_LOGX(lit("Http response from %1 has been successfully read. Status line: %2(%3)").
                arg(m_url.toString()).arg(m_httpStreamReader.message().response->statusLine.statusCode).
                arg(QLatin1String(m_httpStreamReader.message().response->statusLine.reasonPhrase)), cl_logDEBUG2);

            const Response* response = m_httpStreamReader.message().response;
            if (response->statusLine.statusCode == StatusCode::unauthorized)
            {
                //TODO #ak following block should be moved somewhere
                if (!m_ha1RecalcTried &&
                    response->headers.find(Qn::REALM_HEADER_NAME) != response->headers.cend())
                {
                    m_authorizationTried = false;
                    m_ha1RecalcTried = true;
                }

                if (!m_authorizationTried && (!m_userName.isEmpty() || !m_userPassword.isEmpty()))
                {
                    //trying authorization
                    if (resendRequestWithAuthorization(*response))
                        return;
                }
            }
            else if (response->statusLine.statusCode == StatusCode::proxyAuthenticationRequired)
            {
                if (!m_proxyAuthorizationTried && (!m_proxyUserName.isEmpty() || !m_proxyUserPassword.isEmpty()))
                {
                    if (resendRequestWithAuthorization(*response))
                        return;
                }
            }

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
                    NX_LOGX(lit("Failed to read (1) response from %1. %2").arg(m_url.toString()).arg(SystemError::getLastOSErrorText()), cl_logDEBUG1);
                    m_state = sFailed;
                    emit done(sharedThis);
                    return;
                }
                m_socket->readSomeAsync(
                    &m_responseBuffer,
                    std::bind(&AsyncHttpClient::onSomeBytesReadAsync, this, sock, _1, _2));
                return;
            }

            //message body has been received with request
            NX_ASSERT(m_httpStreamReader.state() == HttpStreamReader::messageDone || m_httpStreamReader.state() == HttpStreamReader::parseError);

            m_state = m_httpStreamReader.state() == HttpStreamReader::parseError ? sFailed : sDone;
            emit done(sharedThis);
            break;
        }

        case sReadingMessageBody:
        {
            const size_t bytesParsed = readAndParseHttp(bytesRead);
            //TODO #ak reconnect in case of error
            if (bytesParsed > 0)
            {
                emit someMessageBodyAvailable(sharedThis);
                if (m_terminated)
                    break;
                if (m_forcedEof)
                {
                    m_forcedEof = false;
                    return;
                }
            }

            if (m_state != sFailed && m_state != sDone)
            {
                m_responseBuffer.resize(0);
                m_socket->readSomeAsync(
                    &m_responseBuffer,
                    std::bind(&AsyncHttpClient::onSomeBytesReadAsync, this, sock, _1, _2));
                return;
            }

            emit done(sharedThis);
            break;
        }

        default:
        {
            NX_ASSERT(false);
            break;
        }
        }
    }

    void AsyncHttpClient::resetDataBeforeNewRequest()
    {
        ++m_requestSequence;
        m_authorizationTried = false;
        m_ha1RecalcTried = false;
        m_request = nx_http::Request();
    }

    void AsyncHttpClient::initiateHttpMessageDelivery(const QUrl& url)
    {
        using namespace std::placeholders;

        bool canUseExistingConnection = false;
        if (m_httpStreamReader.message().type == nx_http::MessageType::response)
        {
            canUseExistingConnection =
                (m_httpStreamReader.message().response->statusLine.version == nx_http::http_1_1) &&
                (nx_http::getHeaderValue(m_httpStreamReader.message().response->headers, "Connection") != "close");
        }

        m_url = url;
        const SocketAddress remoteEndpoint(url.host(), url.port(nx_http::DEFAULT_HTTP_PORT));

        canUseExistingConnection =
            m_socket &&
            !m_connectionClosed &&
            canUseExistingConnection &&
            (m_remoteEndpoint == remoteEndpoint);

        if (!canUseExistingConnection)
        {
            m_httpStreamReader.resetState();
            m_awaitedMessageNumber = 0;
        }

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

                    m_socket->sendAsync(
                        m_requestBuffer,
                        std::bind(
                            &AsyncHttpClient::asyncSendDone, this,
                            m_socket.get(), _1, _2));
                    return;
                }

                m_socket.reset();

                initiateTcpConnection();
            });
    }

    void AsyncHttpClient::initiateTcpConnection()
    {
        const SocketAddress remoteAddress(m_url.host(), m_url.port(nx_http::DEFAULT_HTTP_PORT));

        m_state = sInit;

        m_socket = SocketFactory::createStreamSocket(/*m_url.scheme() == lit("https")*/);
        m_socket->bindToAioThread(m_aioThreadBinder.getAioThread());
        m_connectionClosed = false;
        if (!m_socket->setNonBlockingMode(true) ||
            !m_socket->setSendTimeout(m_sendTimeoutMs) ||
            !m_socket->setRecvTimeout(m_responseReadTimeoutMs))
        {
            m_socket->post(std::bind(
                &AsyncHttpClient::asyncConnectDone,
                this,
                m_socket.get(),
                SystemError::getLastOSErrorCode()));
            return;
        }

        m_state = sWaitingConnectToHost;

        //starting async connect
        m_socket->connectAsync(
            remoteAddress,
            std::bind(&AsyncHttpClient::asyncConnectDone, this, m_socket.get(), std::placeholders::_1));
    }

    size_t AsyncHttpClient::readAndParseHttp(size_t bytesRead)
    {
        if (bytesRead == 0)   //connection closed
        {
            //closing connection is a valid HTTP way to signal message end
            //m_state = m_httpStreamReader.state() == HttpStreamReader::messageDone ? sDone : sFailed;
            //TODO #ak check if whole message body is received (if message body size is known)
            m_httpStreamReader.flush();
            m_state = (m_httpStreamReader.state() == HttpStreamReader::messageDone) ||
                (m_httpStreamReader.state() == HttpStreamReader::pullingLineEndingBeforeMessageBody) ||
                (m_httpStreamReader.state() == HttpStreamReader::readingMessageBody)
                ? sDone
                : sFailed;
            m_connectionClosed = true;
            return 0;
        }

        m_totalBytesRead += bytesRead;

        //TODO #ak m_httpStreamReader is allowed to process not all bytes in m_responseBuffer. MUST support this!

        if (!m_httpStreamReader.parseBytes(m_responseBuffer, bytesRead))
        {
            NX_LOGX(lit("Error parsing http response from %1. %2").
                arg(m_url.toString()).arg(m_httpStreamReader.errorText()), cl_logDEBUG1);
            m_state = sFailed;
            return -1;
        }

        if (m_httpStreamReader.state() == HttpStreamReader::parseError)
        {
            m_state = sFailed;
            return bytesRead;
        }

        NX_ASSERT(m_httpStreamReader.currentMessageNumber() <= m_awaitedMessageNumber);
        if (m_httpStreamReader.currentMessageNumber() < m_awaitedMessageNumber)
            return bytesRead;   //reading some old message, not changing state in this case

        if (m_httpStreamReader.state() == HttpStreamReader::messageDone)
            m_state = sDone;
        return bytesRead;
    }

    void AsyncHttpClient::composeRequest(const nx_http::StringType& httpMethod)
    {
        const bool useHttp11 = true;   //TODO #ak check if we need it (e.g. we using keep-alive or requesting live capture)

        m_request.requestLine.method = httpMethod;
        m_request.requestLine.url = m_url.path() + (m_url.hasQuery() ? (QLatin1String("?") + m_url.query()) : QString());
        m_request.requestLine.version = useHttp11 ? nx_http::http_1_1 : nx_http::http_1_0;

        nx_http::insertOrReplaceHeader(
            &m_request.headers,
            HttpHeader("Date", dateTimeToHTTPFormat(QDateTime::currentDateTime())));
        m_request.headers.emplace(
            "User-Agent",
            m_userAgent.isEmpty() ? nx_http::userAgentString() : m_userAgent.toLatin1());
        if (useHttp11)
        {
            if (httpMethod == nx_http::Method::GET || httpMethod == nx_http::Method::HEAD)
            {
                //m_request.headers.insert( std::make_pair("Accept", "*/*") );
                if (m_contentEncodingUsed)
                    m_request.headers.insert(std::make_pair("Accept-Encoding", "gzip"));
                //else
                //    m_request.headers.insert( std::make_pair("Accept-Encoding", "identity;q=1.0, *;q=0") );
            }
            //m_request.headers.insert( std::make_pair("Cache-Control", "max-age=0") );
            m_request.headers.insert(std::make_pair("Connection", "keep-alive"));
            m_request.headers.insert(std::make_pair("Host", m_url.host().toLatin1()));
        }

        m_request.headers.insert(m_additionalHeaders.cbegin(), m_additionalHeaders.cend());

        //adding user credentials
        if (!m_url.userName().isEmpty())
            m_userName = m_url.userName();
        if (!m_url.password().isEmpty())
            m_userPassword = m_url.password();
        m_url.setUserName(m_userName);
        m_url.setPassword(m_userPassword);

        if (!m_effectiveUserName.isEmpty())
        {
            nx_http::insertOrReplaceHeader(
                &m_request.headers,
                HttpHeader(Qn::EFFECTIVE_USER_NAME_HEADER_NAME, m_effectiveUserName.toUtf8()));
        }

        //adding X-Nx-User-Name to help server to port data from 2.1 to 2.3 and from 2.3 to 2.4 (generate user's digest)
        //TODO #ak remove it after 2.3 support is over
        if (!m_userName.isEmpty())
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
                m_url,
                &m_request,
                &m_authCacheItem))
        {
            //not using Basic authentication by default, since it is not secure
            nx_http::removeHeader(&m_request.headers, header::Authorization::NAME);
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
        //TODO #ak we need reconnect and request entity from the point we stopped at
        return false;
    }

    AsyncHttpClientPtr AsyncHttpClient::create()
    {
        return AsyncHttpClientPtr(std::shared_ptr<AsyncHttpClient>(new AsyncHttpClient()));
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
        NX_ASSERT(response.statusLine.statusCode == StatusCode::unauthorized);

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
                m_url,
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
                    m_userName.toUtf8(),
                    m_authType != authDigestWithPasswordHash
                        ? m_userPassword.toUtf8()
                        : boost::optional<nx_http::BufferType>(),
                    m_authType == authDigestWithPasswordHash
                        ? m_userPassword.toLatin1()
                        : boost::optional<nx_http::BufferType>(),
                    m_url.path().toUtf8(),
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
                m_url,
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
        initiateHttpMessageDelivery(m_url);
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


    /**********************************************************
    * utils
    ***********************************************************/

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

}
