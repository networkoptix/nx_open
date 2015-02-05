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
/*!
    \note Not using QHostAddress because QHostAddress can trigger dns name lookup which depends on Qt sockets which we do not want to use
*/
class HostAddress
{
public:
    //!Creates 0.0.0.0 address
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

    bool operator==( const HostAddress& right ) const;

    struct in_addr inAddr(bool* ok = nullptr) const;

    static const HostAddress localhost;
    static const HostAddress anyHost;

private:
    mutable boost::optional<QString> m_addrStr;
    struct in_addr m_sinAddr;
    //!if \a true \a m_sinAddr contains ip address corresponding to \a m_addrStr
    bool m_addressResolved;

    friend class HostAddressResolver;
};

//!Represents host and port (e.g. 127.0.0.1:1234)
class SocketAddress
{
public:
    HostAddress address;
    int port;

    SocketAddress( const HostAddress& _address = HostAddress(), unsigned short _port = 0 )
    :
        address( _address ),
        port( _port )
    {
    }

    SocketAddress( const QString& str )
    :
        port( 0 )
    {
        int sepPos = str.indexOf(QLatin1Char(':'));
        if( sepPos == -1 )
        {
            address = HostAddress(str);
        }
        else
        {
            address = HostAddress(str.mid( 0, sepPos ));
            port = str.mid( sepPos+1 ).toInt();
        }
    }

    QString toString() const
    {
        return
            address.toString() +
            (port > 0 ? QString::fromLatin1(":%1").arg(port) : QString());
    }

    bool operator==( const SocketAddress& rhs ) const
    {
        return address == rhs.address && port == rhs.port;
    }
};

class SocketGlobalRuntimeInternal;

//!This class instance MUST be created for sockets to be operational
class SocketGlobalRuntime
{
public:
    SocketGlobalRuntime();
    ~SocketGlobalRuntime();

    static SocketGlobalRuntime* instance();

private:
    SocketGlobalRuntimeInternal* m_data;
};

#endif  //SOCKET_COMMON_H
