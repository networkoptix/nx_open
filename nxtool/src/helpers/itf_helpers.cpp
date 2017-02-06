
#include "itf_helpers.h"

#include <QHostAddress>
#include <QNetworkInterface>

namespace
{
    bool isValidIpV4Address(const QString &address)
    {
        const QHostAddress &ipHostAddress = QHostAddress(address);
        return (!ipHostAddress.isNull() 
            && (ipHostAddress.protocol() == QAbstractSocket::IPv4Protocol));
    }
}

bool rtu::helpers::isValidSubnetMask(const QString &mask)
{
    if (!isValidIpV4Address(mask))
        return false;

    quint32 value = QHostAddress(mask).toIPv4Address();
    value = ~value;
    return ((value & (value + 1)) == 0);
}

bool rtu::helpers::isDiscoverableFromCurrentNetwork(const QString &ip
    , const QString &mask)
{
    if (!isValidIpV4Address(ip))
        return false;

    const auto &allInterfaces = QNetworkInterface::allInterfaces();
    for(const auto &itf: allInterfaces)
    {
        const QNetworkInterface::InterfaceFlags flags = itf.flags();
        if (!itf.isValid() || !(flags & QNetworkInterface::IsUp)
            || !(flags & QNetworkInterface::IsRunning) 
            || (flags & QNetworkInterface::IsLoopBack))
        {
            continue;
        }

        for (const QNetworkAddressEntry &address: itf.addressEntries())
        {
            const QString &itfIp = address.ip().toString();
            const QString &itfMask = address.netmask().toString();
            if (!isValidIpV4Address(itfIp) || !isValidSubnetMask(itfMask))
                continue;

            const auto checkingMask = (mask.isEmpty() ? itfMask : mask);
            if (isDiscoverableFromNetwork(ip, checkingMask, itfIp, itfMask))
                return true;
        }
    }

    return false;
}

bool rtu::helpers::isDiscoverableFromNetwork(const QString &ip
    , const QString &mask
    , const QString &subnet
    , const QString &subnetMask)
{
    if (!isValidIpV4Address(ip) || !isValidSubnetMask(mask)
        || !isValidIpV4Address(subnet) || !isValidSubnetMask(subnetMask))
    {
        return false;
    }

    const quint32 longMask = std::max(QHostAddress(mask).toIPv4Address()
        , QHostAddress(subnetMask).toIPv4Address());

    const quint32 ipNetwork = (QHostAddress(ip).toIPv4Address() & longMask );
    const quint32 secondNetwork = (QHostAddress(subnet).toIPv4Address() & longMask);

    return (ipNetwork == secondNetwork);
}
