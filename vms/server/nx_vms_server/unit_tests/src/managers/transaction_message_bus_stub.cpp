#include "transaction_message_bus_stub.h"

namespace ec2 {
namespace test {

TransactionTransportStub::TransactionTransportStub(
    const nx::vms::api::PeerData& localPeer,
    const nx::vms::api::PeerData& remotePeer,
    const nx::utils::Url& remotePeerApiUrl,
    bool isIncoming)
    :
    m_localPeer(localPeer),
    m_remotePeer(remotePeer),
    m_remotePeerApiUrl(remotePeerApiUrl),
    m_isIncoming(isIncoming)
{
}

const nx::vms::api::PeerData& TransactionTransportStub::localPeer() const
{
    return m_localPeer;
}

const nx::vms::api::PeerData& TransactionTransportStub::remotePeer() const
{
    return m_remotePeer;
}

nx::utils::Url TransactionTransportStub::remoteAddr() const
{
    return m_remotePeerApiUrl;
}

bool TransactionTransportStub::isIncoming() const
{
    return m_isIncoming;
}

nx::network::http::AuthInfoCache::AuthorizationCacheItem TransactionTransportStub::authData() const
{
    // TODO
    return nx::network::http::AuthInfoCache::AuthorizationCacheItem();
}

std::multimap<QString, QString>  TransactionTransportStub::httpQueryParams() const
{
    return {};
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
    int* /*distance*/,
    nx::network::SocketAddress* /*knownPeerAddress*/) const
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
    nx::vms::api::PeerType /*peerType*/,
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

void TransactionMessageBusStub::addConnectionToRemotePeer(
    const nx::vms::api::PeerData& localPeer,
    const nx::vms::api::PeerData& remotePeer,
    const nx::utils::Url& remotePeerApiUrl,
    bool isIncoming)
{
    auto newConnection = std::make_unique<TransactionTransportStub>(
        localPeer,
        remotePeer,
        remotePeerApiUrl,
        isIncoming);
    m_connections.push_back(std::move(newConnection));

    emit newDirectConnectionEstablished(m_connections.back().get());
}

} // namespace test
} // namespace ec2
