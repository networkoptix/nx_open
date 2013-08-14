////////////////////////////////////////////////////////////
// 18 aug 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef NAMED_PIPE_SERVER_H
#define NAMED_PIPE_SERVER_H

#include <utils/common/systemerror.h>

#include "named_pipe_socket.h"


class NamedPipeServerImpl;

//!Named pipe for IPC
class NamedPipeServer
{
public:
    NamedPipeServer();
    virtual ~NamedPipeServer();

    //!Openes \a pipeName and listenes it for connections
    /*!
        This method returns immediately
    */
    SystemError::ErrorCode listen( const QString& pipeName );
    /*!
        Blocks till new connection is accepted, timeout expires or error occurs
        \param timeoutMillis max time to wait for incoming connection (millis)
        \param sock If return value \a SystemError::noError, \a *sock is assigned with newly-created socket. It must be freed by calling party
    */
    SystemError::ErrorCode accept( NamedPipeSocket** sock, int timeoutMillis = -1 );

private:
    NamedPipeServerImpl* m_impl;
};

#endif  //NAMED_PIPE_SERVER_H
