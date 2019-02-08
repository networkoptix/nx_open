#include "threadsafe_message_bus_adapter.h"

namespace ec2 {

void ThreadsafeMessageBusAdapter::start()
{
    QnMutexLocker guard(&m_mutex);
    base_type::start();
}

void ThreadsafeMessageBusAdapter::stop()
{
    QnMutexLocker guard(&m_mutex);
    base_type::stop();
}

QSet<QnUuid> ThreadsafeMessageBusAdapter::directlyConnectedClientPeers() const
{
    QnMutexLocker guard(&m_mutex);
    return base_type::directlyConnectedClientPeers();
}

QnUuid ThreadsafeMessageBusAdapter::routeToPeerVia(
    const QnUuid& dstPeer,
    int* distance,
    nx::network::SocketAddress* knownPeerAddress) const
{
    QnMutexLocker guard(&m_mutex);
    return base_type::routeToPeerVia(dstPeer, distance, knownPeerAddress);
}

int ThreadsafeMessageBusAdapter::distanceToPeer(const QnUuid& dstPeer) const
{
    QnMutexLocker guard(&m_mutex);
    return base_type::distanceToPeer(dstPeer);
}

void ThreadsafeMessageBusAdapter::addOutgoingConnectionToPeer(
    const QnUuid& id,
    nx::vms::api::PeerType peerType,
    const nx::utils::Url& url)
{
    QnMutexLocker guard(&m_mutex);
    base_type::addOutgoingConnectionToPeer(id, peerType, url);
}

void ThreadsafeMessageBusAdapter::removeOutgoingConnectionFromPeer(const QnUuid& id)
{
    QnMutexLocker guard(&m_mutex);
    base_type::removeOutgoingConnectionFromPeer(id);
}

void ThreadsafeMessageBusAdapter::dropConnections()
{
    QnMutexLocker guard(&m_mutex);
    base_type::dropConnections();
}

QVector<QnTransportConnectionInfo> ThreadsafeMessageBusAdapter::connectionsInfo() const
{
    QnMutexLocker guard(&m_mutex);
    return base_type::connectionsInfo();
}

void ThreadsafeMessageBusAdapter::setHandler(ECConnectionNotificationManager* handler)
{
    QnMutexLocker guard(&m_mutex);
    base_type::setHandler(handler);
}

void ThreadsafeMessageBusAdapter::removeHandler(ECConnectionNotificationManager* handler)
{
    QnMutexLocker guard(&m_mutex);
    base_type::removeHandler(handler);
}

QnJsonTransactionSerializer* ThreadsafeMessageBusAdapter::jsonTranSerializer() const
{
    QnMutexLocker guard(&m_mutex);
    return base_type::jsonTranSerializer();
}

QnUbjsonTransactionSerializer* ThreadsafeMessageBusAdapter::ubjsonTranSerializer() const
{
    QnMutexLocker guard(&m_mutex);
    return base_type::ubjsonTranSerializer();
}

ConnectionGuardSharedState* ThreadsafeMessageBusAdapter::connectionGuardSharedState()
{
    QnMutexLocker guard(&m_mutex);
    return base_type::connectionGuardSharedState();
}

} // namespace ec2
