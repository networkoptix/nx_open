#pragma once

#include <memory>
#include <vector>

#include <common/common_module.h>

#include <transaction/abstract_transaction_message_bus.h>
#include <transaction/abstract_transaction_transport.h>

namespace ec2 {
namespace test {

class TransactionTransportStub:
    public QnAbstractTransactionTransport
{
public:
    TransactionTransportStub(
        const nx::vms::api::PeerData& localPeer,
        const nx::vms::api::PeerData& remotePeer,
        const nx::utils::Url& remotePeerApiUrl,
        bool isIncoming);

    virtual const nx::vms::api::PeerData& localPeer() const override;
    virtual const nx::vms::api::PeerData& remotePeer() const override;
    virtual nx::utils::Url remoteAddr() const override;
    virtual bool isIncoming() const override;
    virtual nx::network::http::AuthInfoCache::AuthorizationCacheItem authData() const override;
    virtual std::multimap<QString, QString> httpQueryParams() const override;

private:
    const nx::vms::api::PeerData m_localPeer;
    const nx::vms::api::PeerData m_remotePeer;
    const nx::utils::Url m_remotePeerApiUrl;
    const bool m_isIncoming;
};

//-------------------------------------------------------------------------------------------------

class TransactionMessageBusStub:
    public AbstractTransactionMessageBus
{
public:
    TransactionMessageBusStub(QnCommonModule* commonModule);

    virtual void start() override;
    virtual void stop() override;

    virtual QSet<QnUuid> directlyConnectedClientPeers() const override;
    virtual QSet<QnUuid> directlyConnectedServerPeers() const override;

    virtual QnUuid routeToPeerVia(
        const QnUuid& dstPeer,
        int* distance,
        nx::network::SocketAddress* knownPeerAddress) const override;
    virtual int distanceToPeer(const QnUuid& dstPeer) const override;

    virtual void addOutgoingConnectionToPeer(
        const QnUuid& id,
        nx::vms::api::PeerType peerType,
        const nx::utils::Url& url) override;
    virtual void removeOutgoingConnectionFromPeer(const QnUuid& id) override;

    virtual void dropConnections() override;

    virtual QVector<QnTransportConnectionInfo> connectionsInfo() const override;

    virtual void setHandler(ECConnectionNotificationManager* handler) override;
    virtual void removeHandler(ECConnectionNotificationManager* handler) override;

    virtual QnJsonTransactionSerializer* jsonTranSerializer() const override;
    virtual QnUbjsonTransactionSerializer* ubjsonTranSerializer() const override;

    virtual ConnectionGuardSharedState* connectionGuardSharedState() override;

    void addConnectionToRemotePeer(
        const nx::vms::api::PeerData& localPeer,
        const nx::vms::api::PeerData& remotePeer,
        const nx::utils::Url& remotePeerApiUrl,
        bool isIncoming);

private:
    std::vector<std::unique_ptr<TransactionTransportStub>> m_connections;
};

} // namespace test
} // namespace ec2
