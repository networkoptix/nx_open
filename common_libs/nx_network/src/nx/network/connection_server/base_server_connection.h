/**********************************************************
* 19 dec 2013
* a.kolesnikov
***********************************************************/

#ifndef BASE_SERVER_CONNECTION_H
#define BASE_SERVER_CONNECTION_H

#include <forward_list>
#include <functional>
#include <memory>

#include <nx/network/abstract_socket.h>
#include <nx/network/aio/abstract_pollable.h>
#include <nx/utils/object_destruction_flag.h>
#include <nx/utils/move_only_func.h>
#include <utils/common/stoppable.h>

#include "stream_socket_server.h"


namespace nx_api
{
    static const size_t READ_BUFFER_CAPACITY = 16*1024;

    //!Contains common logic for server-side connection created by \a StreamSocketServer
    /*!
        \a CustomConnectionType MUST implement following methods:
        \code {*.cpp}
            //!Received data from remote host
            void bytesReceived( const nx::Buffer& buf );
            //!Submitted data has been sent, \a m_writeBuffer has some free space
            void readyToSendData();
        \endcode

        \a CustomConnectionManagerType MUST implement following types:
        \code {*.cpp}
            //!Type of connections, handled by server. BaseServerConnection<CustomConnectionType, CustomConnectionManagerType> MUST be convertible to ConnectionType with static_cast
            typedef ... ConnectionType;
        \endcode
        and method(s):
        \code {*.cpp}
            //!This connection is passed here just after socket has been terminated
            closeConnection( SystemError::ErrorCode reasonCode, CustomConnectionType* )
        \endcode

        \note This class is not thread-safe. All methods are expected to be executed in aio thread, undelying socket is bound to. 
            In other case, it is caller's responsibility to syunchronize access to the connection object.
        \note Despite absence of thread-safety simultaneous read/write operations are allowed in different threads
        \note This class instance can be safely freed in any event handler (i.e., in internal socket's aio thread)
        \note It is allowed to free instance within event handler
    */
    template<
        class CustomConnectionType
    > class BaseServerConnection
    :
            public nx::network::aio::AbstractPollable
    {
    public:
        typedef BaseServerConnection<CustomConnectionType> SelfType;

        /*!
            \param connectionManager When connection is finished, \a connectionManager->closeConnection(closeReason, this) is called
        */
        BaseServerConnection(
            StreamConnectionHolder<CustomConnectionType>* connectionManager,
            std::unique_ptr<AbstractCommunicatingSocket> streamSocket )
        :
            m_connectionManager(connectionManager),
            m_streamSocket(std::move(streamSocket)),
            m_bytesToSend(0)
        {
            m_readBuffer.reserve( READ_BUFFER_CAPACITY );
        }

        ~BaseServerConnection()
        {
            stopWhileInAioThread();
        }

        virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> handler) override
        {
            m_streamSocket->pleaseStop(
                [this, handler = std::move(handler)]
                {
                    stopWhileInAioThread();
                    handler();
                });
        }

        virtual nx::network::aio::AbstractAioThread* getAioThread() const override
        {
            return m_streamSocket->getAioThread();
        }

        virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override
        {
            m_streamSocket->bindToAioThread(aioThread);
        }

        virtual void post(nx::utils::MoveOnlyFunc<void()> func) override
        {
            m_streamSocket->post(std::move(func));
        }

        virtual void dispatch(nx::utils::MoveOnlyFunc<void()> func) override
        {
            m_streamSocket->dispatch(std::move(func));
        }

        //!Start receiving data from connection
        /*!
            \return \a false, if could not start asynchronous operation (this can happen due to lack of resources on host machine)
        */
        void startReadingConnection()
        {
            using namespace std::placeholders;
            m_streamSocket->readSomeAsync(
                &m_readBuffer,
                std::bind( &SelfType::onBytesRead, this, _1, _2 ) );
        }

        /*!
            \return \a true, if started async send operation
        */
        void sendBufAsync( const nx::Buffer& buf )
        {
            using namespace std::placeholders;
            m_streamSocket->sendAsync(
                buf,
                std::bind( &SelfType::onBytesSent, this, _1, _2 ) );
            m_bytesToSend = buf.size();
        }

        void closeConnection(SystemError::ErrorCode closeReasonCode)
        {
            m_connectionManager->closeConnection(
                closeReasonCode,
                static_cast<CustomConnectionType*>(this) );
        }

        //!Register handler to be executed when connection just about to be destroyed
        /*!
            \note It is unspecified which thread \a handler will be invoked in (usually, it is aio thread corresponding to the socket)
        */
        void registerCloseHandler(nx::utils::MoveOnlyFunc<void()> handler)
        {
            m_connectionCloseHandlers.push_front(std::move(handler));
        }

        const std::unique_ptr<AbstractCommunicatingSocket>& socket() const
        {
            return m_streamSocket;
        }

        /** Moves socket to the caller.
            \a BaseServerConnection instance MUST be deleted just after this call
        */
        std::unique_ptr<AbstractCommunicatingSocket> takeSocket()
        {
            return std::move(m_streamSocket);
        }

    protected:
        SocketAddress getForeignAddress() const
        {
            return m_streamSocket->getForeignAddress();
        }

        StreamConnectionHolder<CustomConnectionType>* connectionManager()
        {
            return m_connectionManager;
        }

    private:
        StreamConnectionHolder<CustomConnectionType>* m_connectionManager;
        std::unique_ptr<AbstractCommunicatingSocket> m_streamSocket;
        nx::Buffer m_readBuffer;
        size_t m_bytesToSend;
        std::forward_list<nx::utils::MoveOnlyFunc<void()>> m_connectionCloseHandlers;
        nx::utils::ObjectDestructionFlag m_connectionFreedFlag;

        void onBytesRead( SystemError::ErrorCode errorCode, size_t bytesRead )
        {
            using namespace std::placeholders;

            if( errorCode != SystemError::noError )
                return handleSocketError( errorCode );
            if( bytesRead == 0 )    //connection closed by remote peer
                return handleSocketError(SystemError::connectionReset);

            NX_ASSERT( (size_t)m_readBuffer.size() == bytesRead );

            {
                nx::utils::ObjectDestructionFlag::Watcher watcher(&m_connectionFreedFlag);
                static_cast<CustomConnectionType*>(this)->bytesReceived( m_readBuffer );
                if (watcher.objectDestroyed())
                    return; //connection has been removed by handler
            }

            m_readBuffer.resize( 0 );
            m_streamSocket->readSomeAsync(
                &m_readBuffer,
                std::bind(&SelfType::onBytesRead, this, _1, _2));
        }

        void onBytesSent( SystemError::ErrorCode errorCode, size_t count )
        {
            if( errorCode != SystemError::noError )
                return handleSocketError( errorCode );

            static_cast<void>(count);
            NX_ASSERT(count == m_bytesToSend);

            static_cast<CustomConnectionType*>(this)->readyToSendData();
        }

        void handleSocketError( SystemError::ErrorCode errorCode )
        {
            nx::utils::ObjectDestructionFlag::Watcher watcher(&m_connectionFreedFlag);
            switch( errorCode )
            {
                case SystemError::noError:
                    NX_ASSERT( false );
                    break;

                case SystemError::connectionReset:
                case SystemError::notConnected:
                    m_connectionManager->closeConnection(
                        errorCode,
						static_cast<CustomConnectionType*>(this) );
                    break;

                default:
                    m_connectionManager->closeConnection(
                        errorCode,
						static_cast<CustomConnectionType*>(this) );
                    break;
            }

            if (!watcher.objectDestroyed())
                triggerConnectionClosedEvent();
        }

        void triggerConnectionClosedEvent()
        {
            auto connectionCloseHandlers = std::move(m_connectionCloseHandlers);
            m_connectionCloseHandlers.clear();
            for (auto& connectionCloseHandler : connectionCloseHandlers)
                connectionCloseHandler();
        }

        void stopWhileInAioThread()
        {
            triggerConnectionClosedEvent();
        }
    };
}

#endif  //BASE_SERVER_CONNECTION_H
