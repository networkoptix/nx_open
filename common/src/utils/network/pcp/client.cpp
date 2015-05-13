#include "client.h"

#include <QtCore/QMutex>
#include <QtCore/QDateTime>

#include <queue>

#include "messaging.h"

static const int AFORT_COUNT = 3;
static const quint32 LIFETIME = 1 * 60 * 60; // 1 hour

namespace pcp {

Client::Client()
{
    // TODO: listen to incomming messages and notify m_subscription
}

Guard Client::mapPort(SocketAddress address)
{
    // TODO: send mapping request and setup timer to renew it
    static_cast<void>(address);

    return Guard([]()
    {
        // TODO: remove renew timer
    });
}

Guard Client::subscribe(const std::function<void(Mapping)>& callback)
{
    return m_subscription.subscribe(callback);
}

} // namespace pcp
