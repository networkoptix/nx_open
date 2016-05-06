#include "client.h"

#include <nx/utils/log/log.h>
#include <nx/network/nettools.h>
#include <nx/utils/timer_manager.h>

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

//TODO this method MUST follow m_subscription.subscribe API to avoid thread race error
Guard Client::subscribe(const std::function<void(Mapping)>& callback)
{
    nx::utils::SubscriptionId subscriptionId = nx::utils::kInvalidSubscriptionId;
    m_subscription.subscribe(callback, &subscriptionId);
    return Guard(
        [this, subscriptionId](){
            m_subscription.removeSubscription(subscriptionId);
        });
}

} // namespace pcp
