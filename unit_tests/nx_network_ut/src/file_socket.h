/**********************************************************
* 25 feb 2015
* a.kolesnikov
***********************************************************/

#ifndef FILE_SOCKET_H
#define FILE_SOCKET_H

#include <fstream>

#include "dummy_socket.h"


namespace nx {
namespace network {

//!Reads data from file. Sent data is just ignored
class FileSocket
:
    public DummySocket
{
public:
    FileSocket( const std::string& filePath );

    virtual bool close() override;
    virtual bool isClosed() const override;

    virtual bool connect(
        const SocketAddress& remoteSocketAddress,
        unsigned int timeoutMillis = kDefaultTimeoutMillis) override;
    virtual int recv( void* buffer, unsigned int bufferLen, int flags = 0 ) override;
    virtual int send( const void* buffer, unsigned int bufferLen ) override;
    virtual bool isConnected() const override;

private:
    std::string m_filePath;
    std::ifstream m_file;
    size_t m_bytesRead;
};

}   //network
}   //nx

#endif  //FILE_SOCKET_H
