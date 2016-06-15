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

#include <QString>

#ifndef Q_MOC_RUN
#include <boost/optional.hpp>
#endif
#include "utils/common/hash.h"


namespace nx {

class DnsResolver;

namespace network {

enum class TransportProtocol
{
    udp,
    tcp,
    udt
};

static const size_t kUDPHeaderSize = 8;
static const size_t kIPHeaderSize = 20;
static const size_t kMaxUDPDatagramSize = 64*1024 - kUDPHeaderSize - kIPHeaderSize;

static const size_t kTypicalMtuSize = 1500;

}   //network
}   //nx

//!Represents ipv4 address. Supports conversion to QString and to uint32
/*!
    \note Not using QHostAddress because QHostAddress can trigger dns name lookup which depends on Qt sockets which we do not want to use
*/
class NX_NETWORK_API HostAddress
{
public:
    //!Creates 0.0.0.0 address
    HostAddress();
    ~HostAddress();

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
    bool isLocalIp() const;

    HostAddress& operator=( const HostAddress& rhs );
    HostAddress& operator=( HostAddress&& rhs );

    bool operator==( const HostAddress& right ) const;
    bool operator!=( const HostAddress& right ) const;
    bool operator<( const HostAddress& right ) const;

    struct in_addr inAddr(bool* ok = nullptr) const;

    /** 127.0.0.1 */
    static const HostAddress localhost;
    /** 0.0.0.0 */
    static const HostAddress anyHost;

private:
    mutable boost::optional<QString> m_addrStr;
    mutable struct in_addr m_sinAddr;
    //!if \a true \a m_sinAddr contains ip address corresponding to \a m_addrStr
    mutable bool m_addressResolved;

    void initializeFromString(const char* addrStr);

    // TODO: use IpAddress instead
    friend class nx::DnsResolver;
};

//!Represents host and port (e.g. 127.0.0.1:1234)
class NX_NETWORK_API SocketAddress
{
public:
    HostAddress address;
    quint16 port;

    SocketAddress();
    ~SocketAddress();
    SocketAddress(HostAddress _address, quint16 _port);
    SocketAddress(const QString& str);
    SocketAddress(const QByteArray& utf8Str);
    SocketAddress(const char* str);

    QString toString() const;
    bool operator==(const SocketAddress& rhs) const;
    bool operator!=(const SocketAddress& rhs) const;
    bool operator<(const SocketAddress& rhs) const;
    bool isNull() const;

    static const SocketAddress anyAddress;
private:
    void initializeFromString(const QString& str);
};

inline uint qHash(const SocketAddress &address) {
    return qHash(address.address.toString(), address.port);
}

Q_DECLARE_METATYPE(SocketAddress)

#endif  //SOCKET_COMMON_H
