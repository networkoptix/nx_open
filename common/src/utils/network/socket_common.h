/**********************************************************
* 30 aug 2013
* a.kolesnikov
***********************************************************/

#ifndef SOCKET_COMMON_H
#define SOCKET_COMMON_H

#ifdef _WIN32
#   include <winsock2.h>
#   include <in6addr.h>
#   include <ws2ipdef.h>

// Windows does not support this flag, so we emulate it
#   define MSG_DONTWAIT 0x01000000
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
    HostAddress( const HostAddress& rhs );
    HostAddress( HostAddress&& rhs );

    HostAddress( const in_addr& addr );
    HostAddress( const in6_addr& addr = in6addr_any );

    HostAddress( const QString& addrStr );
    HostAddress( const char* addrStr );

    HostAddress& operator=( const HostAddress& rhs );
    HostAddress& operator=( HostAddress&& rhs );

    /**
     * WARNING: There is a logical bug in here:
     *  "!(a > b) && !(a < b)" does not mean "a == b"
     */
    bool operator==( const HostAddress& right ) const;
    bool operator<( const HostAddress& right ) const;

    /** Domain name or IP v4 (if can be converted) or IP v6 */
    const QString& toString() const;

    /** IP v4 if address is v4 or v6 which can be converted to v4 */
    const boost::optional<in_addr>& ipV4() const;

    /** IP v6 if address is v6 or v4 converted to v6 */
    const boost::optional<in6_addr>& ipV6() const;

    bool isLocal() const;
    bool isResolved() const;

    static const HostAddress localhost;
    static const HostAddress anyHost;

    static boost::optional<QString> ipToString(const in_addr& addr);
    static boost::optional<QString> ipToString(const in6_addr& addr);

    static boost::optional<in_addr> ipV4from(const QString& ip);
    static boost::optional<in6_addr> ipV6from(const QString& ip);

private:
    mutable boost::optional<QString> m_string;
    mutable boost::optional<in_addr> m_ipV4;
    mutable boost::optional<in6_addr> m_ipV6;

    friend class HostAddressResolver;
};

//!Represents host and port (e.g. 127.0.0.1:1234)
class SocketAddress
{
public:
    HostAddress address;
    int port;

    SocketAddress( const HostAddress& _address = HostAddress(), unsigned short _port = 0 );
    SocketAddress( const QString& str );

    QString toString() const;
    bool operator==( const SocketAddress& rhs ) const;
    bool isNull() const;

    static QString trimIpV6(const QString& ip);
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
