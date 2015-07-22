/**********************************************************
* 21 jun 2015
* a.kolesnikov@networkoptix.com
***********************************************************/

#ifndef nx_cc_cloud_server_socket_h
#define nx_cc_cloud_server_socket_h

#include "../abstract_socket.h"


namespace nx_cc
{
    //!Accepts connections incoming via mediator
    /*!
        Listening hostname is reported to the mediator to listen on.
        \todo #ak what listening port should mean in this case?
    */
    class CloudServerSocket
    :
        public AbstractStreamServerSocket
    {
    public:
        //!Implementation of AbstractSocket::bind
        /*!
            \param endpoint Host address is reported to the mediator to listen on. port ???
        */
        virtual bool bind( const SocketAddress& endpoint ) override;

        //!Implementation of AbstractStreamServerSocket::listen
        virtual bool listen( int queueLen ) override;
        //!Implementation of AbstractStreamServerSocket::accept
        virtual AbstractStreamSocket* accept() override;
        //!Implementation of AbstractStreamServerSocket::cancelAsyncIO
        virtual void cancelAsyncIO( bool waitForRunningHandlerCompletion = true ) override;

    protected:
        //!Implementation of AbstractStreamServerSocket::acceptAsyncImpl
        virtual bool acceptAsyncImpl(
            std::function<void(
                SystemError::ErrorCode,
                AbstractStreamSocket* )>&& handler ) override;
    };
}

#endif  //nx_cc_cloud_server_socket_h
