#include "traffic_relay.h"

#include <nx/network/aio/aio_service.h>
#include <nx/network/socket_global.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>

namespace nx {
namespace cloud {
namespace relay {
namespace controller {

TrafficRelay::~TrafficRelay()
{
    decltype(m_relaySessions) relaySessions;
    {
        QnMutexLocker lock(&m_mutex);
        m_terminated = true;
        m_relaySessions.swap(relaySessions);
    }

    for (const auto& relaySession: relaySessions)
        relaySession.channelBridge->pleaseStopSync();
}

void TrafficRelay::startRelaying(
    RelayConnectionData clientConnection,
    RelayConnectionData serverConnection)
{
    NX_LOGX(lm("Starting relaying between %1 and %2")
        .arg(clientConnection.peerId).arg(serverConnection.peerId),
        cl_logDEBUG2);

    RelaySession relaySession;
    relaySession.clientPeerId = std::move(clientConnection.peerId);
    relaySession.serverPeerId = std::move(serverConnection.peerId);
    relaySession.channelBridge = network::aio::makeAsyncChannelBridge(
        std::move(clientConnection.connection),
        std::move(serverConnection.connection));
    relaySession.channelBridge->bindToAioThread(
        network::SocketGlobals::aioService().getRandomAioThread());

    QnMutexLocker lock(&m_mutex);

    m_relaySessions.push_back(std::move(relaySession));
    m_relaySessions.back().channelBridge->start(
        std::bind(&TrafficRelay::onRelaySessionFinished, this, --m_relaySessions.end()));
}

void TrafficRelay::onRelaySessionFinished(
    std::list<RelaySession>::iterator sessionIter)
{
    QnMutexLocker lock(&m_mutex);

    if (m_terminated)
        return;

    NX_LOGX(lm("Done relaying between %1 and %2")
        .arg(sessionIter->clientPeerId).arg(sessionIter->serverPeerId),
        cl_logDEBUG2);

    m_relaySessions.erase(sessionIter);
}

} // namespace controller
} // namespace relay
} // namespace cloud
} // namespace nx
