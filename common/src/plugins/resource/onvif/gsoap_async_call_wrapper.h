/**********************************************************
* 21 jul 2014
* a.kolesnikov
***********************************************************/

#ifndef GSOAP_ASYNC_CALL_WRAPPER_H
#define GSOAP_ASYNC_CALL_WRAPPER_H

#include <functional>

#include <onvif/stdsoap2.h>

#include <utils/common/joinable.h>
#include <utils/common/stoppable.h>
#include <utils/network/socket.h>
#include <utils/network/http/httptypes.h>
#include <utils/network/aio/aioservice.h>

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

    static int nullGsoapFconnect( struct soap*, const char*, const char*, int )
    {
        return SOAP_OK;
    }

    static int nullGsoapFdisconnect( struct soap* )
    {
        return SOAP_OK;
    }

    //Socket send through UdpSocket
    static int gsoapFsend( struct soap* soap, const char *s, size_t n )
    {
        GSoapAsyncCallWrapperBase* pThis = static_cast<GSoapAsyncCallWrapperBase*>(soap->user);
        return pThis->onGsoapSendData( s, n );
    }

    // avoid SOAP select call
    static size_t gsoapFrecv( struct soap* soap, char* data, size_t maxSize )
    {
        GSoapAsyncCallWrapperBase* pThis = static_cast<GSoapAsyncCallWrapperBase*>(soap->user);
        return pThis->onGsoapRecvData(data, maxSize);
    }
};

//!Async wrapper for gsoap call
template<typename SyncWrapper, typename Request, typename Response>
class GSoapAsyncCallWrapper
:
    public GSoapAsyncCallWrapperBase,
    public aio::AIOEventHandler,
    public QnStoppable,
    public QnJoinable
{
public:
    static const int SOAP_INTERRUPTED = -1;

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
        m_responseDataSize(0),
        m_responseDataPos(0),
        m_bytesSent(0)
    {
        m_responseBuffer.resize( 64*1024 );
    }

    ~GSoapAsyncCallWrapper()
    {
        pleaseStop();
        join();

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
            aio::AIOService::instance()->removeFromWatch(m_socket, aio::etRead);
            aio::AIOService::instance()->removeFromWatch(m_socket, aio::etWrite);
            m_socket.clear();
        }
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

        m_socket = QSharedPointer<AbstractStreamSocket>(SocketFactory::createStreamSocket());
        m_syncWrapper->getProxy()->soap->user = this;
        m_syncWrapper->getProxy()->soap->fconnect = nullGsoapFconnect;
        m_syncWrapper->getProxy()->soap->fdisconnect = nullGsoapFdisconnect;
        m_syncWrapper->getProxy()->soap->fsend = gsoapFsend;
        m_syncWrapper->getProxy()->soap->frecv = gsoapFrecv;
        m_syncWrapper->getProxy()->soap->fopen = NULL;
        m_syncWrapper->getProxy()->soap->fclose = NULL;
        m_syncWrapper->getProxy()->soap->socket = m_socket->handle();
        m_syncWrapper->getProxy()->soap->master = m_socket->handle();

        //serializing request
        m_request = request;
        m_state = sendingRequest;
        (m_syncWrapper->*m_syncFunc)(m_request, m_response);
        assert(!m_serializedRequest.isEmpty());
        soap_destroy(m_syncWrapper->getProxy()->soap);
        soap_end(m_syncWrapper->getProxy()->soap);
        //m_serializedRequest full

        const QUrl endpoint(QLatin1String(m_syncWrapper->endpoint()));
        return m_socket->setNonBlockingMode(true)
            && aio::AIOService::instance()->watchSocket(m_socket, aio::etWrite, this)
            && m_socket->connect(endpoint.host(), endpoint.port(nx_http::DEFAULT_HTTP_PORT));
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

        const int bytesToCopy = std::min<int>(m_responseDataSize - m_responseDataPos, (int)maxSize);
        memcpy(data, m_responseBuffer.constData() + m_responseDataPos, bytesToCopy);
        m_responseDataPos += bytesToCopy;
        return bytesToCopy;
    }

    virtual void eventTriggered(AbstractSocket* sock, aio::EventType eventType) throw()
    {
        assert( sock == m_socket.data() );
        switch( eventType )
        {
            case aio::etWrite:
            {
                assert(m_state == sendingRequest);
                const int bytesSent = m_socket->send(QByteArray::fromRawData(m_serializedRequest.constData() + m_bytesSent, m_serializedRequest.size() - m_bytesSent));
                if( bytesSent == -1 )
                {
                    m_state = done;
                    aio::AIOService::instance()->removeFromWatch(m_socket, aio::etWrite);
                    m_resultHandler(SOAP_FAULT);
                    return;
                }

                m_bytesSent += bytesSent;
                if( m_bytesSent == m_serializedRequest.size() )
                {
                    //request has been sent, receiving response
                    aio::AIOService::instance()->removeFromWatch(m_socket, aio::etWrite);
                    m_state = receivingResponse;
                    m_responseDataSize = 0;
                    aio::AIOService::instance()->watchSocket(m_socket, aio::etRead, this);
                }
                break;
            }

            case aio::etRead:
            {
                static const int MIN_SOCKET_READ_SIZE = 4096;
                static const int READ_BUFFER_GROW_STEP = 4096;

                assert(m_state == receivingResponse);
                if( m_responseBuffer.size() - m_responseDataSize < MIN_SOCKET_READ_SIZE )
                    m_responseBuffer.resize(m_responseBuffer.size() + READ_BUFFER_GROW_STEP);
                const int bytesRead = m_socket->recv(m_responseBuffer.data() + m_responseDataSize, m_responseBuffer.size() - m_responseDataSize);
                if( bytesRead > 0 )
                {
                    m_responseDataSize += bytesRead;
                    break;
                }
                //error or connection closed, trying to deserialize what we have
                aio::AIOService::instance()->removeFromWatch(m_socket, aio::etRead);
                deserializeResponse();
                break;
            }

            case aio::etTimedOut:
            {
                if( m_state == sendingRequest )
                {
                    aio::AIOService::instance()->removeFromWatch(m_socket, aio::etWrite);
                    m_resultHandler(SOAP_FAULT);
                }
                else if( m_state == receivingResponse )
                {
                    aio::AIOService::instance()->removeFromWatch(m_socket, aio::etWrite);
                    if( m_responseDataSize > 0 )
                        deserializeResponse();  //deserializing what we have
                    else
                        m_resultHandler(SOAP_FAULT);
                }
                break;
            }

            case aio::etError:
            {
                aio::AIOService::instance()->removeFromWatch(m_socket, aio::etWrite);
                m_resultHandler(SOAP_FAULT);
                break;
            }

            default:
                assert( false );
        }
    }

    void deserializeResponse()
    {
        m_responseDataPos = 0;
        const int resultCode = (m_syncWrapper->*m_syncFunc)(m_request, m_response);
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
    int m_responseDataSize;
    int m_responseDataPos;
    QSharedPointer<AbstractStreamSocket> m_socket;
    int m_bytesSent;
    std::function<void(int)> m_resultHandler;
};

#endif  //GSOAP_ASYNC_CALL_WRAPPER_H
