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


//TODO/IMPL reconnect

using std::make_pair;

namespace nx_http
{
    static const size_t RESPONSE_BUFFER_SIZE = 16*1024;

    static QAtomicInt AsyncHttpClient_instanceCount = 0;

    AsyncHttpClient::AsyncHttpClient()
    :
        m_state( sInit ),
        m_requestBytesSent( 0 ),
        m_authorizationTried( false ),
        m_terminated( false )
    {
        m_responseBuffer.resize(RESPONSE_BUFFER_SIZE);

        //AsyncHttpClient_instanceCount.ref();
    }

    AsyncHttpClient::~AsyncHttpClient()
    {
        terminate();

        //AsyncHttpClient_instanceCount.deref();
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
            aio::AIOService::instance()->removeFromWatch( m_socket, PollSet::etRead );
            aio::AIOService::instance()->removeFromWatch( m_socket, PollSet::etWrite );

            //AIOService guarantees that eventTriggered had returned and will never be called with m_socket
        }
    }

    //!Implementation of aio::AIOEventHandler::eventTriggered
    void AsyncHttpClient::eventTriggered( AbstractSocket* sock, PollSet::EventType eventType ) throw()
    {
        //TODO/IMPL #AK need to refactore this method: possibly split to multiple methods

        //ScopedDestructionProhibition undestructable( this );    //~ScopedDestructionProhibition can call delete *this, which will lock m_mutex
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
                        case PollSet::etRead:
                            Q_ASSERT( false );
                            break;

                        case PollSet::etWrite:
                            //connect successful
                            formRequest();
                            serializeRequest();
                            m_state = sSendingRequest;
                            lk.unlock();
                            emit tcpConnectionEstablished( this );
                            lk.relock();
                            continue;

                        case PollSet::etTimedOut:
                        case PollSet::etError:
                            NX_LOG( QString::fromLatin1("Failed to connect to %1:%2").arg(m_url.host()).arg(m_url.port()), cl_logDEBUG1 );
                            if( reconnectIfAppropriate() )
                                break;
                            m_state = sFailed;
                            aio::AIOService::instance()->removeFromWatch( m_socket, PollSet::etWrite );
                            lk.unlock();
                            emit done( this );
                            lk.relock();
                            break;

                       default:
                            break;
                    }
                    break;

                case sSendingRequest:
                    if( eventType == PollSet::etError )
                    {
                        if( reconnectIfAppropriate() )
                            break;
                        NX_LOG( QString::fromLatin1("Error sending http request to %1").arg(m_url.toString()), cl_logDEBUG1 );
                        m_state = m_httpStreamReader.state() == HttpStreamReader::messageDone ? sDone : sFailed;
                        lk.unlock();
                        emit done( this );
                        lk.relock();
                        break;
                    }

                    if( !sendRequest() )
                    {
                        NX_LOG( QString::fromLatin1("Failed to send request to %1. %2").arg(m_url.toString()).arg(SystemError::getLastOSErrorText()), cl_logDEBUG1 );
                        m_state = sFailed;
                        aio::AIOService::instance()->removeFromWatch( m_socket, PollSet::etWrite );
                        lk.unlock();
                        emit done( this );
                        lk.relock();
                        break;
                    }
                    if( (int)m_requestBytesSent == m_requestBuffer.size() )
                    {
                        NX_LOG( QString::fromLatin1("Http request has been successfully sent to %1").arg(m_url.toString()), cl_logDEBUG2 );
                        m_state = sReceivingResponse;
                        aio::AIOService::instance()->removeFromWatch( m_socket, PollSet::etWrite );
                        aio::AIOService::instance()->watchSocket( m_socket, PollSet::etRead, this );
                    }
                    break;

                case sReceivingResponse:
                {
                    Q_ASSERT( eventType != PollSet::etWrite );
                    if( eventType == PollSet::etError )
                    {
                        if( reconnectIfAppropriate() )
                            break;
                        NX_LOG( QString::fromLatin1("Error reading http response from %1").arg(m_url.toString()), cl_logDEBUG1 );
                        m_state = m_httpStreamReader.state() == HttpStreamReader::messageDone ? sDone : sFailed;
                        lk.unlock();
                        emit done( this );
                        lk.relock();
                        break;
                    }

                    readAndParseHttp();
                    //TODO/IMPL reconnect in case of error

                    if( m_httpStreamReader.state() == HttpStreamReader::readingMessageHeaders )
                        break;  //response has not been read yet

                    //read http message headers
                    if( m_httpStreamReader.message().type != nx_http::MessageType::response )
                    {
                        NX_LOG( QString::fromLatin1("Unexpectedly received request from %1:%2 while expecting response! Ignoring...").
                            arg(m_url.host()).arg(m_url.port()), cl_logDEBUG1 );
                        m_state = sFailed;
                        lk.unlock();
                        emit done( this );
                        lk.relock();
                        break;
                    }

                    //response read
                    NX_LOG( QString::fromLatin1("Http response from %1 has been successfully read. Status line: %2(%3)").
                        arg(m_url.toString()).arg(m_httpStreamReader.message().response->statusLine.statusCode).
                        arg(QLatin1String(m_httpStreamReader.message().response->statusLine.reasonPhrase)), cl_logDEBUG2 );

                    const HttpResponse* response = m_httpStreamReader.message().response;
                    if( response->statusLine.statusCode == StatusCode::unauthorized
                        && !m_authorizationTried && (!m_userName.isEmpty() || !m_userPassword.isEmpty()) )
                    {
                        //trying authorization
                        if( resendRequestWithAuthorization( *response ) )
                            return;
                    }

                    //should only call removeFromWatch if startReadMessageBody has not been called from responseReceived connected slot
                    aio::AIOService::instance()->removeFromWatch( m_socket, PollSet::etRead );

                    const bool messageHasMessageBody = 
                        (m_httpStreamReader.state() == HttpStreamReader::readingMessageBody) || (m_httpStreamReader.messageBodyBufferSize() > 0);

                    m_state = sResponseReceived;
                    lk.unlock();
                    emit responseReceived( this );
                    lk.relock();
                    if( m_terminated )
                        break;

                    //is message body follows?
                    if( !messageHasMessageBody )
                    {
                        //no message body: done
                        m_state = m_httpStreamReader.state() == HttpStreamReader::parseError ? sFailed : sDone;
                        lk.unlock();
                        emit done( this );
                        lk.relock();
                        break;
                    }

                    if( m_httpStreamReader.messageBodyBufferSize() > 0 &&   //some message body has been read
                        m_state == sReadingMessageBody )                    //client wants to read message body
                    {
                        lk.unlock();
                        emit someMessageBodyAvailable( this );
                        lk.relock();
                    }

                    //TODO/FIXME/BUG in case if startReadMessageBody is called not from responseReceived but later, 
                        //we will not send someMessageBodyAvailable signal for already read message body

                    if( m_httpStreamReader.state() == HttpStreamReader::readingMessageBody )
                        break; // wait for more data

                    //message body has been received with request
                    assert( m_httpStreamReader.state() == HttpStreamReader::messageDone || m_httpStreamReader.state() == HttpStreamReader::parseError );

                    //TODO/FIXME/BUG in case if startReadMessageBody is called not from responseReceived but later 
                        //and whole message body has been received with response, we MUST NOT emit done now, but wait or user to receive message body

                    //TODO: #AK looks like startReadMessageBody() call was a bad idea...

                    m_state = m_httpStreamReader.state() == HttpStreamReader::parseError ? sFailed : sDone;
                    lk.unlock();
                    emit done( this );
                    lk.relock();
                    break;
                }

                case sReadingMessageBody:
                {
                    Q_ASSERT( eventType != PollSet::etWrite );
                    if( eventType == PollSet::etError )
                    {
                        if( reconnectIfAppropriate() )
                            break;
                        NX_LOG( QString::fromLatin1("Error reading http response message body from %1").arg(m_url.toString()), cl_logDEBUG1 );
                        m_state = m_httpStreamReader.state() == HttpStreamReader::messageDone ? sDone : sFailed;
                        lk.unlock();
                        emit done( this );
                        lk.relock();
                        break;
                    }

                    int bytesRead = readAndParseHttp();
                    //TODO/IMPL reconnect in case of error
                    if( m_state != sReadingMessageBody )
                        aio::AIOService::instance()->removeFromWatch( m_socket, PollSet::etRead );
                    if( bytesRead > 0 )
                    {
                        lk.unlock();
                        emit someMessageBodyAvailable( this );
                        lk.relock();
                        if( m_terminated )
                            break;
                    }
                    if( m_state >= sFailed )
                    {
                        lk.unlock();
                        emit done( this );
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
            aio::AIOService::instance()->removeFromWatch( m_socket, PollSet::etRead );
            aio::AIOService::instance()->removeFromWatch( m_socket, PollSet::etWrite );
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
        //stopping client, if it is running
        terminate();
        {
            QMutexLocker lk( &m_mutex );
            m_terminated = false;
        }

        m_authorizationTried = false;
        m_customHeaders.clear();
        return doGetPrivate( url );
    }

    /*!
        Response is valid only after signal \a responseReceived() has been emitted
    */
    const HttpResponse* AsyncHttpClient::response() const
    {
        const HttpMessage& httpMsg = m_httpStreamReader.message();
        return httpMsg.type == MessageType::response ? httpMsg.response : NULL;
    }

    StringType AsyncHttpClient::contentType() const
    {
        const HttpMessage& httpMsg = m_httpStreamReader.message();
        if( httpMsg.type == MessageType::none )
            return StringType();
        HttpHeaders::const_iterator contentTypeIter = httpMsg.headers().find( "Content-Type" );
        if( contentTypeIter == httpMsg.headers().end() )
            return StringType();
        return contentTypeIter->second;
    }

    //!Start receiving message body
    /*!
        \return false if failed to start reading message body
    */
    bool AsyncHttpClient::startReadMessageBody()
    {
        if( m_state < sResponseReceived )
        {
            NX_LOG( QString::fromLatin1("HttpClient (%1). Message body cannot be read while in state %2").
                arg(m_url.toString()).arg(QLatin1String(toString(m_state))), cl_logDEBUG1 );
            return false;
        }
        else if( m_state > sResponseReceived )
        {
            return true;
        }

        m_state = sReadingMessageBody;

        if( !aio::AIOService::instance()->watchSocket( m_socket, PollSet::etRead, this ) )
        {
            NX_LOG( QString::fromLatin1("HttpClient (%1). Failed to start socket polling. %2").
                arg(m_url.toString()).arg(SystemError::getLastOSErrorText()), cl_logDEBUG1 );
            m_state = sResponseReceived;
            return false;
        }

        return true;
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

    void AsyncHttpClient::setSubsequentReconnectTries( int /*reconnectTries*/ )
    {
        //TODO/IMPL
    }

    void AsyncHttpClient::setTotalReconnectTries( int /*reconnectTries*/ )
    {
        //TODO/IMPL
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

    bool AsyncHttpClient::doGetPrivate( const QUrl& url )
    {
        if( m_socket )
        {
            aio::AIOService::instance()->removeFromWatch( m_socket, PollSet::etRead );
            aio::AIOService::instance()->removeFromWatch( m_socket, PollSet::etWrite );
            m_socket.clear();
        }
        m_state = sInit;

        m_httpStreamReader.resetState();

        m_socket = QSharedPointer<AbstractStreamSocket>( SocketFactory::createStreamSocket() );
        if( !m_socket->setNonBlockingMode( true ) )
        {
            NX_LOG( QString::fromLatin1("Failed to put socket to non blocking mode. %1").
                arg(SystemError::toString(SystemError::getLastOSErrorCode())), cl_logDEBUG1 );
            m_socket.clear();
            return false;
        }

        //starting async connect
        if( !m_socket->connect( url.host(), url.port(DEFAULT_HTTP_PORT), 0 ) )
        {
            NX_LOG( QString::fromLatin1("Failed to perform async connect to %1:%2. %3").
                arg(url.host()).arg(url.port()).arg(SystemError::toString(SystemError::getLastOSErrorCode())), cl_logDEBUG1 );
            m_socket.clear();
            return false;
        }

        m_url = url;
        m_state = sWaitingConnectToHost;

        //connect is done if socket is available for write
        if( !aio::AIOService::instance()->watchSocket( m_socket, PollSet::etWrite, this ) )
        {
            NX_LOG( QString::fromLatin1("Failed to add socket (connecting to %1:%2) to aio service. %3").
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
            if( SystemError::getLastOSErrorCode() == SystemError::wouldBlock )
                return 0;
            NX_LOG( QString::fromLatin1("AsyncHttpClient. Error reading socket (%1). Url %2").
                arg(SystemError::getLastOSErrorText()).arg(m_url.toString()), cl_logERROR );
            m_state = sFailed;
            return -1;
        }

        if( bytesRead == 0 )   //connection closed
        {
            //closing connection is a valid HTTP way to signal message end
            //m_state = m_httpStreamReader.state() == HttpStreamReader::messageDone ? sDone : sFailed;
            //TODO/IMPL check if whole message body is received (if message body size is known)
            m_httpStreamReader.flush();
            m_state = sDone;
            return 0;
        }

        if( !m_httpStreamReader.parseBytes( m_responseBuffer, bytesRead ) )
        {
            NX_LOG( QString::fromLatin1("Error parsing http response from %1. %2").
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

    void AsyncHttpClient::formRequest()
    {
        const bool useHttp11 = true;   //TODO/IMPL check. if we need it (e.g. we using keep-alive or requesting live capture)

        m_request.requestLine.method = nx_http::Method::GET;
        m_request.requestLine.url = m_url.path();
        m_request.requestLine.version = useHttp11 ? nx_http::Version::http_1_1 : nx_http::Version::http_1_0;
        if( !m_userAgent.isEmpty() )
            m_request.headers["User-Agent"] = m_userAgent.toLatin1();
        if( useHttp11 )
        {
            m_request.headers["Accept"] = "*/*";
            m_request.headers["Accept-Encoding"] = "gzip;q=1.0, identity;q=0.5, *;q=0";
            //m_request.headers["Accept-Encoding"] = "identity";
            m_request.headers["Host"] = m_url.host().toLatin1();
        }
        //adding user credentials
        if( !m_userName.isEmpty() || !m_userPassword.isEmpty() )
            m_request.headers[Header::Authorization::NAME] = Header::BasicAuthorization( m_userName.toLatin1(), m_userPassword.toLatin1() ).toString();

        //adding custom headers
        for( std::map<BufferType, BufferType>::const_iterator
            it = m_customHeaders.begin();
            it != m_customHeaders.end();
            ++it )
        {
            m_request.headers[it->first] = it->second;
        }
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
        if( (bytesSent == -1) && (SystemError::getLastOSErrorCode() != SystemError::wouldBlock) )
            return false;
        m_requestBytesSent += bytesSent;    //TODO it would be very usefull to use buffer with effective pop_front()
        return true;
    }

    bool AsyncHttpClient::reconnectIfAppropriate()
    {
        //TODO/IMPL we need reconnect and request entity from the point we stopped at
        return false;
    }

    bool AsyncHttpClient::resendRequestWithAuthorization( const nx_http::HttpResponse& response )
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

        if( qop.indexOf("auth-int") != -1 )
            return false;   //qop=auth-int is not supported

        BufferType nonceCount = "00000001";     //TODO/IMPL
        BufferType clientNonce = "0a4f113b";    //TODO/IMPL

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
        md5HashCalc.addData( "GET:" );
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
        m_customHeaders.insert( make_pair( Header::Authorization::NAME, authorizationStr ) );

        m_authorizationTried = true;
        return doGetPrivate( m_url );
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
