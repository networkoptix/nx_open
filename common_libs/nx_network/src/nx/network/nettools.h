#pragma once

#include <QtNetwork/QHostAddress>
#include <QtNetwork/QUdpSocket>
#include <QtNetwork/QNetworkAddressEntry>

#include <nx/network/socket_common.h>
#include "mac_address.h"

namespace nx {
namespace network {

static const int ping_timeout = 300;

struct CLSubNetState;

typedef QList<quint32> CLIPList;

struct NX_NETWORK_API QnInterfaceAndAddr
{
    QnInterfaceAndAddr(QString name_, QHostAddress address_, QHostAddress netMask_, const QNetworkInterface& _netIf)
        : name(name_),
          address(address_),
          netMask(netMask_),
          netIf(_netIf)
    {
    }

    QHostAddress subNetworkAddress() const;
    QHostAddress broadcastAddress() const;
    bool isHostBelongToIpv4Network(const QHostAddress& address) const;

    QString name;
    QHostAddress address;
    QHostAddress netMask;
    QNetworkInterface netIf;


};

inline bool operator ==(const QnInterfaceAndAddr& lhs, const QnInterfaceAndAddr& rhs)
{
    return  lhs.name == rhs.name
        && lhs.address == rhs.address
        && lhs.netMask == rhs.netMask;
}

inline uint qHash(const QnInterfaceAndAddr& iface, uint seed=0)
{
    return  qHash(iface.name, seed^0xa03f)
        + qHash(iface.address, seed^0x17a317a3)
        + qHash(iface.netMask, seed^0x17a317a3);
}

enum class InterfaceListPolicy
{
    oneAddressPerInterface, //< Return interface and its first IP address.
    allowInterfacesWithoutAddress, //< Return interface even it doesn't have any IP addresses.
    keepAllAddressesPerInterface, //< Return several records if interface has several IP addresses.
    count
};

/**
 * Returns list of network interfaces.
 * @param InterfaceListPolicy: see description above.
 */

typedef QList<QnInterfaceAndAddr> QnInterfaceAndAddrList;
QList<QnInterfaceAndAddr> NX_NETWORK_API getAllIPv4Interfaces(
    InterfaceListPolicy policy = InterfaceListPolicy::oneAddressPerInterface);

// returns list of IPv4 addresses of current machine. Skip 127.0.0.1 and addresses we can't bind to.
QList<QHostAddress> NX_NETWORK_API allLocalIpV4Addresses();

/** Filter mask for allLocalAddresses()*/
enum AddressFilter
{
    ipV4 = 1 << 0,
    ipV6 = 1 << 1,
    noLocal = 1 << 2,
    noLoopback = 1 << 3
};
Q_DECLARE_FLAGS(AddressFilters, AddressFilter)
Q_DECLARE_OPERATORS_FOR_FLAGS(AddressFilters)
/**
 * returns list of all IP addresses of current machine. Filters result by filter param.
 */
QList<HostAddress> NX_NETWORK_API allLocalAddresses(AddressFilters filter);

//returns list of all IPV4 QNetworkAddressEntries of current machine; this function takes time;
QList<QNetworkAddressEntry> NX_NETWORK_API getAllIPv4AddressEntries();

/**
 * Set filter for interface list
 */
void NX_NETWORK_API setInterfaceListFilter(const QList<QHostAddress>& ifList);

void NX_NETWORK_API removeARPrecord(const QHostAddress& ip);

MacAddress NX_NETWORK_API getMacByIP(const QString& host, bool net = true);
MacAddress NX_NETWORK_API getMacByIP(const QHostAddress& ip, bool net = true);

//QHostAddress NX_NETWORK_API getGatewayOfIf(const QString& netIf);

// returns all pingable hosts in the range
QList<QHostAddress> NX_NETWORK_API pingableAddresses(const QHostAddress& startAddr, const QHostAddress& endAddr, int threads);

//bool bindToInterface(QUdpSocket& sock, const QnInterfaceAndAddr& iface, int port = 0, QUdpSocket::BindMode mode = QUdpSocket::DefaultForPlatform);

bool NX_NETWORK_API isIpv4Address(const QString& addr);
// TODO: #ak Remove this method if favor of AddressResolver.
QHostAddress NX_NETWORK_API resolveAddress(const QString& addr);

int NX_NETWORK_API strEqualAmount(const char* str1, const char* str2);
bool NX_NETWORK_API isNewDiscoveryAddressBetter(
    const HostAddress& host,
    const QHostAddress& newAddress,
    const QHostAddress& oldAddress );

static const int MAC_ADDR_LEN = 18;
/*!
    \param host If function succeeds *host contains pointer to statically-allocated buffer,
        so it MUST NOT be freed!
    \return 0 on success, -1 in case of error. Use errno to get error code
*/
int NX_NETWORK_API getMacFromPrimaryIF(char  MAC_str[MAC_ADDR_LEN], char** host);
QString NX_NETWORK_API getMacFromPrimaryIF();

QSet<QString> NX_NETWORK_API getLocalIpV4AddressList();

} // namespace network
} // namespace nx
