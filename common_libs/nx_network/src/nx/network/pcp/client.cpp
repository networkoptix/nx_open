#include "client.h"

#include <nx/utils/log/log.h>
#include <nx/network/nettools.h>
#include <nx/utils/timer_manager.h>

#include <QDateTime>

static const int AFORT_COUNT = 3;
static const quint32 LIFETIME = 1 * 60 * 60; // 1 hour

namespace nx {
namespace network {
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
} // namespace network
} // namespace nx
