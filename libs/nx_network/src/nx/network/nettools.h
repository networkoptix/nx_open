// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <set>

#include <QtNetwork/QHostAddress>
#include <QtNetwork/QNetworkAddressEntry>
#include <QtNetwork/QUdpSocket>

#include <nx/network/socket_common.h>
#include <nx/utils/mac_address.h>

namespace nx::network {

struct NX_NETWORK_API QnInterfaceAndAddr
{
    QnInterfaceAndAddr(
        const QString& name,
        const QHostAddress& address,
        const QHostAddress& netMask,
        const QNetworkInterface& netIf)
        :
        name(name),
        address(address),
        netMask(netMask),
        netIf(netIf)
    {
    }

    QHostAddress subNetworkAddress() const;
    QHostAddress broadcastAddress() const;
    bool isHostBelongToIpv4Network(const QHostAddress& address) const;
    QString toString() const;

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

inline size_t qHash(const QnInterfaceAndAddr& iface, uint seed=0)
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
    count,
};

using QnInterfaceAndAddrList = QList<QnInterfaceAndAddr>;

/**
 * @param InterfaceListPolicy: see description above.
 * @return List of network interfaces.
 */
NX_NETWORK_API QList<QnInterfaceAndAddr> getAllIPv4Interfaces(
    bool ignoreUsb0NetworkInterfaceIfOthersExist = false,
    InterfaceListPolicy interfaceListPolicy = InterfaceListPolicy::oneAddressPerInterface,
    bool ignoreLoopback = true);

/** Filter mask for allLocalAddresses()*/
enum AddressFilter
{
    ipV4 = 1 << 0,
    ipV6 = 1 << 1,
    noLocal = 1 << 2,
    noLoopback = 1 << 3,
    onePerDevice = 1 << 4,
    onlyFirstIpV4 = ipV4 | onePerDevice | noLocal | noLoopback
};
Q_DECLARE_FLAGS(AddressFilters, AddressFilter)
Q_DECLARE_OPERATORS_FOR_FLAGS(AddressFilters)

/**
 * @return List of all IP addresses of current machine. Filters result by filter param.
 */
NX_NETWORK_API QList<HostAddress> allLocalAddresses(AddressFilters filter);

/**
 * Set filter for interface list.
 */
NX_NETWORK_API void setInterfaceListFilter(const QList<QHostAddress>& ifList);

NX_NETWORK_API nx::utils::MacAddress getMacByIP(const QString& host, bool net = true);

NX_NETWORK_API nx::utils::MacAddress getMacByIP(const QHostAddress& ip, bool net = true);

// TODO: #akolesnikov Remove this method if favor of AddressResolver.
NX_NETWORK_API QHostAddress resolveAddress(const QString& addr);

NX_NETWORK_API bool isNewDiscoveryAddressBetter(
    const HostAddress& host,
    const QHostAddress& newAddress,
    const QHostAddress& oldAddress );

static constexpr int MAC_ADDR_LEN = 18;
/**
 * @param host If function succeeds *host contains pointer to statically-allocated buffer,
 * so it MUST NOT be freed!
 * @return 0 on success, -1 in case of error. Use errno to get error code.
 * TODO #akolesnikov Refactor: make memory safe, introduce some high-level type for MAC.
 */
NX_NETWORK_API int getMacFromPrimaryIF(char  MAC_str[MAC_ADDR_LEN], char** host);

NX_NETWORK_API QString getMacFromPrimaryIF();

NX_NETWORK_API QSet<QString> getLocalIpV4AddressList();

NX_NETWORK_API std::set<HostAddress> getLocalIpV4HostAddressList();

} // namespace nx::network
