#include "transaction_message_bus_stub.h"

namespace ec2 {
namespace test {

const ec2::ApiPeerData& TransactionTransportStub::localPeer() const
{
    return m_localPeer;
}

const ec2::ApiPeerData& TransactionTransportStub::remotePeer() const
{
    return m_remotePeer;
}

nx::utils::Url TransactionTransportStub::remoteAddr() const
{
    // TODO
    return nx::utils::Url();
}

bool TransactionTransportStub::isIncoming() const
{
    // TODO
    return false;
}

nx::network::http::AuthInfoCache::AuthorizationCacheItem TransactionTransportStub::authData() const
{
    // TODO
    return nx::network::http::AuthInfoCache::AuthorizationCacheItem();
}

//-------------------------------------------------------------------------------------------------

TransactionMessageBusStub::TransactionMessageBusStub(
    QnCommonModule* commonModule)
    :
    AbstractTransactionMessageBus(commonModule)
{
}

void TransactionMessageBusStub::start()
{
    // TODO
}

void TransactionMessageBusStub::stop()
{
    // TODO
}

QSet<QnUuid> TransactionMessageBusStub::directlyConnectedClientPeers() const
{
    // TODO
    return QSet<QnUuid>();
}

QSet<QnUuid> TransactionMessageBusStub::directlyConnectedServerPeers() const
{
    // TODO
    return QSet<QnUuid>();
}

QnUuid TransactionMessageBusStub::routeToPeerVia(
    const QnUuid& /*dstPeer*/,
    int* /*distance*/) const
{
    // TODO
    return QnUuid();
}

int TransactionMessageBusStub::distanceToPeer(const QnUuid& /*dstPeer*/) const
{
    // TODO
    return 0;
}

void TransactionMessageBusStub::addOutgoingConnectionToPeer(
    const QnUuid& /*id*/,
    const nx::utils::Url& /*url*/)
{
    // TODO
}

void TransactionMessageBusStub::removeOutgoingConnectionFromPeer(
    const QnUuid& /*id*/)
{
    // TODO
}

void TransactionMessageBusStub::dropConnections()
{
    // TODO
}

QVector<QnTransportConnectionInfo> TransactionMessageBusStub::connectionsInfo() const
{
    // TODO
    return QVector<QnTransportConnectionInfo>();
}

void TransactionMessageBusStub::setHandler(
    ECConnectionNotificationManager* /*handler*/)
{
    // TODO
}

void TransactionMessageBusStub::removeHandler(
    ECConnectionNotificationManager* /*handler*/)
{
    // TODO
}

QnJsonTransactionSerializer* TransactionMessageBusStub::jsonTranSerializer() const
{
    // TODO
    return nullptr;
}

QnUbjsonTransactionSerializer* TransactionMessageBusStub::ubjsonTranSerializer() const
{
    // TODO
    return nullptr;
}

ConnectionGuardSharedState* TransactionMessageBusStub::connectionGuardSharedState()
{
    // TODO
    return nullptr;
}

detail::QnDbManager* TransactionMessageBusStub::getDb() const
{
    // TODO
    return nullptr;
}

void TransactionMessageBusStub::setTimeSyncManager(
    TimeSynchronizationManager* /*timeSyncManager*/)
{
    // TODO
}

} // namespace test
} // namespace ec2
