/**********************************************************
* 25 feb 2015
* a.kolesnikov
***********************************************************/

#include "file_socket.h"


FileSocket::FileSocket( const std::string& filePath )
:
    m_filePath( filePath ),
    m_bytesRead( 0 )
{
}

void FileSocket::close()
{
    return m_file.close();
}

bool FileSocket::isClosed() const
{
    return !m_file.is_open();
}

bool FileSocket::connect(
    const SocketAddress& remoteSocketAddress,
    unsigned int timeoutMillis )
{
    DummySocket::connect( remoteSocketAddress, timeoutMillis );

    if( m_file.is_open() )
        m_file.close();
    m_file.open( m_filePath, std::ifstream::binary );
    return m_file.is_open();
}

int FileSocket::recv( void* buffer, unsigned int bufferLen, int flags )
{
    m_file.read( (char*)buffer, bufferLen );
    m_bytesRead += m_file.gcount();
    return m_file.gcount();
}

int FileSocket::send( const void* buffer, unsigned int bufferLen )
{
    return bufferLen;
}

bool FileSocket::isConnected() const
{
    return m_file.is_open();
}
