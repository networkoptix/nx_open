/**********************************************************
* 22 nov 2012
* a.kolesnikov
***********************************************************/

#include "asynchttpclient.h"

#include "../aio/aioservice.h"
#include "../../common/log.h"
#include "../../common/systemerror.h"


//TODO/IMPL add Digit authentication

namespace nx_http
{
    static const size_t RESPONSE_BUFFER_SIZE = 16*1024;

    AsyncHttpClient::AsyncHttpClient()
    :
        m_state( sInit ),
        m_requestBytesSent( 0 )
    {
        m_responseBuffer.resize(RESPONSE_BUFFER_SIZE);
    }

    AsyncHttpClient::~AsyncHttpClient()
    {
    }

    //!Implementation of aio::AIOEventHandler::eventTriggered
    void AsyncHttpClient::eventTriggered( Socket* sock, PollSet::EventType eventType )
    {
        Q_ASSERT( sock == m_socket.data() );
        for( ;; )
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
                            emit tcpConnectionEstablished( this );
                            continue;

                        case PollSet::etError:
                            cl_log.log( QString::fromLatin1("Failed to connect to %1:%2. %3").arg(m_url.host()).arg(m_url.port()).
                                arg(SystemError::toString(m_socket->prevErrorCode())), cl_logWARNING );
                            if( reconnectIfAppropriate() )
                                break;
                            m_state = sFailed;
                            aio::AIOService::instance()->removeFromWatch( m_socket, PollSet::etWrite );
                            emit done( this );
                            break;
                    }
                    break;

                case sSendingRequest:
                    if( eventType == PollSet::etError )
                    {
                        if( reconnectIfAppropriate() )
                            break;
                        cl_log.log( QString::fromLatin1("Error sending http request to %1. %2").
                            arg(m_url.toString()).arg(SystemError::toString(m_socket->prevErrorCode())), cl_logWARNING );
                        m_state = m_httpStreamReader.state() == HttpStreamReader::messageDone ? sDone : sFailed;
                        emit done( this );
                        break;
                    }

                    if( !sendRequest() )
                    {
                        cl_log.log( QString::fromLatin1("Failed to send request to %1. %2").arg(m_url.toString()).arg(SystemError::getLastOSErrorText()), cl_logWARNING );
                        m_state = sFailed;
                        aio::AIOService::instance()->removeFromWatch( m_socket, PollSet::etWrite );
                        emit done( this );
                        break;
                    }
                    if( m_requestBytesSent == m_requestBuffer.size() )
                    {
                        cl_log.log( QString::fromLatin1("Http request has been successfully sent to %1").arg(m_url.toString()), cl_logDEBUG1 );
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
                        cl_log.log( QString::fromLatin1("Error reading http response from %1. %2").
                            arg(m_url.toString()).arg(SystemError::toString(m_socket->prevErrorCode())), cl_logWARNING );
                        m_state = m_httpStreamReader.state() == HttpStreamReader::messageDone ? sDone : sFailed;
                        emit done( this );
                        break;
                    }

                    readAndParseHttp();
                    //TODO/IMPL reconnect in case of error
                    if( m_state >= sFailed )
                        emit done( this );

                    if( m_httpStreamReader.state() == HttpStreamReader::readingMessageHeaders )
                        break;

                    //read http message headers
                    if( m_httpStreamReader.message().type != nx_http::MessageType::response )
                    {
                        cl_log.log( QString::fromLatin1("Unexpectedly received request from %1:%2 while expecting response! Ignoring...").
                            arg(m_url.host()).arg(m_url.port()), cl_logDEBUG1 );
                        m_state = sFailed;
                        emit done( this );
                        break;
                    }
                    //response read
                    cl_log.log( QString::fromLatin1("Http response from %1 has been successfully read. Status line: %2(%3)").
                        arg(m_url.toString()).arg(m_httpStreamReader.message().response->statusLine.statusCode).
                        arg(QLatin1String(m_httpStreamReader.message().response->statusLine.reasonPhrase)), cl_logDEBUG1 );
                    //TODO/IMPL should only call removeFromWatch if startReadMessageBody has not been called from responseReceived connected slot
                    aio::AIOService::instance()->removeFromWatch( m_socket, PollSet::etRead );
                    m_state = sResponseReceived;
                    emit responseReceived( this );
                    if( m_httpStreamReader.state() == HttpStreamReader::messageDone )
                    {
                        m_state = sDone;
                        emit done( this );
                    }
                    //TODO/FIXME/BUG in case if startReadMessageBody is called not from responseReceived but later, we will not send someMessageBodyAvailable signal
                    if( m_state == sReadingMessageBody &&
                        m_httpStreamReader.state() == HttpStreamReader::readingMessageBody && 
                        m_httpStreamReader.messageBodyBufferSize() > 0 )
                    {
                        emit someMessageBodyAvailable( this );
                    }
                    break;
                }

                case sReadingMessageBody:
                {
                    Q_ASSERT( eventType != PollSet::etWrite );
                    if( eventType == PollSet::etError )
                    {
                        if( reconnectIfAppropriate() )
                            break;
                        cl_log.log( QString::fromLatin1("Error reading http response message body from %1. %2").
                            arg(m_url.toString()).arg(SystemError::toString(m_socket->prevErrorCode())), cl_logWARNING );
                        m_state = m_httpStreamReader.state() == HttpStreamReader::messageDone ? sDone : sFailed;
                        emit done( this );
                        break;
                    }

                    int bytesRead = readAndParseHttp();
                    //TODO/IMPL reconnect in case of error
                    if( m_state != sReadingMessageBody )
                        aio::AIOService::instance()->removeFromWatch( m_socket, PollSet::etRead );
                    if( bytesRead > 0 )
                        emit someMessageBodyAvailable( this );
                    if( m_state >= sFailed )
                        emit done( this );
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

        if( isScheduledForRemoval() )
        {
            //removing this
            aio::AIOService::instance()->removeFromWatch( m_socket, PollSet::etRead );
            aio::AIOService::instance()->removeFromWatch( m_socket, PollSet::etWrite );
            delete this;
        }
    }

    AsyncHttpClient::State AsyncHttpClient::state() const
    {
        return m_state;
    }

    //!Start request to \a url
    /*!
        \return true, if socket is created and async connect is started. false otherwise
        To get error description use SystemError::getLastOSErrorCode()
    */
    bool AsyncHttpClient::doGet( const QUrl& url )
    {
        if( m_socket )
        {
            aio::AIOService::instance()->removeFromWatch( m_socket, PollSet::etRead );
            aio::AIOService::instance()->removeFromWatch( m_socket, PollSet::etWrite );
            m_socket.clear();
        }
        m_state = sInit;

        m_socket = QSharedPointer<TCPSocket>( new TCPSocket() );
        if( !m_socket->setNonBlockingMode( true ) )
        {
            cl_log.log( QString::fromLatin1("Failed to put socket to non blocking mode. %1").
                arg(SystemError::toString(SystemError::getLastOSErrorCode())), cl_logDEBUG1 );
            m_socket.clear();
            return false;
        }

        //starting async connect
        if( !m_socket->connect( url.host(), url.port(DEFAULT_HTTP_PORT) ) )
        {
            cl_log.log( QString::fromLatin1("Failed to perform async connect to %1:%2. %3").
                arg(url.host()).arg(url.port()).arg(SystemError::toString(SystemError::getLastOSErrorCode())), cl_logDEBUG1 );
            m_socket.clear();
            return false;
        }

        m_url = url;
        m_state = sWaitingConnectToHost;

        //connect is done if socket is available for write
        if( !aio::AIOService::instance()->watchSocket( m_socket, PollSet::etWrite, this ) )
        {
            cl_log.log( QString::fromLatin1("Failed to add socket (connecting to %1:%2) to aio service. %3").
                arg(url.host()).arg(url.port()).arg(SystemError::toString(SystemError::getLastOSErrorCode())), cl_logDEBUG1 );
            m_socket.clear();
            return false;
        }

        return true;
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
        if( m_state != sResponseReceived )
        {
            cl_log.log( QString::fromLatin1("HttpClient (%1). Message body cannot be read while in state %2").
                arg(m_url.toString()).arg(QLatin1String(toString(m_state))), cl_logWARNING );
            return false;
        }

        m_state = sReadingMessageBody;

        if( !aio::AIOService::instance()->watchSocket( m_socket, PollSet::etRead, this ) )
        {
            cl_log.log( QString::fromLatin1("HttpClient (%1). Failed to start socket polling. %2").
                arg(m_url.toString()).arg(SystemError::getLastOSErrorText()), cl_logWARNING );
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

    void AsyncHttpClient::setSubsequentReconnectTries( int reconnectTries )
    {
        //TODO/IMPL
    }

    void AsyncHttpClient::setTotalReconnectTries( int reconnectTries )
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

    int AsyncHttpClient::readAndParseHttp()
    {
        int bytesRead = m_socket->recv( m_responseBuffer.data(), m_responseBuffer.size() );
        if( bytesRead < 0 )         //read error
        {
            if( SystemError::getLastOSErrorCode() == SystemError::wouldBlock )
                return 0;
            cl_log.log( QString::fromLatin1("AsyncHttpClient. Error reading socket (%1). Url %2").
                arg(SystemError::getLastOSErrorText()).arg(m_url.toString()), cl_logERROR );
            m_state = sFailed;
            return -1;
        }

        if( bytesRead == 0 )   //connection closed
        {
            //closing connection is a valid HTTP way to signal message end
            //m_state = m_httpStreamReader.state() == HttpStreamReader::messageDone ? sDone : sFailed;
            //TODO/IMPL check if whole message body is received (if message body size is known)
            m_state = sDone;
            return 0;
        }

        if( !m_httpStreamReader.parseBytes( m_responseBuffer, bytesRead ) )
        {
            cl_log.log( QString::fromLatin1("Error parsing http response from %1. %2").
                arg(m_url.toString()).arg(m_httpStreamReader.errorText()), cl_logWARNING );
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
            m_request.headers["Accept-Encoding"] = "identity";
            m_request.headers["Host"] = m_url.host().toLatin1();
        }
        //adding user credentials
        if( !m_userName.isEmpty() || !m_userPassword.isEmpty() )
            m_request.headers[Header::Authorization::NAME] = Header::BasicAuthorization( m_userName.toLatin1(), m_userPassword.toLatin1() ).toString();
        //TODO/IMPL add custom headers
    }

    void AsyncHttpClient::serializeRequest()
    {
        m_requestBuffer.clear();
        m_request.serialize( &m_requestBuffer );
        m_requestBytesSent = 0;
    }

    bool AsyncHttpClient::sendRequest()
    {
        Q_ASSERT( m_requestBytesSent < m_requestBuffer.size() );
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
