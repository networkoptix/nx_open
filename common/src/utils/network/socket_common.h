/**********************************************************
* 30 aug 2013
* a.kolesnikov
***********************************************************/

#ifndef SOCKET_COMMON_H
#define SOCKET_COMMON_H

#ifdef Q_OS_WIN
#   include <winsock2.h>
#else
#   include <sys/socket.h>
#   include <netinet/in.h>
#   include <arpa/inet.h>
#endif

#include <stdint.h>

#include <boost/optional.hpp>

#include <QString>


//!Represents ipv4 address. Supports conversion to QString and to uint32
class HostAddress
{
public:
    HostAddress();
    HostAddress( struct in_addr& sinAddr );
    HostAddress( const QString& addrStr );

    //!Returns ip in local byte order
    uint32_t ipv4() const;
    QString toString() const;

private:
    mutable boost::optional<QString> m_addrStr;
    struct in_addr m_sinAddr;
};

//!Pair "host address":port
class SocketAddress
{
public:
    HostAddress address;
    unsigned short port;

    SocketAddress( const HostAddress& _address = HostAddress(), unsigned short _port = 0 );

    QString toString() const;
};

#endif  //SOCKET_COMMON_H
