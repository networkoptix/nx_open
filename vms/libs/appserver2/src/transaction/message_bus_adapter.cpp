#include "message_bus_adapter.h"
#include <nx/p2p/p2p_message_bus.h>
#include "transaction_message_bus.h"

using namespace nx::vms;

namespace ec2 {

TransactionMessageBusAdapter::TransactionMessageBusAdapter(
    QnCommonModule* commonModule,
    QnJsonTransactionSerializer* jsonTranSerializer,
    QnUbjsonTransactionSerializer* ubjsonTranSerializer)
    :
    AbstractTransactionMessageBus(commonModule),
    m_jsonTranSerializer(jsonTranSerializer),
    m_ubjsonTranSerializer(ubjsonTranSerializer)
{
}

void TransactionMessageBusAdapter::reset()
{
    m_bus.reset();
}

void TransactionMessageBusAdapter::initInternal()
{
    connect(m_bus.get(), &AbstractTransactionMessageBus::peerFound,
        this, &AbstractTransactionMessageBus::peerFound, Qt::DirectConnection);
    connect(m_bus.get(), &AbstractTransactionMessageBus::peerLost,
        this, &AbstractTransactionMessageBus::peerLost, Qt::DirectConnection);
    connect(m_bus.get(), &AbstractTransactionMessageBus::remotePeerUnauthorized,
        this, &AbstractTransactionMessageBus::remotePeerUnauthorized, Qt::DirectConnection);
    connect(m_bus.get(), &AbstractTransactionMessageBus::newDirectConnectionEstablished,
        this, &AbstractTransactionMessageBus::newDirectConnectionEstablished, Qt::DirectConnection);
}

void TransactionMessageBusAdapter::start()
{
    m_bus->start();
}

void TransactionMessageBusAdapter::stop()
{
    m_bus->stop();
}

QSet<QnUuid> TransactionMessageBusAdapter::directlyConnectedClientPeers() const
{
    return m_bus->directlyConnectedClientPeers();
}

QSet<QnUuid> TransactionMessageBusAdapter::directlyConnectedServerPeers() const
{
    return m_bus->directlyConnectedServerPeers();
}

QnUuid TransactionMessageBusAdapter::routeToPeerVia(
    const QnUuid& dstPeer, int* distance, nx::network::SocketAddress* knownPeerAddress) const
{
    // This method can be called asynchronously after connection has been closed from the
    // QnRtspClientArchiveDelegate::checkMinTimeFromOtherServer (QtConcurrent call).
    return m_bus ? m_bus->routeToPeerVia(dstPeer, distance, knownPeerAddress) : QnUuid();
}

int TransactionMessageBusAdapter::distanceToPeer(const QnUuid& dstPeer) const
{
    return m_bus->distanceToPeer(dstPeer);
}

void TransactionMessageBusAdapter::addOutgoingConnectionToPeer(
    const QnUuid& id,
    nx::vms::api::PeerType peerType,
    const nx::utils::Url &url)
{
    m_bus->addOutgoingConnectionToPeer(id, peerType, url);
}

void TransactionMessageBusAdapter::removeOutgoingConnectionFromPeer(const QnUuid& id)
{
    m_bus->removeOutgoingConnectionFromPeer(id);
}

void TransactionMessageBusAdapter::dropConnections()
{
    m_bus->dropConnections();
}

QVector<QnTransportConnectionInfo> TransactionMessageBusAdapter::connectionsInfo() const
{
    return m_bus->connectionsInfo();
}

void TransactionMessageBusAdapter::setHandler(ECConnectionNotificationManager* handler)
{
    m_bus->setHandler(handler);
}

void TransactionMessageBusAdapter::removeHandler(ECConnectionNotificationManager* handler)
{
    m_bus->removeHandler(handler);
}

QnJsonTransactionSerializer* TransactionMessageBusAdapter::jsonTranSerializer() const
{
    return m_bus->jsonTranSerializer();
}

QnUbjsonTransactionSerializer* TransactionMessageBusAdapter::ubjsonTranSerializer() const
{
    return m_bus->ubjsonTranSerializer();
}

ConnectionGuardSharedState* TransactionMessageBusAdapter::connectionGuardSharedState()
{
    return m_bus->connectionGuardSharedState();
}

#if 0
detail::QnDbManager* TransactionMessageBusAdapter::getDb() const
{
    return m_bus->getDb();
}
#endif

} // namespace ec2
