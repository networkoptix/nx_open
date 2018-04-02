#include "message_bus_adapter.h"
#include <nx/p2p/p2p_message_bus.h>
#include "transaction_message_bus.h"
#include <database/db_manager.h>

namespace ec2 {

TransactionMessageBusAdapter::TransactionMessageBusAdapter(
    Qn::PeerType peerType,
    QnCommonModule* commonModule,
    QnJsonTransactionSerializer* jsonTranSerializer,
    QnUbjsonTransactionSerializer* ubjsonTranSerializer)
    :
    AbstractTransactionMessageBus(commonModule),
    m_peerType(peerType),
    m_jsonTranSerializer(jsonTranSerializer),
    m_ubjsonTranSerializer(ubjsonTranSerializer),
    m_timeSyncManager(nullptr)
{
}

void TransactionMessageBusAdapter::reset()
{
    m_bus.reset();
}

template <typename MessageBusType>
MessageBusType* TransactionMessageBusAdapter::init()
{
    reset();

    if (value == MessageBusType::P2pMode)
    {
        m_bus.reset(new MessageBusType(
            m_peerType,
            commonModule(),
            m_jsonTranSerializer,
            m_ubjsonTranSerializer));
    }
    else
    {
        m_bus.reset(new MessageBusType(
            m_peerType,
            commonModule(),
            m_jsonTranSerializer,
            m_ubjsonTranSerializer));
    }

    m_bus->setTimeSyncManager(m_timeSyncManager);
    connect(m_bus.get(), &AbstractTransactionMessageBus::peerFound,
        this, &AbstractTransactionMessageBus::peerFound, Qt::DirectConnection);
    connect(m_bus.get(), &AbstractTransactionMessageBus::peerLost,
        this, &AbstractTransactionMessageBus::peerLost, Qt::DirectConnection);
    connect(m_bus.get(), &AbstractTransactionMessageBus::remotePeerUnauthorized,
        this, &AbstractTransactionMessageBus::remotePeerUnauthorized, Qt::DirectConnection);
    connect(m_bus.get(), &AbstractTransactionMessageBus::newDirectConnectionEstablished,
        this, &AbstractTransactionMessageBus::newDirectConnectionEstablished, Qt::DirectConnection);

    return dynamic_cast<MessageBusType*> (m_bus.get())
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

QnUuid TransactionMessageBusAdapter::routeToPeerVia(const QnUuid& dstPeer, int* distance) const
{
    return m_bus->routeToPeerVia(dstPeer, distance);
}

int TransactionMessageBusAdapter::distanceToPeer(const QnUuid& dstPeer) const
{
    return m_bus->distanceToPeer(dstPeer);
}

void TransactionMessageBusAdapter::addOutgoingConnectionToPeer(const QnUuid& id, const nx::utils::Url &url)
{
    m_bus->addOutgoingConnectionToPeer(id, url);
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

void TransactionMessageBusAdapter::setTimeSyncManager(TimeSynchronizationManager* timeSyncManager)
{
    m_timeSyncManager = timeSyncManager;
    if (m_bus)
        m_bus->setTimeSyncManager(m_timeSyncManager);
}

} // namespace ec2
