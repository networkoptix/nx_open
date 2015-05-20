#include "client.h"

#include "utils/common/log.h"
#include "utils/network/nettools.h"
#include "utils/common/timermanager.h"

#include <QDateTime>

static const int AFORT_COUNT = 3;
static const quint32 LIFETIME = 1 * 60 * 60; // 1 hour

static QList<QnInterfaceAndAddr> getInterfaces(const SocketAddress& address)
{
    QList<QnInterfaceAndAddr> allIfs = getAllIPv4Interfaces();
    if (address.address == HostAddress::anyHost)
        return allIfs;

    QHostAddress qAddress(address.address.toString());
    QList<QnInterfaceAndAddr> targetIfs;
    for (const auto& ifc : allIfs)
        if (qAddress == ifc.address)
            targetIfs.push_back(ifc);

    return targetIfs;
}

namespace pcp {

Guard Client::mapPort(const SocketAddress& address)
{
    return Guard([address]()
    {
    });
}

Guard Client::subscribe(const std::function<void(Mapping)>& callback)
{
    return m_subscription.subscribe(callback);
}

} // namespace pcp
