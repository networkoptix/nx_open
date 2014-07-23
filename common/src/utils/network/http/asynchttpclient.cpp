/**********************************************************
* 22 nov 2012
* a.kolesnikov
***********************************************************/

#include "asynchttpclient.h"

#include <QtCore/QCryptographicHash>
#include <QtCore/QMutexLocker>

#include "../aio/aioservice.h"
#include "../../common/log.h"
#include "../../common/systemerror.h"


//TODO: #ak persistent connection support
//TODO: #ak MUST call removeFromWatch with 3rd parameter set to false
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
        m_responseReadTimeoutMs( DEFAULT_RESPONSE_READ_TIMEOUT )
    {
        m_responseBuffer.resize(RESPONSE_BUFFER_SIZE);
    }

    AsyncHttpClient::~AsyncHttpClient()
    {
        terminate();
    }

    QSharedPointer<AbstractStreamSocket> AsyncHttpClient::takeSocket()
    {
        QSharedPointer<AbstractStreamSocket> result = m_socket;
        terminate();
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
        {
            aio::AIOService::instance()->removeFromWatch( m_socket, aio::etRead );
            aio::AIOService::instance()->removeFromWatch( m_socket, aio::etWrite );

            //AIOService guarantees that eventTriggered had returned and will never be called with m_socket
        }
    }

    //!Implementation of aio::AIOEventHandler::eventTriggered
    void AsyncHttpClient::eventTriggered( AbstractSocket* sock, aio::EventType eventType ) throw()
    {
        //TODO #AK: need to refactore this method: possibly split to multiple methods

        std::shared_ptr<AsyncHttpClient> sharedThis( shared_from_this() );

        QMutexLocker lk( &m_mutex );

        Q_ASSERT( sock == m_socket.data() );
        while( !m_terminated )
        {
            switch( m_state )
            {
                case sWaitingConnectToHost:
                    switch( eventType )
                    {
                        case aio::etRead:
                            Q_ASSERT( false );
                            break;

                        case aio::etWrite:
                            //connect successful
                            serializeRequest();
                            m_state = sSendingRequest;
                            lk.unlock();
                            emit tcpConnectionEstablished( sharedThis );
                            lk.relock();
                            continue;

                        case aio::etTimedOut:
                        case aio::etError:
                            NX_LOG( lit("AsyncHttpClient. Failed to establish tcp connection to %1. %2").
                                arg(m_url.toString()).arg(QLatin1String(aio::toString(eventType))), cl_logDEBUG1 );
                            if( reconnectIfAppropriate() )
                                break;
                            m_state = sFailed;
                            aio::AIOService::instance()->removeFromWatch( m_socket, aio::etWrite );
                            lk.unlock();
                            emit done( sharedThis );
                            lk.relock();
                            break;

                       default:
                            break;
                    }
                    break;

                case sSendingRequest:
                    if( eventType == aio::etError || eventType == aio::etTimedOut )
                    {
                        if( eventType == aio::etError )
                        {
                            if( reconnectIfAppropriate() )
                                break;
                            SystemError::ErrorCode errorCode = SystemError::noError;
                            sock->getLastError(&errorCode);
                            NX_LOG( lit("Error sending http request to %1. %2").
                                arg(m_url.toString()).arg(SystemError::toString(errorCode)), cl_logDEBUG1);
                        }
                        else
                        {
                            NX_LOG( lit("Error sending http request from %1. Socket write operation has timed out").
                                arg(m_url.toString()), cl_logDEBUG1 );
                        }
                        m_state = m_httpStreamReader.state() == HttpStreamReader::messageDone ? sDone : sFailed;
                        lk.unlock();
                        emit done( sharedThis );
                        lk.relock();
                        break;
                    }

                    if( !sendRequest() )
                    {
                        NX_LOG( lit("Failed to send request to %1. %2").arg(m_url.toString()).arg(SystemError::getLastOSErrorText()), cl_logDEBUG1 );
                        m_state = sFailed;
                        aio::AIOService::instance()->removeFromWatch( m_socket, aio::etWrite );
                        lk.unlock();
                        emit done( sharedThis );
                        lk.relock();
                        break;
                    }
                    if( (int)m_requestBytesSent == m_requestBuffer.size() )
                    {
                        NX_LOG( lit("Http request has been successfully sent to %1").arg(m_url.toString()), cl_logDEBUG2 );
                        m_state = sReceivingResponse;
                        aio::AIOService::instance()->removeFromWatch( m_socket, aio::etWrite );
                        m_socket->setRecvTimeout( m_responseReadTimeoutMs );
                        aio::AIOService::instance()->watchSocket( m_socket, aio::etRead, this );
                    }
                    break;

                case sReceivingResponse:
                {
                    Q_ASSERT( eventType != aio::etWrite );
                    if( eventType == aio::etError || eventType == aio::etTimedOut )
                    {
                        if( eventType == aio::etError )
                        {
                            if( reconnectIfAppropriate() )
                                break;
                            SystemError::ErrorCode errorCode = SystemError::noError;
                            sock->getLastError(&errorCode);
                            NX_LOG(lit("Error reading http response from %1. %2").
                                arg(m_url.toString()).arg(SystemError::toString(errorCode)), cl_logDEBUG1);
                        }
                        else
                        {
                            NX_LOG( lit("Error reading http response from %1. Socket read operation has timed out").
                                arg(m_url.toString()), cl_logDEBUG1 );
                        }
                        m_state = m_httpStreamReader.state() == HttpStreamReader::messageDone ? sDone : sFailed;
                        lk.unlock();
                        emit done( sharedThis );
                        lk.relock();
                        break;
                    }

                    readAndParseHttp();
                    //TODO/IMPL reconnect in case of error

                    if( m_state == sFailed )
                    {
                        lk.unlock();
                        emit done( sharedThis );
                        lk.relock();
                        break;
                    }

                    if( m_httpStreamReader.state() <= HttpStreamReader::readingMessageHeaders )
                        break;  //response has not been read yet

                    //read http message headers
                    if( m_httpStreamReader.message().type != nx_http::MessageType::response )
                    {
                        NX_LOG( lit("Unexpectedly received request from %1:%2 while expecting response! Ignoring...").
                            arg(m_url.host()).arg(m_url.port()), cl_logDEBUG1 );
                        m_state = sFailed;
                        lk.unlock();
                        emit done( sharedThis );
                        lk.relock();
                        break;
                    }

                    //response read
                    NX_LOG( lit("Http response from %1 has been successfully read. Status line: %2(%3)").
                        arg(m_url.toString()).arg(m_httpStreamReader.message().response->statusLine.statusCode).
                        arg(QLatin1String(m_httpStreamReader.message().response->statusLine.reasonPhrase)), cl_logDEBUG2 );

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
                        break; // wait for more data

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
                    Q_ASSERT( eventType != aio::etWrite );
                    if( eventType == aio::etError )
                    {
                        if( reconnectIfAppropriate() )
                            break;
                        SystemError::ErrorCode errorCode = SystemError::noError;
                        sock->getLastError(&errorCode);
                        NX_LOG(lit("Error reading http response message body from %1. %2").arg(m_url.toString()).arg(SystemError::toString(errorCode)), cl_logDEBUG1);
                        m_state = m_httpStreamReader.state() == HttpStreamReader::messageDone ? sDone : sFailed;
                        lk.unlock();
                        emit done( sharedThis );
                        lk.relock();
                        break;
                    }

                    int bytesRead = readAndParseHttp();
                    //TODO #ak reconnect in case of error
                    if( m_state != sReadingMessageBody )
                        aio::AIOService::instance()->removeFromWatch( m_socket, aio::etRead );
                    if( bytesRead > 0 )
                    {
                        lk.unlock();
                        emit someMessageBodyAvailable( sharedThis );
                        lk.relock();
                        if( m_terminated )
                            break;
                    }
                    if( m_state >= sFailed )
                    {
                        lk.unlock();
                        emit done( sharedThis );
                        lk.relock();
                    }
                    break;
                }

                default:
                {
                    Q_ASSERT( false );
                    break;
                }
            }

            break;
        }

        if( m_state == sFailed || m_state == sDone )
        {
            aio::AIOService::instance()->removeFromWatch( m_socket, aio::etRead );
            aio::AIOService::instance()->removeFromWatch( m_socket, aio::etWrite );
        }
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

    void AsyncHttpClient::setResponseReadTimeoutMs( int _responseReadTimeoutMs )
    {
        m_responseReadTimeoutMs = _responseReadTimeoutMs;
    }

    void AsyncHttpClient::setDecodeChunkedMessageBody( bool val )
    {
        m_httpStreamReader.setDecodeChunkedMessageBody( val );
    }

    void AsyncHttpClient::resetDataBeforeNewRequest()
    {
        //stopping client, if it is running
        terminate();
        {
            QMutexLocker lk( &m_mutex );
            m_terminated = false;
        }

        m_authorizationTried = false;
    }

    bool AsyncHttpClient::initiateHttpMessageDelivery( const QUrl& url )
    {
        m_httpStreamReader.resetState();

        if( m_socket )
        {
            aio::AIOService::instance()->removeFromWatch( m_socket, aio::etRead );
            aio::AIOService::instance()->removeFromWatch( m_socket, aio::etWrite );

            serializeRequest();
            m_state = sSendingRequest;
        }
        else {
            m_state = sInit;

            m_socket = QSharedPointer<AbstractStreamSocket>( SocketFactory::createStreamSocket(/*url.scheme() == lit("https")*/) ); //TODO #ak uncomment when async ssl connection supported
            if( !m_socket->setNonBlockingMode( true ) ||
                !m_socket->setSendTimeout( DEFAULT_CONNECT_TIMEOUT ) )
            {
                NX_LOG( lit("Failed to put socket to non blocking mode. %1").
                    arg(SystemError::toString(SystemError::getLastOSErrorCode())), cl_logDEBUG1 );
                m_socket.clear();
                return false;
            }

            //starting async connect
            if( !m_socket->connect( url.host(), url.port(DEFAULT_HTTP_PORT), 0 ) )
            {
                NX_LOG( lit("Failed to perform async connect to %1:%2. %3").
                    arg(url.host()).arg(url.port()).arg(SystemError::toString(SystemError::getLastOSErrorCode())), cl_logDEBUG1 );
                m_socket.clear();
                return false;
            }

            m_url = url;
            m_state = sWaitingConnectToHost;
        }

        //connect is done if socket is available for write
        if( !aio::AIOService::instance()->watchSocket( m_socket, aio::etWrite, this ) )
        {
            NX_LOG( lit("Failed to add socket (connecting to %1:%2) to aio service. %3").
                arg(url.host()).arg(url.port()).arg(SystemError::toString(SystemError::getLastOSErrorCode())), cl_logDEBUG1 );
            m_socket.clear();
            return false;
        }

        return true;
    }

    int AsyncHttpClient::readAndParseHttp()
    {
        int bytesRead = m_socket->recv( m_responseBuffer.data(), m_responseBuffer.size(), 0 );
        if( bytesRead < 0 )         //read error
        {
            if( SystemError::getLastOSErrorCode() == SystemError::wouldBlock || SystemError::getLastOSErrorCode() == SystemError::again )
                return 0;
            NX_LOG( lit("AsyncHttpClient. Error reading socket (%1). Url %2").
                arg(SystemError::getLastOSErrorText()).arg(m_url.toString()), cl_logERROR );
            m_state = sFailed;
            return -1;
        }

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
        //adding user credentials
        if( !m_userName.isEmpty() || !m_userPassword.isEmpty() )
        {
            nx_http::insertOrReplaceHeader(
                &m_request.headers,
                nx_http::HttpHeader(
                    Header::Authorization::NAME,
                    Header::BasicAuthorization( m_userName.toLatin1(), m_userPassword.toLatin1() ).toString() ) );
        }
    }

    void AsyncHttpClient::addRequestHeader(const StringType& key, const StringType& value)
    {
        m_request.headers.insert( make_pair(key, value) );
    }

    void AsyncHttpClient::serializeRequest()
    {
        m_requestBuffer.clear();
        m_request.serialize( &m_requestBuffer );
        m_requestBytesSent = 0;
    }

    bool AsyncHttpClient::sendRequest()
    {
        Q_ASSERT( (int)m_requestBytesSent < m_requestBuffer.size() );
        int bytesSent = m_socket->send( m_requestBuffer.data()+m_requestBytesSent, m_requestBuffer.size()-m_requestBytesSent );
        if( bytesSent == -1 )
            return (SystemError::getLastOSErrorCode() == SystemError::wouldBlock) || (SystemError::getLastOSErrorCode() == SystemError::again);
        m_requestBytesSent += bytesSent;    //TODO #ak it would be very usefull to use buffer with effective pop_front()
        return true;
    }

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
    //    const Header::WWWAuthenticate& wwwAuthenticateHeader,
    //    Header::DigestAuthorization* const digestAuthorizationHeader )
    //{
    //    if( wwwAuthenticateHeader.authScheme != Header::AuthScheme::digest )
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


//    nx_http::Header::WWWAuthenticate wwwAuthenticateHeader;
//    wwwAuthenticateHeader.authScheme = nx_http::Header::AuthScheme::digest;
//    wwwAuthenticateHeader.params["realm"] = "Surveillance Server";
//    wwwAuthenticateHeader.params["nonce"] = "06737538";
//    nx_http::Header::DigestAuthorization digestAuthorizationHeader;
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

        Header::WWWAuthenticate wwwAuthenticateHeader;
        wwwAuthenticateHeader.parse( wwwAuthenticateIter->second );
        if( wwwAuthenticateHeader.authScheme != Header::AuthScheme::digest )
            return false;

        //reading params
        QMap<BufferType, BufferType>::const_iterator nonceIter = wwwAuthenticateHeader.params.find("nonce");
        const BufferType nonce = nonceIter != wwwAuthenticateHeader.params.end() ? nonceIter.value() : BufferType();
        QMap<BufferType, BufferType>::const_iterator realmIter = wwwAuthenticateHeader.params.find("realm");
        const BufferType realm = realmIter != wwwAuthenticateHeader.params.end() ? realmIter.value() : BufferType();
        QMap<BufferType, BufferType>::const_iterator qopIter = wwwAuthenticateHeader.params.find("qop");
        const BufferType qop = qopIter != wwwAuthenticateHeader.params.end() ? qopIter.value() : BufferType();

        if( qop.indexOf("auth-int") != -1 ) //TODO #ak qop can have value "auth,auth-int". That should be supported
            return false;   //qop=auth-int is not supported

        QCryptographicHash md5HashCalc( QCryptographicHash::Md5 );

        //HA1
        md5HashCalc.addData( m_userName.toLatin1() );
        md5HashCalc.addData( ":" );
        md5HashCalc.addData( realm );
        md5HashCalc.addData( ":" );
        md5HashCalc.addData( m_userPassword.toLatin1() );
        const BufferType& ha1 = md5HashCalc.result().toHex();
        //HA2, qop=auth-int is not supported
        md5HashCalc.reset();
        md5HashCalc.addData( m_request.requestLine.method );
        md5HashCalc.addData( ":" );
        md5HashCalc.addData( m_url.path().toLatin1() );
        const BufferType& ha2 = md5HashCalc.result().toHex();
        //response
        Header::DigestAuthorization digestAuthorizationHeader;
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
            nx_http::HttpHeader( Header::Authorization::NAME, authorizationStr ) );

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
}
