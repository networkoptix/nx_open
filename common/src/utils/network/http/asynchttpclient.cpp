/**********************************************************
* 22 nov 2012
* a.kolesnikov
***********************************************************/

#include "asynchttpclient.h"

#include <condition_variable>
#include <mutex>

#include <QtCore/QCryptographicHash>
#include <QtCore/QMutexLocker>

#include "utils/network/socket_factory.h"
#include "../../common/log.h"
#include "../../common/systemerror.h"


//TODO: #ak persistent connection support
//TODO: #ak MUST call cancelAsyncIO with 1st parameter set to false
//TODO: #ak reconnect support

static const int DEFAULT_CONNECT_TIMEOUT = 3000;
static const int DEFAULT_RESPONSE_READ_TIMEOUT = 3000;
static const int DEFAULT_HTTP_PORT = 80;

using std::make_pair;

namespace nx_http
{
    static const size_t RESPONSE_BUFFER_SIZE = 16*1024;

    AsyncHttpClient::AsyncHttpClient()
    :
        m_state( sInit ),
        m_requestBytesSent( 0 ),
        m_authorizationTried( false ),
        m_terminated( false ),
        m_totalBytesRead( 0 ),
        m_contentEncodingUsed( true ),
        m_responseReadTimeoutMs( DEFAULT_RESPONSE_READ_TIMEOUT ),
        m_msgBodyReadTimeoutMs( 0 ),
        m_authType(authBasicAndDigest)
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
            QMutexLocker lk( &m_mutex );
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
            QMutexLocker lk( &m_mutex );
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
        resetDataBeforeNewRequest();
        m_url = url;
        composeRequest( nx_http::Method::GET );
        return initiateHttpMessageDelivery( url );
    }

    bool AsyncHttpClient::doPost(
        const QUrl& url,
        const nx_http::StringType& contentType,
        const nx_http::StringType& messageBody )
    {
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
        QMutexLocker lk( &m_mutex );
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

    void AsyncHttpClient::setResponseReadTimeoutMs( unsigned int _responseReadTimeoutMs )
    {
        m_responseReadTimeoutMs = _responseReadTimeoutMs;
    }

    void AsyncHttpClient::setMessageBodyReadTimeoutMs( unsigned int messageBodyReadTimeoutMs )
    {
        m_msgBodyReadTimeoutMs = messageBodyReadTimeoutMs;
    }

    void AsyncHttpClient::setDecodeChunkedMessageBody( bool val )
    {
        m_httpStreamReader.setDecodeChunkedMessageBody( val );
    }

    void AsyncHttpClient::asyncConnectDone( AbstractSocket* sock, SystemError::ErrorCode errorCode )
    {
        std::shared_ptr<AsyncHttpClient> sharedThis( shared_from_this() );

        QMutexLocker lk( &m_mutex );

        Q_ASSERT( sock == m_socket.data() );
        if( m_terminated )
            return;

        if( m_state != sWaitingConnectToHost )
        {
            Q_ASSERT( false );
            return;
        }

        if( errorCode == SystemError::noError )
        {
            //connect successful
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

        QMutexLocker lk( &m_mutex );

        Q_ASSERT( sock == m_socket.data() );
        if( m_terminated )
            return;

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
            m_state = m_httpStreamReader.state() == HttpStreamReader::messageDone ? sDone : sFailed;
            lk.unlock();
            emit done( sharedThis );
            lk.relock();
            return;
        }

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

        QMutexLocker lk( &m_mutex );

        Q_ASSERT( sock == m_socket.data() );
        if( m_terminated )
            return;

        if( errorCode != SystemError::noError )
        {
            if( reconnectIfAppropriate() )
                return;
            NX_LOG(lit("Error reading (state %1) http response from %2. %3").arg( m_state ).arg( m_url.toString() ).arg( SystemError::toString( errorCode ) ), cl_logDEBUG1 );
            m_state = m_httpStreamReader.state() == HttpStreamReader::messageDone ? sDone : sFailed;
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

                if( m_httpStreamReader.state() <= HttpStreamReader::readingMessageHeaders )
                {
                    m_responseBuffer.resize( 0 );
                    if( !m_socket->readSomeAsync( &m_responseBuffer, std::bind( &AsyncHttpClient::onSomeBytesReadAsync, this, sock, _1, _2 ) ) )
                    {
                        NX_LOG( lit( "Failed to read (1) response from %1. %2" ).arg( m_url.toString() ).arg( SystemError::getLastOSErrorText() ), cl_logDEBUG1 );
                        m_state = sFailed;
                        lk.unlock();
                        emit done( sharedThis );
                        lk.relock();
                    }
                    return;  //response has not been read yet
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
                if( response->statusLine.statusCode == StatusCode::unauthorized
                    && !m_authorizationTried && (!m_userName.isEmpty() || !m_userPassword.isEmpty()) )
                {
                    //trying authorization
                    if( resendRequestWithAuthorization( *response ) )
                        return;
                }

                const bool messageHasMessageBody =
                    (m_httpStreamReader.state() == HttpStreamReader::readingMessageBody) || (m_httpStreamReader.messageBodyBufferSize() > 0);

                m_state = sResponseReceived;
                lk.unlock();
                emit responseReceived( sharedThis );
                lk.relock();
                if( m_terminated )
                    break;

                //is message body follows?
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

                if( m_httpStreamReader.state() == HttpStreamReader::readingMessageBody )
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
            QMutexLocker lk( &m_mutex );
            m_terminated = true;
        }
        //after we set m_terminated to true with m_mutex locked socket event processing is stopped and m_socket cannot change its value
        if( m_socket )
            m_socket->cancelAsyncIO();

        {
            QMutexLocker lk( &m_mutex );
            m_terminated = false;
        }

        m_authorizationTried = false;
        m_request = nx_http::Request();
    }

    bool AsyncHttpClient::initiateHttpMessageDelivery( const QUrl& url )
    {
        using namespace std::placeholders;

        bool canUseExistingConnection = true;
        if( m_httpStreamReader.message().type == nx_http::MessageType::response )
        {
            canUseExistingConnection = nx_http::getHeaderValue(
                m_httpStreamReader.message().response->headers,
                "Connection" ) != "close";
        }

        m_httpStreamReader.resetState();

        if( m_socket )
        {
            //TODO #ak think again about next cancellation
            m_socket->cancelAsyncIO();

            if( canUseExistingConnection )
            {
                serializeRequest();
                m_state = sSendingRequest;

                if( !m_socket->sendAsync( m_requestBuffer, std::bind( &AsyncHttpClient::asyncSendDone, this, m_socket.data(), _1, _2 ) ) )
                {
                    NX_LOG( lit("Failed to init async socket call (connecting to %1:%2) to aio service. %3").
                        arg(url.host()).arg(url.port()).arg(SystemError::toString(SystemError::getLastOSErrorCode())), cl_logDEBUG1 );
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

        m_state = sInit;

        m_socket = QSharedPointer<AbstractStreamSocket>( SocketFactory::createStreamSocket(/*url.scheme() == lit("https")*/));
        if( !m_socket->setNonBlockingMode( true ) ||
            !m_socket->setSendTimeout( DEFAULT_CONNECT_TIMEOUT ) ||
            !m_socket->setRecvTimeout( m_responseReadTimeoutMs ) )
        {
            NX_LOG( lit("Failed to put socket to non blocking mode. %1").
                arg(SystemError::toString(SystemError::getLastOSErrorCode())), cl_logDEBUG1 );
            m_socket.clear();
            return false;
        }

        m_url = url;
        m_state = sWaitingConnectToHost;

        //starting async connect
        if( !m_socket->connectAsync(
                SocketAddress( url.host(), url.port( DEFAULT_HTTP_PORT ) ),
                std::bind( &AsyncHttpClient::asyncConnectDone, this, m_socket.data(), _1 ) ) )
        {
            NX_LOG( lit("Failed to perform async connect to %1:%2. %3").
                arg(url.host()).arg(url.port()).arg(SystemError::toString(SystemError::getLastOSErrorCode())), cl_logDEBUG1 );
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
            m_state = (m_httpStreamReader.state() == HttpStreamReader::messageDone) || (m_httpStreamReader.state() == HttpStreamReader::readingMessageBody)
                ? sDone
                : sFailed;
            return 0;
        }

        m_totalBytesRead += bytesRead;

        if( !m_httpStreamReader.parseBytes( m_responseBuffer, bytesRead ) )
        {
            NX_LOG( lit("Error parsing http response from %1. %2").
                arg(m_url.toString()).arg(m_httpStreamReader.errorText()), cl_logDEBUG1 );
            m_state = sFailed;
            return -1;
        }

        if( m_httpStreamReader.state() == HttpStreamReader::messageDone )
            m_state = sDone;
        else if( m_httpStreamReader.state() == HttpStreamReader::parseError )
            m_state = sFailed;
        return bytesRead;
    }

    void AsyncHttpClient::composeRequest( const nx_http::StringType& httpMethod )
    {
        const bool useHttp11 = true;   //TODO #ak check if we need it (e.g. we using keep-alive or requesting live capture)

        m_request.requestLine.method = httpMethod;
        m_request.requestLine.url = m_url.path() + (m_url.hasQuery() ? (QLatin1String("?") + m_url.query()) : QString());
        m_request.requestLine.version = useHttp11 ? nx_http::http_1_1 : nx_http::http_1_0;
        if( !m_userAgent.isEmpty() )
            m_request.headers.insert( std::make_pair("User-Agent", m_userAgent.toLatin1()) );
        if( useHttp11 )
        {
            m_request.headers.insert( std::make_pair("Accept", "*/*") );
            if( m_contentEncodingUsed )
                m_request.headers.insert( std::make_pair("Accept-Encoding", "gzip;q=1.0, identity;q=0.5, *;q=0") );
            else
                m_request.headers.insert( std::make_pair("Accept-Encoding", "identity;q=1.0, *;q=0") );
            m_request.headers.insert( std::make_pair("Cache-Control", "max-age=0") );
            //m_request.headers.insert( std::make_pair("Connection", "keep-alive") );
            m_request.headers.insert( std::make_pair("Host", m_url.host().toLatin1()) );
        }

        m_request.headers.insert(m_additionalHeaders.cbegin(), m_additionalHeaders.cend());

        //adding user credentials
        if( !m_url.userName().isEmpty() )
            m_userName = m_url.userName();
        if( !m_url.password().isEmpty() )
            m_userPassword = m_url.password();

        //not using Basic authentication by default, since it is not secure
        nx_http::removeHeader(&m_request.headers, header::Authorization::NAME);
    }

    void AsyncHttpClient::addRequestHeader(const StringType& key, const StringType& value)
    {
        m_additionalHeaders.insert( make_pair(key, value) );
    }

    void AsyncHttpClient::serializeRequest()
    {
        m_requestBuffer.clear();
        m_request.serialize( &m_requestBuffer );
        m_requestBytesSent = 0;
    }

#if 0
    bool AsyncHttpClient::sendRequest()
    {
        Q_ASSERT( (int)m_requestBytesSent < m_requestBuffer.size() );
        int bytesSent = m_socket->send( m_requestBuffer.data()+m_requestBytesSent, m_requestBuffer.size()-m_requestBytesSent );
        if( bytesSent == -1 )
            return (SystemError::getLastOSErrorCode() == SystemError::wouldBlock) || (SystemError::getLastOSErrorCode() == SystemError::again);
        m_requestBytesSent += bytesSent;    //TODO #ak it would be very usefull to use buffer with effective pop_front()
        return true;
    }
#endif

    bool AsyncHttpClient::reconnectIfAppropriate()
    {
        //TODO #ak we need reconnect and request entity from the point we stopped at
        return false;
    }

    //bool calcDigestResponse(
    //    const QString& method,
    //    const QString& userName,
    //    const QString& userPassword,
    //    const QUrl& url,
    //    const header::WWWAuthenticate& wwwAuthenticateHeader,
    //    header::DigestAuthorization* const digestAuthorizationHeader )
    //{
    //    if( wwwAuthenticateHeader.authScheme != header::AuthScheme::digest )
    //        return false;

    //    //reading params
    //    QMap<BufferType, BufferType>::const_iterator nonceIter = wwwAuthenticateHeader.params.find("nonce");
    //    const BufferType nonce = nonceIter != wwwAuthenticateHeader.params.end() ? nonceIter.value() : BufferType();
    //    QMap<BufferType, BufferType>::const_iterator realmIter = wwwAuthenticateHeader.params.find("realm");
    //    const BufferType realm = realmIter != wwwAuthenticateHeader.params.end() ? realmIter.value() : BufferType();
    //    QMap<BufferType, BufferType>::const_iterator qopIter = wwwAuthenticateHeader.params.find("qop");
    //    const BufferType qop = qopIter != wwwAuthenticateHeader.params.end() ? qopIter.value() : BufferType();

    //    if( qop.indexOf("auth-int") != -1 )
    //        return false;   //qop=auth-int is not supported

    //    BufferType nonceCount = "00000001";     //TODO/IMPL
    //    BufferType clientNonce = "0a4f113b";    //TODO/IMPL

    //    QCryptographicHash md5HashCalc( QCryptographicHash::Md5 );

    //    //HA1
    //    md5HashCalc.addData( userName.toLatin1() );
    //    md5HashCalc.addData( ":" );
    //    md5HashCalc.addData( realm );
    //    md5HashCalc.addData( ":" );
    //    md5HashCalc.addData( userPassword.toLatin1() );
    //    const BufferType& ha1 = md5HashCalc.result().toHex();
    //    //HA2, qop=auth-int is not supported
    //    md5HashCalc.reset();
    //    md5HashCalc.addData( method.toLatin1() );
    //    md5HashCalc.addData( ":" );
    //    //md5HashCalc.addData( url.path().toLatin1() );
    //    md5HashCalc.addData( url.toString().toLatin1() );
    //    const BufferType& ha2 = md5HashCalc.result().toHex();
    //    //response
    //    digestAuthorizationHeader->addParam( "username", userName.toLatin1() );
    //    digestAuthorizationHeader->addParam( "realm", realm );
    //    digestAuthorizationHeader->addParam( "nonce", nonce );
    //    digestAuthorizationHeader->addParam( "uri", url.toString().toLatin1() );
    //    md5HashCalc.reset();
    //    md5HashCalc.addData( ha1 );
    //    md5HashCalc.addData( ":" );
    //    md5HashCalc.addData( nonce );
    //    md5HashCalc.addData( ":" );
    //    if( !qop.isEmpty() )
    //    {
    //        md5HashCalc.addData( nonceCount );
    //        md5HashCalc.addData( ":" );
    //        md5HashCalc.addData( clientNonce );
    //        md5HashCalc.addData( ":" );
    //        md5HashCalc.addData( qop );
    //        md5HashCalc.addData( ":" );

    //        digestAuthorizationHeader->addParam( "qop", qop );
    //        digestAuthorizationHeader->addParam( "nc", nonceCount );
    //        digestAuthorizationHeader->addParam( "cnonce", clientNonce );
    //    }
    //    md5HashCalc.addData( ha2 );
    //    digestAuthorizationHeader->addParam( "response", md5HashCalc.result().toHex() );
    //    return true;
    //}


//    nx_http::header::WWWAuthenticate wwwAuthenticateHeader;
//    wwwAuthenticateHeader.authScheme = nx_http::header::AuthScheme::digest;
//    wwwAuthenticateHeader.params["realm"] = "Surveillance Server";
//    wwwAuthenticateHeader.params["nonce"] = "06737538";
//    nx_http::header::DigestAuthorization digestAuthorizationHeader;
//
//    bool res = nx_http::calcDigestResponse(
//        lit("DESCRIBE"),
//        lit("admin"), lit("admin"),
//        QUrl(lit("rtsp://192.168.1.104:554/0")),
//        wwwAuthenticateHeader,
//        &digestAuthorizationHeader );


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
            nx_http::insertOrReplaceHeader(
                &m_request.headers,
                nx_http::HttpHeader(
                    header::Authorization::NAME,
                    header::BasicAuthorization( m_userName.toLatin1(), m_userPassword.toLatin1() ).toString() ) );
        }
        else if( wwwAuthenticateHeader.authScheme == header::AuthScheme::digest )
        {
            //reading params
            QMap<BufferType, BufferType>::const_iterator nonceIter = wwwAuthenticateHeader.params.find("nonce");
            const BufferType nonce = nonceIter != wwwAuthenticateHeader.params.end() ? nonceIter.value() : BufferType();
            QMap<BufferType, BufferType>::const_iterator realmIter = wwwAuthenticateHeader.params.find("realm");
            const BufferType realm = realmIter != wwwAuthenticateHeader.params.end() ? realmIter.value() : BufferType();
            QMap<BufferType, BufferType>::const_iterator qopIter = wwwAuthenticateHeader.params.find("qop");
            const BufferType qop = qopIter != wwwAuthenticateHeader.params.end() ? qopIter.value() : BufferType();

            if( qop.indexOf("auth-int") != -1 ) //TODO #ak qop can have value "auth,auth-int". That should be supported
                return false;   //qop=auth-int is not supported

            BufferType ha1;
            QCryptographicHash md5HashCalc( QCryptographicHash::Md5 );
            if (m_authType == authDigestWithPasswordHash) {
                ha1 = m_userPassword.toUtf8();
            }
            else {
                //HA1
                md5HashCalc.addData( m_userName.toLatin1() );
                md5HashCalc.addData( ":" );
                md5HashCalc.addData( realm );
                md5HashCalc.addData( ":" );
                md5HashCalc.addData( m_userPassword.toLatin1() );
                ha1 = md5HashCalc.result().toHex();
            }
            //HA2, qop=auth-int is not supported
            md5HashCalc.reset();
            md5HashCalc.addData( m_request.requestLine.method );
            md5HashCalc.addData( ":" );
            md5HashCalc.addData( m_url.path().toLatin1() );
            const BufferType& ha2 = md5HashCalc.result().toHex();
            //response
            header::DigestAuthorization digestAuthorizationHeader;
            digestAuthorizationHeader.addParam( "username", m_userName.toLatin1() );
            digestAuthorizationHeader.addParam( "realm", realm );
            digestAuthorizationHeader.addParam( "nonce", nonce );
            digestAuthorizationHeader.addParam( "uri", m_url.path().toLatin1() );
            md5HashCalc.reset();
            md5HashCalc.addData( ha1 );
            md5HashCalc.addData( ":" );
            md5HashCalc.addData( nonce );
            md5HashCalc.addData( ":" );
            if( !qop.isEmpty() )
            {
                const BufferType nonceCount = "00000001";     //TODO/IMPL
                const BufferType clientNonce = "0a4f113b";    //TODO/IMPL

                md5HashCalc.addData( nonceCount );
                md5HashCalc.addData( ":" );
                md5HashCalc.addData( clientNonce );
                md5HashCalc.addData( ":" );
                md5HashCalc.addData( qop );
                md5HashCalc.addData( ":" );

                digestAuthorizationHeader.addParam( "qop", qop );
                digestAuthorizationHeader.addParam( "nc", nonceCount );
                digestAuthorizationHeader.addParam( "cnonce", clientNonce );
            }
            md5HashCalc.addData( ha2 );
            digestAuthorizationHeader.addParam( "response", md5HashCalc.result().toHex() );

            BufferType authorizationStr;
            digestAuthorizationHeader.serialize( &authorizationStr );

            nx_http::insertOrReplaceHeader(
                &m_request.headers,
                nx_http::HttpHeader( header::Authorization::NAME, authorizationStr ) );
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


    bool downloadFileAsync(
        const QUrl& url,
        std::function<void(SystemError::ErrorCode, int, nx_http::BufferType)> completionHandler )
    {
        nx_http::AsyncHttpClientPtr httpClientCaptured = std::make_shared<nx_http::AsyncHttpClient>();
        auto requestCompletionFunc = [httpClientCaptured, completionHandler]
            ( nx_http::AsyncHttpClientPtr httpClient ) mutable
        {
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
