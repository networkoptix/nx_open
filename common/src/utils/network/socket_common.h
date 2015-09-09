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
#include "utils/common/hash.h"

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

    HostAddress( const HostAddress& rhs );
    HostAddress( HostAddress&& rhs );
    HostAddress( const struct in_addr& sinAddr );

    /*!
        \param _ipv4 ipv4 address in local byte order
    */
    HostAddress( uint32_t _ipv4 );

    /*!
     *  \param _ipv6 ipv6 address in local byte order (ipv4-mapped
     */
    HostAddress( const QByteArray& _ipv6 );

    HostAddress( const QString& addrStr );
    HostAddress( const char* addrStr );

    //!Returns ip in local byte order
    /*!
        \note This method can trigger blocking address resolve. Check 
    */
    uint32_t ipv4() const;

    //!Returns ip4-mapped ipv6
    QByteArray ipv6() const;

    QString toString() const;
    //!Returns \a true if address is resolved. I.e., it's ip address is known
    bool isResolved() const;

    HostAddress& operator=( const HostAddress& rhs );
    HostAddress& operator=( HostAddress&& rhs );

    bool operator==( const HostAddress& right ) const;
    bool operator!=( const HostAddress& right ) const;
    bool operator<( const HostAddress& right ) const;

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
    quint16 port;

    SocketAddress();
    SocketAddress( HostAddress _address, quint16 _port );
    SocketAddress( const QString& str );
    SocketAddress( const char* str );

    QString toString() const;
    bool operator==( const SocketAddress& rhs ) const;
    bool operator!=( const SocketAddress& rhs ) const;
    bool operator<( const SocketAddress& rhs ) const;
    bool isNull() const;

private:
    void initializeFromString(const QString& str);
};

inline uint qHash(const SocketAddress &address) {
    return qHash(address.address.toString(), address.port);
}

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

Q_DECLARE_METATYPE(SocketAddress)

#endif  //SOCKET_COMMON_H
