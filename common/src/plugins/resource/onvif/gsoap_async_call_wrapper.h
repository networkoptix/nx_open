/**********************************************************
* 21 jul 2014
* a.kolesnikov
***********************************************************/

#ifndef GSOAP_ASYNC_CALL_WRAPPER_H
#define GSOAP_ASYNC_CALL_WRAPPER_H

#include <functional>
#include <type_traits>

#include <onvif/stdsoap2.h>

#include <utils/common/joinable.h>
#include <utils/common/stoppable.h>
#include <utils/network/socket.h>
#include <utils/network/http/httptypes.h>

#include "soap_wrapper.h"


class GSoapAsyncCallWrapperBase
{
public:
    /*!
        \return error code
    */
    virtual int onGsoapSendData( const char* data, size_t size ) = 0;
    /*!
        \return bytes read
    */
    virtual int onGsoapRecvData( char* data, size_t size ) = 0;
};

//!Async wrapper for gsoap call
template<typename SyncWrapper, typename Request, typename Response>
class GSoapAsyncCallWrapper
:
    public GSoapAsyncCallWrapperBase,
    public QnStoppable,
    public QnJoinable
{
public:
    static const int SOAP_INTERRUPTED = -1;
    static const int READ_BUF_SIZE = 32 * 1024;

    typedef int(SyncWrapper::*GSoapCallFuncType)(Request&, Response&);

    /*!
        \param syncWrapper 
        \param syncFunc Synchronous function to call
    */
    GSoapAsyncCallWrapper(SyncWrapper* syncWrapper, GSoapCallFuncType syncFunc)
    :
        m_syncWrapper(syncWrapper),
        m_syncFunc(syncFunc),
        m_state(init),
        m_responseDataPos(0),
        m_bytesSent(0)
    {
        m_responseBuffer.reserve( READ_BUF_SIZE );
    }

    ~GSoapAsyncCallWrapper()
    {
        pleaseStop();
        join();

        m_syncWrapper->getProxy()->soap->socket = SOAP_INVALID_SOCKET;
        m_syncWrapper->getProxy()->soap->master = SOAP_INVALID_SOCKET;
        soap_destroy(m_syncWrapper->getProxy()->soap);
        soap_end(m_syncWrapper->getProxy()->soap);
        delete m_syncWrapper;
        m_syncWrapper = nullptr;
    }

    virtual void pleaseStop() override
    {
    }

    /*!
        It is garanteed that after return of this method \a resultHandler is not running and will not be called
    */
    virtual void join() override
    {
        if( m_socket )
        {
            m_socket->cancelAsyncIO( aio::etRead );
            m_socket->cancelAsyncIO( aio::etWrite );
            m_socket.reset();
        }
    }

    template<class GSoapAsyncCallWrapperType>
    static int custom_soap_fsend( struct soap* soap, const char *s, size_t n )
    {
        GSoapAsyncCallWrapperType* pThis = static_cast<GSoapAsyncCallWrapperType*>(soap->user);
        return pThis->onGsoapSendData( s, n );
    }

    template<class GSoapAsyncCallWrapperType>
    static size_t custom_soap_frecv( struct soap* soap, char* data, size_t maxSize )
    {
        GSoapAsyncCallWrapperType* pThis = static_cast<GSoapAsyncCallWrapperType*>(soap->user);
        return pThis->onGsoapRecvData( data, maxSize );
    }

    //!Start async call
    /*!
        \param resultHandler Called on async call completion, receives GSoap result code or \a SOAP_INTERRUPTED
        \return \a true if async call has been started successfully
    */
    template<class ResultHandler>
    bool callAsync(const Request& request, ResultHandler resultHandler)
    {
        m_state = init;
        m_resultHandler = resultHandler;

        m_socket.reset( SocketFactory::createStreamSocket( false, SocketFactory::nttDisabled ) );
        m_syncWrapper->getProxy()->soap->user = this;
        m_syncWrapper->getProxy()->soap->fconnect = [](struct soap*, const char*, const char*, int) -> int { return SOAP_OK; };
        m_syncWrapper->getProxy()->soap->fdisconnect = [](struct soap*) -> int { return SOAP_OK; };
        m_syncWrapper->getProxy()->soap->fsend = &custom_soap_fsend<typename std::remove_reference<decltype(*this)>::type>;
        m_syncWrapper->getProxy()->soap->frecv = &custom_soap_frecv<typename std::remove_reference<decltype(*this)>::type>;
        m_syncWrapper->getProxy()->soap->fopen = NULL;
        m_syncWrapper->getProxy()->soap->fdisconnect = [](struct soap*) -> int { return SOAP_OK; };
        m_syncWrapper->getProxy()->soap->fclose = [](struct soap*) -> int { return SOAP_OK; };
        m_syncWrapper->getProxy()->soap->fclosesocket = [](struct soap*, SOAP_SOCKET) -> int { return SOAP_OK; };
        m_syncWrapper->getProxy()->soap->fshutdownsocket = [](struct soap*, SOAP_SOCKET, int) -> int { return SOAP_OK; };
        m_syncWrapper->getProxy()->soap->socket = m_socket->handle();
        m_syncWrapper->getProxy()->soap->master = m_socket->handle();

        //serializing request
        m_request = request;

        const QUrl endpoint(QLatin1String(m_syncWrapper->endpoint()));
        using namespace std::placeholders;
        if( !m_socket->setNonBlockingMode( true ) ||
            !m_socket->connectAsync(
                SocketAddress( endpoint.host(), endpoint.port( nx_http::DEFAULT_HTTP_PORT ) ),
                std::bind( &GSoapAsyncCallWrapper::onConnectCompleted, this, _1 ) ) )
        {
            m_socket.reset();
            return false;
        }

        return true;
    }

    SyncWrapper* syncWrapper() const
    {
        return m_syncWrapper;
    }

    const Response& response() const
    {
        return m_response;
    }

private:
    enum State
    {
        init,
        sendingRequest,
        receivingResponse,
        done
    };

    virtual int onGsoapSendData(const char* data, size_t size) override
    {
        if( m_state == sendingRequest )
            m_serializedRequest.append(data, (int)size);
        return SOAP_OK;
    }

    virtual int onGsoapRecvData(char* data, size_t maxSize) override
    {
        if( m_state < receivingResponse )
            return 0;  //serialized request

        const int bytesToCopy = std::min<int>( m_responseBuffer.size() - m_responseDataPos, (int)maxSize );
        memcpy(data, m_responseBuffer.constData() + m_responseDataPos, bytesToCopy);
        m_responseDataPos += bytesToCopy;
        return bytesToCopy;
    }

    void onConnectCompleted( SystemError::ErrorCode errorCode )
    {
        if( errorCode )
        {
            m_state = done;
            m_socket.reset();
            return m_resultHandler( SOAP_FAULT );
        }

        //serializing request
        m_state = sendingRequest;
        (m_syncWrapper->*m_syncFunc)(m_request, m_response);
        assert( !m_serializedRequest.isEmpty() );
        m_syncWrapper->getProxy()->soap->socket = SOAP_INVALID_SOCKET;
        m_syncWrapper->getProxy()->soap->master = SOAP_INVALID_SOCKET;
        soap_destroy( m_syncWrapper->getProxy()->soap );
        soap_end( m_syncWrapper->getProxy()->soap );

        //sending request
        using namespace std::placeholders;
        if( !m_socket->sendAsync( m_serializedRequest, std::bind( &GSoapAsyncCallWrapper::onRequestSent, this, _1, _2 ) ) )
        {
            m_state = done;
            m_socket.reset();
            return m_resultHandler( SOAP_FAULT );
        }
    }

    void onRequestSent( SystemError::ErrorCode errorCode, size_t bytesSent )
    {
        if( errorCode )
        {
            m_state = done;
            m_socket.reset();
            return m_resultHandler( SOAP_FAULT );
        }

        assert( bytesSent == m_serializedRequest.size() );
        m_state = receivingResponse;

        m_responseBuffer.reserve( READ_BUF_SIZE );
        m_responseBuffer.resize( 0 );
        using namespace std::placeholders;
        if( !m_socket->readSomeAsync( &m_responseBuffer, std::bind( &GSoapAsyncCallWrapper::onSomeBytesRead, this, _1, _2 ) ) )
        {
            m_state = done;
            m_socket.reset();
            return m_resultHandler( SOAP_FAULT );
        }
    }

    void onSomeBytesRead( SystemError::ErrorCode errorCode, size_t bytesRead )
    {
        static const int MIN_SOCKET_READ_SIZE = 4096;
        static const int READ_BUFFER_GROW_STEP = 4096;

        assert( m_state == receivingResponse );

        if( errorCode || bytesRead == 0 )
        {
            m_state = done;
            m_socket.reset();
            if( !m_responseBuffer.isEmpty() )
                deserializeResponse();  //error or connection closed, trying to deserialize what we have
            else
                m_resultHandler( SOAP_FAULT );
            return;
        }

        //reading until connection end
        //TODO #ak it is better to parse http response and detect message boundary

        if( m_responseBuffer.capacity() - m_responseBuffer.size() < MIN_SOCKET_READ_SIZE )
            m_responseBuffer.reserve( m_responseBuffer.capacity() + READ_BUFFER_GROW_STEP );

        using namespace std::placeholders;
        if( !m_socket->readSomeAsync( &m_responseBuffer, std::bind( &GSoapAsyncCallWrapper::onSomeBytesRead, this, _1, _2 ) ) )
        {
            m_state = done;
            m_socket.reset();
            return m_resultHandler( SOAP_FAULT );
        }
    }

    void deserializeResponse()
    {
        m_responseDataPos = 0;
        const int resultCode = (m_syncWrapper->*m_syncFunc)(m_request, m_response);
        m_syncWrapper->getProxy()->soap->socket = SOAP_INVALID_SOCKET;
        m_syncWrapper->getProxy()->soap->master = SOAP_INVALID_SOCKET;
        m_state = done;
        m_resultHandler(resultCode);
    }

private:
    Request m_request;
    Response m_response;
    SyncWrapper* m_syncWrapper;
    GSoapCallFuncType m_syncFunc;
    State m_state;
    QByteArray m_serializedRequest;
    QByteArray m_responseBuffer;
    int m_responseDataPos;
    std::unique_ptr<AbstractStreamSocket> m_socket;
    int m_bytesSent;
    std::function<void(int)> m_resultHandler;
};

#endif  //GSOAP_ASYNC_CALL_WRAPPER_H
