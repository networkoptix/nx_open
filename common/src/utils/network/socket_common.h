/**********************************************************
* 30 aug 2013
* a.kolesnikov
***********************************************************/

#ifndef SOCKET_COMMON_H
#define SOCKET_COMMON_H

#ifdef _WIN32
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
    /*!
        \param _ipv4 ipv4 address in local byte order
    */
    HostAddress( uint32_t _ipv4 );
    HostAddress( const QString& addrStr );

    //!Returns ip in local byte order
    uint32_t ipv4() const;
    QString toString() const;

    static const HostAddress localhost;
    static const HostAddress anyHost;

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
    /*!
        \param addrStr. String in format "host:port". Also, following strings are valid:\n
            - :port (host is considered \a HostAddress::anyHost)
            - host (port is zero)
    */
    SocketAddress( const QString& addrStr );

    QString toString() const;
};

#endif  //SOCKET_COMMON_H
