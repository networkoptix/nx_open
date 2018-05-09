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
        const ::ec2::ApiPeerData& localPeer,
        const ::ec2::ApiPeerData& remotePeer,
        const nx::utils::Url& remotePeerApiUrl,
        bool isIncoming);

    virtual const ec2::ApiPeerData& localPeer() const override;
    virtual const ec2::ApiPeerData& remotePeer() const override;
    virtual nx::utils::Url remoteAddr() const override;
    virtual bool isIncoming() const override;
    virtual nx::network::http::AuthInfoCache::AuthorizationCacheItem authData() const override;

private:
    const ec2::ApiPeerData m_localPeer;
    const ec2::ApiPeerData m_remotePeer;
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

    virtual QnUuid routeToPeerVia(const QnUuid& dstPeer, int* distance) const override;
    virtual int distanceToPeer(const QnUuid& dstPeer) const override;

    virtual void addOutgoingConnectionToPeer(const QnUuid& id, const nx::utils::Url& url) override;
    virtual void removeOutgoingConnectionFromPeer(const QnUuid& id) override;

    virtual void dropConnections() override;

    virtual QVector<QnTransportConnectionInfo> connectionsInfo() const override;

    virtual void setHandler(ECConnectionNotificationManager* handler) override;
    virtual void removeHandler(ECConnectionNotificationManager* handler) override;

    virtual QnJsonTransactionSerializer* jsonTranSerializer() const override;
    virtual QnUbjsonTransactionSerializer* ubjsonTranSerializer() const override;

    virtual ConnectionGuardSharedState* connectionGuardSharedState() override;

    void addConnectionToRemotePeer(
        const ::ec2::ApiPeerData& localPeer,
        const ::ec2::ApiPeerData& remotePeer,
        const nx::utils::Url& remotePeerApiUrl,
        bool isIncoming);

private:
    std::vector<std::unique_ptr<TransactionTransportStub>> m_connections;
};

} // namespace test
} // namespace ec2
