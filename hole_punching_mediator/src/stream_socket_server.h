/**********************************************************
* 18 sep 2013
* a.kolesnikov
***********************************************************/

#ifndef STREAM_SOCKET_SERVER_H
#define STREAM_SOCKET_SERVER_H

#include <utils/network/abstract_socket.h>
#include <utils/network/socket_common.h>


//!Accepts incoming connections and forwards them to specified processor
/*!
    \uses \a aio::AIOService
*/
class StreamSocketServer
:
    public AbstractStreamServerSocket::AsyncAcceptHandler
{
public:
    //!Initialization
    /*!
        \param newConnectionHandler Receives accepted connections
    */
    StreamSocketServer( AbstractStreamServerSocket::AsyncAcceptHandler* newConnectionHandler );

    //!Binds to adddress \a addrToListen
    /*!
        \return false of error. Use \a SystemError::getLastOSErrorCode() for error information
    */
    bool bind( const SocketAddress& addrToListen );
    //!Calls \a AbstractStreamServerSocket::listen
    bool listen();

private:
    AbstractStreamServerSocket::AsyncAcceptHandler* m_newConnectionHandler;

    virtual void newConnectionAccepted( AbstractStreamServerSocket* serverSocket, AbstractStreamSocket* newConnection ) override;
};

#endif  //STREAM_SOCKET_SERVER_H
