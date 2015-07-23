/**********************************************************
* 22 nov 2012
* a.kolesnikov
***********************************************************/

#include "asynchttpclient.h"

#include <condition_variable>
#include <mutex>

#include <QtCore/QCryptographicHash>
#include <utils/thread/mutex.h>

#include <http/custom_headers.h>
#include <utils/common/log.h>
#include <utils/common/util.h>
#include <utils/network/socket_factory.h>
#include <utils/common/systemerror.h>

#include "auth_tools.h"
#include "version.h"

//TODO: #ak persistent connection support
//TODO: #ak MUST call cancelAsyncIO with 1st parameter set to false
//TODO: #ak reconnect support

static const int DEFAULT_SEND_TIMEOUT = 3000;
static const int DEFAULT_RESPONSE_READ_TIMEOUT = 3000;
//static const int DEFAULT_HTTP_PORT = 80;

using std::make_pair;

namespace nx_http
{
    static const size_t RESPONSE_BUFFER_SIZE = 16*1024;

    AsyncHttpClient::AsyncHttpClient()
    :
        m_state( sInit ),
        m_connectionClosed( false ),
        m_requestBytesSent( 0 ),
        m_authorizationTried( false ),
        m_terminated( false ),
        m_totalBytesRead( 0 ),
        m_contentEncodingUsed( true ),
        m_sendTimeoutMs( DEFAULT_SEND_TIMEOUT ),
        m_responseReadTimeoutMs( DEFAULT_RESPONSE_READ_TIMEOUT ),
        m_msgBodyReadTimeoutMs( 0 ),
        m_authType(authBasicAndDigest),
        m_awaitedMessageNumber( 0 )
    {
        m_responseBuffer.reserve(RESPONSE_BUFFER_SIZE);
    }

    AsyncHttpClient::~AsyncHttpClient()
    {
        terminate();
    }

    QSharedPointer<AbstractStreamSocket> AsyncHttpClient::takeSocket()
    {
        QSharedPointer<AbstractStreamSocket> result = m_socket;

        {
            QnMutexLocker lk( &m_mutex );
            m_terminated = true;
        }
        //after we set m_terminated to true with m_mutex locked socket event processing is stopped and m_socket cannot change its value
        if( result )
            result->cancelAsyncIO();
        //AIOService guarantees that eventTriggered had returned and will never be called with m_socket

        m_socket.clear();
        return result;
    }

    void AsyncHttpClient::terminate()
    {
        {
            QnMutexLocker lk( &m_mutex );
            if( m_terminated )
                return;
            m_terminated = true;
        }
        //after we set m_terminated to true with m_mutex locked socket event processing is stopped and m_socket cannot change its value
        if( m_socket )
            m_socket->terminateAsyncIO( true );
        //AIOService guarantees that eventTriggered had returned and will never be called with m_socket
    }

    AsyncHttpClient::State AsyncHttpClient::state() const
    {
        return m_state;
    }

    bool AsyncHttpClient::failed() const
    {
        return m_state == sFailed;
    }

    //!Start request to \a url
    /*!
        \return true, if socket is created and async connect is started. false otherwise
        To get error description use SystemError::getLastOSErrorCode()
    */
    bool AsyncHttpClient::doGet( const QUrl& url )
    {
        if( !url.isValid() )
            return false;

        resetDataBeforeNewRequest();
        m_url = url;
        composeRequest( nx_http::Method::GET );
        return initiateHttpMessageDelivery( url );
    }

    bool AsyncHttpClient::doPost(
        const QUrl& url,
        const nx_http::StringType& contentType,
        const nx_http::StringType& messageBody)
    {
        if( !url.isValid() )
            return false;

        resetDataBeforeNewRequest();
        m_url = url;
        composeRequest( nx_http::Method::POST );
        m_request.headers.insert( make_pair("Content-Type", contentType) );
        m_request.headers.insert( make_pair("Content-Length", StringType::number(messageBody.size())) );
        //TODO #ak support chunked encoding & compression
        m_request.headers.insert( make_pair("Content-Encoding", "identity") );
        m_request.messageBody = messageBody;
        return initiateHttpMessageDelivery( url );
    }

    bool AsyncHttpClient::doPut(
        const QUrl& url,
        const nx_http::StringType& contentType,
        const nx_http::StringType& messageBody )
    {
        if( !url.isValid() )
            return false;

        resetDataBeforeNewRequest();
        m_url = url;
        composeRequest( nx_http::Method::PUT );
        m_request.headers.insert( make_pair("Content-Type", contentType) );
        m_request.headers.insert( make_pair("Content-Length", StringType::number(messageBody.size())) );
        //TODO #ak support chunked encoding & compression
        m_request.messageBody = messageBody;
        return initiateHttpMessageDelivery( url );
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
        if( httpMsg.type == MessageType::none )
            return StringType();
        HttpHeaders::const_iterator contentTypeIter = httpMsg.headers().find( "Content-Type" );
        if( contentTypeIter == httpMsg.headers().end() )
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
        QnMutexLocker lk( &m_mutex );
        return m_totalBytesRead;
    }

    void AsyncHttpClient::setUseCompression( bool toggleUseEntityEncoding )
    {
        m_contentEncodingUsed = toggleUseEntityEncoding;
    }

    void AsyncHttpClient::setSubsequentReconnectTries( int /*reconnectTries*/ )
    {
        //TODO #ak
    }

    void AsyncHttpClient::setTotalReconnectTries( int /*reconnectTries*/ )
    {
        //TODO #ak
    }

    void AsyncHttpClient::setUserAgent( const QString& userAgent )
    {
        m_userAgent = userAgent;
    }

    void AsyncHttpClient::setUserName( const QString& userName )
    {
        m_userName = userName;
    }

    void AsyncHttpClient::setUserPassword( const QString& userPassword )
    {
        m_userPassword = userPassword;
    }

    void AsyncHttpClient::setSendTimeoutMs( unsigned int sendTimeoutMs )
    {
        m_sendTimeoutMs = sendTimeoutMs;
    }

    void AsyncHttpClient::setResponseReadTimeoutMs( unsigned int _responseReadTimeoutMs )
    {
        m_responseReadTimeoutMs = _responseReadTimeoutMs;
    }

    void AsyncHttpClient::setMessageBodyReadTimeoutMs( unsigned int messageBodyReadTimeoutMs )
    {
        m_msgBodyReadTimeoutMs = messageBodyReadTimeoutMs;
    }

    void AsyncHttpClient::asyncConnectDone( AbstractSocket* sock, SystemError::ErrorCode errorCode )
    {
        std::shared_ptr<AsyncHttpClient> sharedThis( shared_from_this() );

        QnMutexLocker lk( &m_mutex );
        if( m_terminated )
            return;

        Q_ASSERT( sock == m_socket.data() );

        if( m_state != sWaitingConnectToHost )
        {
            Q_ASSERT( false );
            return;
        }

        if( errorCode == SystemError::noError )
        {
            //connect successful
            m_remoteEndpoint = SocketAddress( m_url.host(), m_url.port(nx_http::DEFAULT_HTTP_PORT) );
            serializeRequest();
            m_state = sSendingRequest;
            lk.unlock();
            emit tcpConnectionEstablished( sharedThis );
            lk.relock();
            using namespace std::placeholders;
            if( m_socket->sendAsync( m_requestBuffer, std::bind( &AsyncHttpClient::asyncSendDone, this, sock, _1, _2 ) ) )
                return;

            NX_LOG( lit( "Failed to send request to %1. %2" ).arg( m_url.toString() ).arg( SystemError::getLastOSErrorText() ), cl_logDEBUG1 );
        }
        else
        {
            NX_LOG( lit( "AsyncHttpClient. Failed to establish tcp connection to %1. %2" ).
                arg( m_url.toString() ).arg( SystemError::toString( errorCode ) ), cl_logDEBUG1 );
            if( reconnectIfAppropriate() )
                return;
        }

        m_state = sFailed;
        lk.unlock();
        emit done( sharedThis );
        lk.relock();
    }

    void AsyncHttpClient::asyncSendDone( AbstractSocket* sock, SystemError::ErrorCode errorCode, size_t bytesWritten )
    {
        std::shared_ptr<AsyncHttpClient> sharedThis( shared_from_this() );

        QnMutexLocker lk( &m_mutex );
        if( m_terminated )
            return;

        Q_ASSERT( sock == m_socket.data() );

        if( m_state != sSendingRequest )
        {
            Q_ASSERT( false );
            return;
        }

        if( errorCode != SystemError::noError )
        {
            if( reconnectIfAppropriate() )
                return;
            NX_LOG( lit( "Error sending (1) http request to %1. %2" ).arg( m_url.toString() ).arg( SystemError::toString( errorCode ) ), cl_logDEBUG1 );
            m_state = sFailed;
            lk.unlock();
            emit done( sharedThis );
            lk.relock();
            return;
        }

        //TODO #ak recheck connection closure situation

        using namespace std::placeholders;

        m_requestBytesSent += bytesWritten;
        if( (int)m_requestBytesSent < m_requestBuffer.size() )
        {
            if( !m_socket->sendAsync( m_requestBuffer, std::bind( &AsyncHttpClient::asyncSendDone, this, sock, _1, _2 ) ) )
            {
                NX_LOG( lit( "Error sending (2) http request to %1. %2" ).arg( m_url.toString() ).arg( SystemError::getLastOSErrorText() ), cl_logDEBUG1 );
                m_state = sFailed;
                lk.unlock();
                emit done( sharedThis );
                lk.relock();
            }
            return;
        }

        NX_LOG( lit( "Http request has been successfully sent to %1" ).arg( m_url.toString() ), cl_logDEBUG2 );
        m_state = sReceivingResponse;
        m_responseBuffer.resize( 0 );
        if( !m_socket->setRecvTimeout( m_responseReadTimeoutMs ) ||
            !m_socket->readSomeAsync( &m_responseBuffer, std::bind( &AsyncHttpClient::onSomeBytesReadAsync, this, sock, _1, _2 ) ) )
        {
            NX_LOG( lit( "Error reading (1) http response from %1. %2" ).arg( m_url.toString() ).arg( SystemError::getLastOSErrorText() ), cl_logDEBUG1 );
            m_state = sFailed;
            lk.unlock();
            emit done( sharedThis );
            lk.relock();
        }
    }

    void AsyncHttpClient::onSomeBytesReadAsync( AbstractSocket* sock, SystemError::ErrorCode errorCode, size_t bytesRead )
    {
        using namespace std::placeholders;

        std::shared_ptr<AsyncHttpClient> sharedThis( shared_from_this() );

        QnMutexLocker lk( &m_mutex );
        if( m_terminated )
            return;

        Q_ASSERT( sock == m_socket.data() );

        if( errorCode != SystemError::noError )
        {
            if( reconnectIfAppropriate() )
                return;
            NX_LOG(lit("Error reading (state %1) http response from %2. %3").arg( m_state ).arg( m_url.toString() ).arg( SystemError::toString( errorCode ) ), cl_logDEBUG1 );
            m_state = 
                ((m_httpStreamReader.state() == HttpStreamReader::messageDone) &&
                    m_httpStreamReader.currentMessageNumber() == m_awaitedMessageNumber)
                ? sDone
                : sFailed;
            lk.unlock();
            emit done( sharedThis );
            lk.relock();
            return;
        }

        switch( m_state )
        {
            case sReceivingResponse:
            {
                readAndParseHttp( bytesRead );
                //TODO/IMPL reconnect in case of error

                if( m_state == sFailed )
                {
                    lk.unlock();
                    emit done( sharedThis );
                    lk.relock();
                    break;
                }

                //connection could be closed by remote peer already

                if( m_httpStreamReader.currentMessageNumber() < m_awaitedMessageNumber ||       //still reading previous message
                    m_httpStreamReader.state() <= HttpStreamReader::readingMessageHeaders )     //still reading message headers
                {
                    //response has not been read yet, reading futher
                    m_responseBuffer.resize( 0 );
                    if( m_connectionClosed ||
                        !m_socket->readSomeAsync(
                            &m_responseBuffer,
                            std::bind( &AsyncHttpClient::onSomeBytesReadAsync, this, sock, _1, _2 ) ) )
                    {
                        NX_LOG( lit( "Failed to read (1) response from %1. %2" ).
                            arg( m_url.toString() ).arg( SystemError::getLastOSErrorText() ), cl_logDEBUG1 );
                        m_state = sFailed;
                        lk.unlock();
                        emit done( sharedThis );
                        lk.relock();
                    }
                    return;
                }

                //read http message headers
                if( m_httpStreamReader.message().type != nx_http::MessageType::response )
                {
                    NX_LOG( lit( "Unexpectedly received request from %1:%2 while expecting response! Ignoring..." ).
                        arg( m_url.host() ).arg( m_url.port() ), cl_logDEBUG1 );
                    m_state = sFailed;
                    lk.unlock();
                    emit done( sharedThis );
                    lk.relock();
                    break;
                }

                //response read
                NX_LOG( lit( "Http response from %1 has been successfully read. Status line: %2(%3)" ).
                    arg( m_url.toString() ).arg( m_httpStreamReader.message().response->statusLine.statusCode ).
                    arg( QLatin1String( m_httpStreamReader.message().response->statusLine.reasonPhrase ) ), cl_logDEBUG2 );

                const Response* response = m_httpStreamReader.message().response;
                if( response->statusLine.statusCode == StatusCode::unauthorized )
                {
                    if( !m_authorizationTried && (!m_userName.isEmpty() || !m_userPassword.isEmpty()) )
                    {
                        //trying authorization
                        if( resendRequestWithAuthorization( *response ) )
                            return;
                    }
                }

                const bool messageHasMessageBody =
                    (m_httpStreamReader.state() == HttpStreamReader::readingMessageBody) ||
                    (m_httpStreamReader.state() == HttpStreamReader::pullingLineEndingBeforeMessageBody) ||
                    (m_httpStreamReader.messageBodyBufferSize() > 0);

                m_state = sResponseReceived;
                lk.unlock();
                emit responseReceived( sharedThis );
                lk.relock();
                if( m_terminated )
                    break;

                //does message body follow?
                if( !messageHasMessageBody )
                {
                    //no message body: done
                    m_state = m_httpStreamReader.state() == HttpStreamReader::parseError ? sFailed : sDone;
                    lk.unlock();
                    emit done( sharedThis );
                    lk.relock();
                    break;
                }

                //starting reading message body
                m_state = sReadingMessageBody;

                if( m_httpStreamReader.messageBodyBufferSize() > 0 &&   //some message body has been read
                    m_state == sReadingMessageBody )                    //client wants to read message body
                {
                    lk.unlock();
                    emit someMessageBodyAvailable( sharedThis );
                    lk.relock();
                    if( m_terminated )
                        break;
                }

                if( ((m_httpStreamReader.state() == HttpStreamReader::readingMessageBody) ||
                     (m_httpStreamReader.state() == HttpStreamReader::pullingLineEndingBeforeMessageBody)) &&
                    (!m_connectionClosed) )
                {
                    //reading more data
                    m_responseBuffer.resize( 0 );
                    if( !m_socket->setRecvTimeout( m_msgBodyReadTimeoutMs ) ||
                        !m_socket->readSomeAsync( &m_responseBuffer, std::bind( &AsyncHttpClient::onSomeBytesReadAsync, this, sock, _1, _2 ) ) )
                    {
                        NX_LOG( lit( "Failed to read (1) response from %1. %2" ).arg( m_url.toString() ).arg( SystemError::getLastOSErrorText() ), cl_logDEBUG1 );
                        m_state = sFailed;
                        lk.unlock();
                        emit done( sharedThis );
                        lk.relock();
                    }
                    return;
                }

                //message body has been received with request
                assert( m_httpStreamReader.state() == HttpStreamReader::messageDone || m_httpStreamReader.state() == HttpStreamReader::parseError );

                m_state = m_httpStreamReader.state() == HttpStreamReader::parseError ? sFailed : sDone;
                lk.unlock();
                emit done( sharedThis );
                lk.relock();
                break;
            }

            case sReadingMessageBody:
            {
                const size_t bytesParsed = readAndParseHttp( bytesRead );
                //TODO #ak reconnect in case of error
                if( bytesParsed > 0 )
                {
                    lk.unlock();
                    emit someMessageBodyAvailable( sharedThis );
                    lk.relock();
                    if( m_terminated )
                        break;
                }

                if( m_state != sFailed && m_state != sDone )
                {
                    m_responseBuffer.resize( 0 );
                    if( m_socket->readSomeAsync( &m_responseBuffer, std::bind( &AsyncHttpClient::onSomeBytesReadAsync, this, sock, _1, _2 ) ) )
                    {
                        return;
                    }
                    else
                    {
                        NX_LOG( lit( "Failed to read (2) response from %1. %2" ).arg( m_url.toString() ).arg( SystemError::getLastOSErrorText() ), cl_logDEBUG1 );
                        m_state = sFailed;
                    }
                }

                lk.unlock();
                emit done( sharedThis );
                lk.relock();
                break;
            }

            default:
            {
                Q_ASSERT( false );
                break;
            }
        }
    }

    void AsyncHttpClient::resetDataBeforeNewRequest()
    {
        //stopping client, if it is running
        {
            QnMutexLocker lk( &m_mutex );
            m_terminated = true;
        }
        //after we set m_terminated to true with m_mutex locked socket event processing is stopped and m_socket cannot change its value
        if( m_socket )
            m_socket->cancelAsyncIO();

        {
            QnMutexLocker lk( &m_mutex );
            m_terminated = false;
        }

        m_authorizationTried = false;
        m_request = nx_http::Request();
    }

    bool AsyncHttpClient::initiateHttpMessageDelivery( const QUrl& url )
    {
        using namespace std::placeholders;

        bool canUseExistingConnection = false;
        if( m_httpStreamReader.message().type == nx_http::MessageType::response )
        {
            canUseExistingConnection =
                (m_httpStreamReader.message().response->statusLine.version == nx_http::http_1_1) &&
                (nx_http::getHeaderValue( m_httpStreamReader.message().response->headers, "Connection" ) != "close");
        }

        m_url = url;
        const SocketAddress remoteEndpoint( url.host(), url.port(nx_http::DEFAULT_HTTP_PORT) );

        if( m_socket )
        {
            //TODO #ak think again about next cancellation
            m_socket->cancelAsyncIO();

            if( !m_connectionClosed &&
                canUseExistingConnection &&
                (m_remoteEndpoint == remoteEndpoint) )  //m_socket->getForeignAddress() returns ip address only, not host name
            {
                ++m_awaitedMessageNumber;   //current message will be skipped

                serializeRequest();
                m_state = sSendingRequest;

                if( !m_socket->sendAsync(
                        m_requestBuffer,
                        std::bind( &AsyncHttpClient::asyncSendDone, this, m_socket.data(), _1, _2 ) ) )
                {
                    NX_LOG( lit("Failed to init async socket call (connecting to %1) to aio service. %2").
                        arg(remoteEndpoint.toString()).arg(SystemError::toString(SystemError::getLastOSErrorCode())), cl_logDEBUG1 );
                    m_socket.clear();
                    return false;
                }
                return true;
            }
            else
            {
                m_socket.clear();
            }
        }

        //resetting parser state only if establishing new tcp connection
        m_httpStreamReader.resetState();
        m_awaitedMessageNumber = 0;

        return initiateTcpConnection();
    }

    bool AsyncHttpClient::initiateTcpConnection()
    {
        const SocketAddress remoteAddress( m_url.host(), m_url.port( nx_http::DEFAULT_HTTP_PORT ) );

        m_state = sInit;

        m_socket = QSharedPointer<AbstractStreamSocket>( SocketFactory::createStreamSocket(m_url.scheme() == lit("https")));
        m_connectionClosed = false;
        if( !m_socket->setNonBlockingMode( true ) ||
            !m_socket->setSendTimeout( m_sendTimeoutMs ) ||
            !m_socket->setRecvTimeout( m_responseReadTimeoutMs ) )
        {
            NX_LOG( lit("Failed to put socket to non blocking mode. %1").
                arg(SystemError::toString(SystemError::getLastOSErrorCode())), cl_logDEBUG1 );
            m_socket.clear();
            return false;
        }

        m_state = sWaitingConnectToHost;

        //starting async connect
        if( !m_socket->connectAsync(
                remoteAddress,
                std::bind( &AsyncHttpClient::asyncConnectDone, this, m_socket.data(), std::placeholders::_1 ) ) )
        {
            NX_LOG( lit("Failed to perform async connect to %1. %2").
                arg(remoteAddress.toString()).arg(SystemError::toString(SystemError::getLastOSErrorCode())), cl_logDEBUG1 );
            m_socket.clear();
            m_url.clear();
            m_state = sInit;
            return false;
        }

        return true;
    }

    size_t AsyncHttpClient::readAndParseHttp( size_t bytesRead )
    {
        if( bytesRead == 0 )   //connection closed
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

        if( !m_httpStreamReader.parseBytes( m_responseBuffer, bytesRead ) )
        {
            NX_LOG( lit("Error parsing http response from %1. %2").
                arg(m_url.toString()).arg(m_httpStreamReader.errorText()), cl_logDEBUG1 );
            m_state = sFailed;
            return -1;
        }

        if( m_httpStreamReader.state() == HttpStreamReader::parseError )
        {
            m_state = sFailed;
            return bytesRead;
        }

        Q_ASSERT( m_httpStreamReader.currentMessageNumber() <= m_awaitedMessageNumber );
        if( m_httpStreamReader.currentMessageNumber() < m_awaitedMessageNumber )
            return bytesRead;   //reading some old message, not changing state in this case

        if( m_httpStreamReader.state() == HttpStreamReader::messageDone )
            m_state = sDone;
        return bytesRead;
    }

    void AsyncHttpClient::composeRequest( const nx_http::StringType& httpMethod )
    {
        const bool useHttp11 = true;   //TODO #ak check if we need it (e.g. we using keep-alive or requesting live capture)

        m_request.requestLine.method = httpMethod;
        m_request.requestLine.url = m_url.path() + (m_url.hasQuery() ? (QLatin1String("?") + m_url.query()) : QString());
        m_request.requestLine.version = useHttp11 ? nx_http::http_1_1 : nx_http::http_1_0;

        nx_http::insertOrReplaceHeader(
            &m_request.headers,
            HttpHeader("Date", dateTimeToHTTPFormat(QDateTime::currentDateTime())) );
        m_request.headers.emplace(
            "User-Agent",
            m_userAgent.isEmpty() ? nx_http::userAgentString() : m_userAgent.toLatin1() );
        if( useHttp11 )
        {
            if( httpMethod == nx_http::Method::GET || httpMethod == nx_http::Method::HEAD )
            {
                //m_request.headers.insert( std::make_pair("Accept", "*/*") );
                if( m_contentEncodingUsed )
                    m_request.headers.insert( std::make_pair("Accept-Encoding", "gzip") );
                //else
                //    m_request.headers.insert( std::make_pair("Accept-Encoding", "identity;q=1.0, *;q=0") );
            }
            //m_request.headers.insert( std::make_pair("Cache-Control", "max-age=0") );
            m_request.headers.insert( std::make_pair("Connection", "keep-alive") );
            m_request.headers.insert( std::make_pair("Host", m_url.host().toLatin1()) );
        }

        m_request.headers.insert(m_additionalHeaders.cbegin(), m_additionalHeaders.cend());

        //adding user credentials
        if( !m_url.userName().isEmpty() )
            m_userName = m_url.userName();
        if( !m_url.password().isEmpty() )
            m_userPassword = m_url.password();

        //adding X-Nx-User-Name to help server to port data from 2.1 to 2.3 and from 2.3 to 2.4 (generate user's digest)
        //TODO #ak remove it after 2.3 support is over
        if( !m_userName.isEmpty() )
            nx_http::insertOrReplaceHeader(
                &m_request.headers,
                HttpHeader(Qn::CUSTOM_USERNAME_HEADER_NAME, m_userName.toUtf8()) );

        //if that url has already been authenticated, adding same authentication info to the request
        //first request per tcp-connection always uses authentication
        //    This is done due to limited AuthInfoCache implementation
        if( m_authCacheItem.url.isEmpty() ||
            !AuthInfoCache::instance()->addAuthorizationHeader(
                m_url,
                &m_request,
                &m_authCacheItem ) )
        {
            //not using Basic authentication by default, since it is not secure
            nx_http::removeHeader(&m_request.headers, header::Authorization::NAME);
        }
    }

    void AsyncHttpClient::addAdditionalHeader(const StringType& key, const StringType& value)
    {
        m_additionalHeaders.emplace( key, value );
    }

    void AsyncHttpClient::removeAdditionalHeader( const StringType& key )
    {
        m_additionalHeaders.erase( key );
    }

    void AsyncHttpClient::addRequestHeaders(const HttpHeaders& headers)
    {
        for (HttpHeaders::const_iterator itr = headers.begin(); itr != headers.end(); ++itr)
            m_additionalHeaders.emplace( itr->first, itr->second);
    }

    void AsyncHttpClient::serializeRequest()
    {
        m_requestBuffer.clear();
        m_request.serialize( &m_requestBuffer );
        m_requestBytesSent = 0;
    }

    bool AsyncHttpClient::reconnectIfAppropriate()
    {
        //TODO #ak we need reconnect and request entity from the point we stopped at
        return false;
    }

    AsyncHttpClientPtr AsyncHttpClient::create()
    {
        return AsyncHttpClientPtr( std::shared_ptr<AsyncHttpClient>( new AsyncHttpClient() ) );
    }

    bool AsyncHttpClient::resendRequestWithAuthorization( const nx_http::Response& response )
    {
        //if response contains WWW-Authenticate with Digest authentication, generating "Authorization: Digest" header and adding it to custom headers
        Q_ASSERT( response.statusLine.statusCode == StatusCode::unauthorized );

        HttpHeaders::const_iterator wwwAuthenticateIter = response.headers.find( "WWW-Authenticate" );
        if( wwwAuthenticateIter == response.headers.end() )
            return false;

        header::WWWAuthenticate wwwAuthenticateHeader;
        wwwAuthenticateHeader.parse( wwwAuthenticateIter->second );
        if( wwwAuthenticateHeader.authScheme == header::AuthScheme::basic )
        {
            header::BasicAuthorization basicAuthorization( m_userName.toLatin1(), m_userPassword.toLatin1() );
            nx_http::insertOrReplaceHeader(
                &m_request.headers,
                nx_http::HttpHeader(
                    header::Authorization::NAME,
                    basicAuthorization.serialized() ) );
            //TODO #ak MUST add to cache only after OK response
            m_authCacheItem = AuthInfoCache::AuthorizationCacheItem(
                m_url,
                m_request.requestLine.method,
                m_userName.toLatin1(),
                m_userPassword.toLatin1(),
                boost::optional<BufferType>(),
                std::move(wwwAuthenticateHeader),
                std::move(basicAuthorization) );
            AuthInfoCache::instance()->cacheAuthorization( m_authCacheItem );
        }
        else if( wwwAuthenticateHeader.authScheme == header::AuthScheme::digest )
        {
            header::DigestAuthorization digestAuthorizationHeader;
            if( !calcDigestResponse(
                    m_request.requestLine.method,
                    m_userName.toUtf8(),
                    m_authType != authDigestWithPasswordHash
                        ? m_userPassword.toUtf8()
                        : boost::optional<nx_http::BufferType>(),
                    m_authType == authDigestWithPasswordHash
                        ? m_userPassword.toLatin1()
                        : boost::optional<nx_http::BufferType>(),
                    m_url,
                    wwwAuthenticateHeader,
                    &digestAuthorizationHeader ) )
            {
                return false;
            }
            BufferType authorizationStr;
            digestAuthorizationHeader.serialize( &authorizationStr );

            nx_http::insertOrReplaceHeader(
                &m_request.headers,
                nx_http::HttpHeader( header::Authorization::NAME, authorizationStr ) );
            //TODO #ak MUST add to cache only after OK response
            m_authCacheItem = AuthInfoCache::AuthorizationCacheItem(
                m_url,
                m_request.requestLine.method,
                m_userName.toLatin1(),
                m_authType == authDigestWithPasswordHash
                    ? boost::optional<BufferType>()
                    : boost::optional<BufferType>(m_userPassword.toLatin1()),
                m_authType == authDigestWithPasswordHash
                    ? boost::optional<BufferType>(m_userPassword.toLatin1())
                    : boost::optional<BufferType>(),
                std::move(wwwAuthenticateHeader),
                std::move(digestAuthorizationHeader) );
            AuthInfoCache::instance()->cacheAuthorization( m_authCacheItem );
        }
        else
        {
            return false;
        }

        m_authorizationTried = true;
        return initiateHttpMessageDelivery( m_url );
    }

    const char* AsyncHttpClient::toString( State state )
    {
        switch( state )
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


    /**********************************************************
    * utils
    ***********************************************************/

    bool downloadFileAsync(
        const QUrl& url,
        std::function<void(SystemError::ErrorCode, int, nx_http::BufferType)> completionHandler,
        const nx_http::HttpHeaders& extraHeaders,
        const QAuthenticator &auth)
    {
        nx_http::AsyncHttpClientPtr httpClientCaptured = nx_http::AsyncHttpClient::create();
        httpClientCaptured->addRequestHeaders(extraHeaders);
        if (!auth.isNull()) {
            httpClientCaptured->setUserName(auth.user());
            httpClientCaptured->setUserPassword(auth.password());
            httpClientCaptured->setAuthType(nx_http::AsyncHttpClient::authDigestWithPasswordHash);
        }

        auto requestCompletionFunc = [httpClientCaptured, completionHandler]
            ( nx_http::AsyncHttpClientPtr httpClient ) mutable
        {
            httpClientCaptured->disconnect( nullptr, (const char*)nullptr );
            httpClientCaptured.reset();

            if( httpClient->failed() )
                return completionHandler(
                    SystemError::connectionReset,
                    nx_http::StatusCode::ok,
                    nx_http::BufferType() );

            if( httpClient->response()->statusLine.statusCode != nx_http::StatusCode::ok &&
                httpClient->response()->statusLine.statusCode != nx_http::StatusCode::partialContent )
            {
                return completionHandler(
                    SystemError::noError,
                    httpClient->response()->statusLine.statusCode,
                    nx_http::BufferType() );
            }

            completionHandler( 
                SystemError::noError,
                httpClient->response()->statusLine.statusCode,
                httpClient->fetchMessageBodyBuffer() );
        };
        QObject::connect(
            httpClientCaptured.get(), &nx_http::AsyncHttpClient::done,
            httpClientCaptured.get(), requestCompletionFunc,
            Qt::DirectConnection );

        if( !httpClientCaptured->doGet( url ) )
        {
            //if we do not disconnect, http client object will not be destroyed
            httpClientCaptured->disconnect( nullptr, (const char*)nullptr );
            return false;
        }
        return true;
    }

    SystemError::ErrorCode downloadFileSync(
        const QUrl& url,
        int* const statusCode,
        nx_http::BufferType* const msgBody )
    {
        bool done = false;
        SystemError::ErrorCode resultingErrorCode = SystemError::noError;
        std::mutex mtx;
        std::condition_variable condVar;
        const bool res = downloadFileAsync(
            url,
            [&resultingErrorCode, statusCode, msgBody, &mtx, &condVar, &done]
            ( SystemError::ErrorCode errorCode, int _statusCode, const nx_http::BufferType& _msgBody ) {
                resultingErrorCode = errorCode;
                *statusCode = _statusCode;
                *msgBody = _msgBody;
                std::unique_lock<std::mutex> lk( mtx );
                done = true;
                condVar.notify_all();
            } );
        if( !res )
            return SystemError::getLastOSErrorCode();

        std::unique_lock<std::mutex> lk( mtx );
        while( !done )
            condVar.wait( lk );

        return resultingErrorCode;
    }
}
