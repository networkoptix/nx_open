////////////////////////////////////////////////////////////
// 18 aug 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef NAMED_PIPE_SOCKET_H
#define NAMED_PIPE_SOCKET_H

#include <utils/common/systemerror.h>


class NamedPipeSocketImpl;

//!Named pipe for IPC
class NamedPipeSocket
{
    friend class NamedPipeServer;

public:
    NamedPipeSocket();
    virtual ~NamedPipeSocket();

    //!
    /*!
        Blocks till connection is complete
        \param timeoutMillis if -1, can wait indefinetely
    */
    SystemError::ErrorCode connectToServerSync( const QString& pipeName, int timeoutMillis = -1 );
    //!Writes in synchronous mode
    SystemError::ErrorCode write( const void* buf, unsigned int bytesToWrite, unsigned int* const bytesWritten );
    //!Reads in synchronous mode
    SystemError::ErrorCode read( void* buf, unsigned int bytesToRead, unsigned int* const bytesRead );

    SystemError::ErrorCode flush();

private:
    NamedPipeSocketImpl* m_impl;

    //!This initializer is used by NamedPipeServer
    NamedPipeSocket( NamedPipeSocketImpl* implToUse );
};

#endif  //NAMED_PIPE_SOCKET_H
