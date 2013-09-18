/**********************************************************
* 18 sep 2013
* a.kolesnikov
***********************************************************/

#ifndef STREAM_SOCKET_SERVER_H
#define STREAM_SOCKET_SERVER_H

#include <utils/network/socket_common.h>
#include <utils/network/socket.h>


//!Accepts incoming connections and forwards them to specified processor
/*!
    \uses \a aio::AIOService
*/
class StreamSocketServer
{
public:
    //!Initialization. Binds to adddress \a addrToListen
    StreamSocketServer( const SocketAddress& addrToListen );

    //!Calls \a AbstractStreamServerSocket::listen
    bool listen();
};

#endif  //STREAM_SOCKET_SERVER_H
