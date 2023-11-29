// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <QtCore/QTimer>

#include <common/common_module_aware.h>
#include <nx/utils/elapsed_timer.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/data/tran_state_data.h>
#include <transaction/amend_transaction_data.h>
#include <transaction/json_transaction_serializer.h>
#include <transaction/transaction.h>
#include <transaction/transaction_descriptor.h>
#include <transaction/transaction_message_bus_base.h>
#include <transaction/ubjson_transaction_serializer.h>

#include "connection_context.h"
#include "p2p_connection.h"
#include "p2p_fwd.h"
#include "p2p_serialization.h"
#include "routing_helpers.h"

namespace nx {
namespace p2p {

using ResolvePeerNumberMessageType = QVector<PeerNumberType>;

struct AlivePeersRecord
{
    PeerNumberType peerNumber = 0;
    quint16 distance = 0;
};
using AlivePeersMessageType = QVector<AlivePeersRecord>;

struct SubscribeForDataUpdateRecord
{
    PeerNumberType peerNumber = 0;
    int sequence = 0;
};
using SubscribeForDataUpdatesMessageType = QVector<SubscribeForDataUpdateRecord>;

class MessageBus: public ec2::TransactionMessageBusBase
{
    Q_OBJECT
    using base_type = ec2::TransactionMessageBusBase;

public:
    const static QString kCloudPathPrefix;

    MessageBus(
        nx::vms::common::SystemContext* systemContext,
        vms::api::PeerType peerType,
        ec2::QnJsonTransactionSerializer* jsonTranSerializer,
        QnUbjsonTransactionSerializer* ubjsonTranSerializer);
    ~MessageBus() override;

    virtual void addOutgoingConnectionToPeer(
        const QnUuid& id,
        nx::vms::api::PeerType peerType,
        const nx::utils::Url& url,
        std::optional<nx::network::http::Credentials> credentials,
        nx::network::ssl::AdapterFunc adapterFunc) override;
    virtual void removeOutgoingConnectionFromPeer(const QnUuid& id) override;
    virtual void updateOutgoingConnection(
        const QnUuid& id, nx::network::http::Credentials credentials) override;

    QMap<QnUuid, P2pConnectionPtr> connections() const;
    int connectionTries() const;

    // Self peer information
    vms::api::PeerData localPeer() const;
    virtual vms::api::PeerDataEx localPeerEx() const;

    virtual void start() override;
    virtual void stop() override;

    virtual QSet<QnUuid> directlyConnectedClientPeers() const override;
    virtual QSet<QnUuid> directlyConnectedServerPeers() const override;
    virtual QnUuid routeToPeerVia(
        const QnUuid& dstPeer, int* distance, nx::network::SocketAddress* knownPeerAddress) const override;
    virtual int distanceToPeer(const QnUuid& dstPeer) const override;
    virtual void dropConnections() override;
    virtual ConnectionInfos connectionInfos() const override;

    template<class T> void sendTransaction(const ec2::QnTransaction<T>& tran);

    template<class T>
    void sendTransaction(const ec2::QnTransaction<T>& tran, const TransportHeader& header);

    template<class T>
    void sendTransaction(const ec2::QnTransaction<T>& tran, const vms::api::PeerSet& dstPeers);

    bool isSubscribedTo(const vms::api::PersistentIdData& peer) const;
    qint32 distanceTo(const vms::api::PersistentIdData& peer) const;

    /* Check remote peer identityTime and set local value if it greater.
     * @return false if system identity time has been changed.
     */
    virtual bool validateRemotePeerData(const vms::api::PeerDataEx& remotePeer);

protected:
    template<class T>
    void sendTransactionImpl(
        const P2pConnectionPtr& connection,
        const ec2::QnTransaction<T>& srcTran,
        TransportHeader transportHeader);

    template<class T>
    void sendUnicastTransaction(const QnTransaction<T>& tran, const vms::api::PeerSet& dstPeers);

    template<class T>
    void sendUnicastTransactionImpl(
        const QnTransaction<T>& tran,
        const QMap<P2pConnectionPtr, TransportHeader>& dstByConnection);

    void printTran(
        const P2pConnectionPtr& connection,
        const ec2::QnAbstractTransaction& tran,
    Connection::Direction direction) const;

    void deleteRemoveUrlById(const QnUuid& id);

    virtual void doPeriodicTasks();
    virtual void addOfflinePeersFromDb() {}
    virtual void sendInitialDataToCloud(const P2pConnectionPtr& connection);

    virtual bool selectAndSendTransactions(
        const P2pConnectionPtr& connection,
        vms::api::TranState newSubscription,
        bool addImplicitData);
    virtual bool handlePushTransactionData(
        const P2pConnectionPtr& connection,
        const QByteArray& data,
        const TransportHeader& header,
        nx::Locker<nx::Mutex>* lock);
    virtual bool handlePushImpersistentBroadcastTransaction(
        const P2pConnectionPtr& connection,
        const QByteArray& payload,
        nx::Locker<nx::Mutex>* lock);
    virtual void onThreadStopped() {}

protected:
    static QString peerName(const QnUuid& id);
    QMap<vms::api::PersistentIdData, P2pConnectionPtr> getCurrentSubscription() const;

    /**  Local connections are not supposed to be shown in 'aliveMessage' */
    bool isLocalConnection(const vms::api::PersistentIdData& peer) const;
    void createOutgoingConnections(const QMap<vms::api::PersistentIdData, P2pConnectionPtr>& currentSubscription);
    bool hasStartingConnections() const;
    P2pConnectionPtr findConnectionById(const vms::api::PersistentIdData& id) const;
    void emitPeerFoundLostSignals();
    void connectSignals(const P2pConnectionPtr& connection);
    void startReading(P2pConnectionPtr connection);
    void sendRuntimeData(const P2pConnectionPtr& connection, const QList<vms::api::PersistentIdData>& peers);

    template <class T>
    void gotTransaction(
        const QnTransaction<T>& tran,
        const P2pConnectionPtr& connection,
        const TransportHeader& transportHeader,
        nx::Locker<nx::Mutex>* lock);
private:
    void sendAlivePeersMessage(const P2pConnectionPtr& connection = P2pConnectionPtr());

    void doSubscribe(const QMap<vms::api::PersistentIdData, P2pConnectionPtr>& currentSubscription);

    bool handleResolvePeerNumberRequest(const P2pConnectionPtr& connection, const QByteArray& data);
    bool handleResolvePeerNumberResponse(const P2pConnectionPtr& connection, const QByteArray& data);
    bool handlePeersMessage(const P2pConnectionPtr& connection, const QByteArray& data);
    bool handleSubscribeForDataUpdates(const P2pConnectionPtr& connection, const QByteArray& data);
    bool handleSubscribeForAllDataUpdates(const P2pConnectionPtr& connection, const QByteArray& data);
    bool handlePushTransactionList(const P2pConnectionPtr& connection, const QByteArray& tranList, nx::Locker<nx::Mutex>* lock);

    friend struct GotTransactionFuction;
    friend struct GotUnicastTransactionFuction;
    friend struct SendTransactionToTransportFuction;

    template <class T>
    void gotUnicastTransaction(
        const QnTransaction<T>& tran,
        const P2pConnectionPtr& connection,
        const TransportHeader& records,
        nx::Locker<nx::Mutex>* lock);

    /*
     * In P2P mode a Client gets transactions only, without any protocol related system messages.
     * It causes client do not receive peerFound/peerLost signals from messageBus any more.
     * This function sends removeRuntimeInfoData transactions to the all connected clients
     * about peer with specified id.
     */
    void sendRuntimeInfoRemovedToClients(const QnUuid& id);
private slots:
    void at_gotMessage(QWeakPointer<ConnectionBase> connection, MessageType messageType, const nx::Buffer& payload);
    void at_stateChanged(QWeakPointer<ConnectionBase> connection, Connection::State state);
    void at_allDataSent(QWeakPointer<ConnectionBase> connection);
    void cleanupRuntimeInfo(const vms::api::PersistentIdData& peer);
    void removeConnection(QWeakPointer<ConnectionBase> weakRef);
signals:
    void removeConnectionAsync(QWeakPointer<ConnectionBase> weakRef);
private:
    void removeConnectionUnsafe(QWeakPointer<ConnectionBase> weakRef);
public:
    bool needStartConnection(
        const vms::api::PersistentIdData& peer,
        const QMap<vms::api::PersistentIdData, P2pConnectionPtr>& currentSubscription) const;
    bool needStartConnection(
        const QnUuid& peerId,
        const QMap<vms::api::PersistentIdData, P2pConnectionPtr>& currentSubscription) const;
    static ConnectionContext* context(const P2pConnectionPtr& connection);

    struct DelayIntervals
    {
        // How often send 'peers' message to the peer if something is changed
        // As soon as connection is opened first message is sent immediately
        std::chrono::milliseconds sendPeersInfoInterval = std::chrono::seconds(15);

        // If new connection is recently established/closed, don't sent subscribe request to the peer
        std::chrono::milliseconds subscribeIntervalLow = std::chrono::seconds(3);

        // If new connections always established/closed too long time, send subscribe request anyway
        std::chrono::milliseconds subscribeIntervalHigh = std::chrono::seconds(15);

        std::chrono::milliseconds outConnectionsInterval = std::chrono::seconds(1);

        // Delay to reconnect if 2 attempts failed in a row
        std::chrono::milliseconds remotePeerReconnectTimeout = std::chrono::seconds(10);

        std::chrono::milliseconds minInterval() const;
    };

    void setDelayIntervals(const DelayIntervals& intervals);
    DelayIntervals delayIntervals() const;

    QMap<vms::api::PersistentIdData, vms::api::RuntimeData> runtimeInfo() const;

    void updateOfflineDistance(
        const P2pConnectionPtr& connection,
        const vms::api::PersistentIdData& to,
        int sequence);

    bool isStarted() const { return m_started; }

protected:
    std::unique_ptr<BidirectionRoutingInfo> m_peers;
    PeerNumberInfo m_localShortPeerInfo; //< Short numbers created by current peer
    DelayIntervals m_intervals;
    struct MiscData
    {
        MiscData(const MessageBus* owner) : owner(owner) {}
        void update();

        int expectedConnections = 0;
        int maxSubscriptionToResubscribe = 0;
        int maxDistanceToUseProxy = 0;
        int newConnectionsAtOnce = 1;
    private:
        const MessageBus* owner;
    } m_miscData;
    QMap<QnUuid, P2pConnectionPtr> m_connections; //< Actual connection list
    QElapsedTimer m_lastPeerInfoTimer;
    QMap<vms::api::PersistentIdData, vms::api::RuntimeData> m_lastRuntimeInfo;
protected:
    void dropConnectionsThreadUnsafe();
private:
    QMap<QnUuid, P2pConnectionPtr> m_outgoingConnections; //< Temporary list of outgoing connections

    struct RemoteConnection
    {
        RemoteConnection() {}
        RemoteConnection(
            const QnUuid& peerId,
            nx::vms::api::PeerType peerType,
            const nx::utils::Url& url,
            std::optional<nx::network::http::Credentials> credentials,
            nx::network::ssl::AdapterFunc adapterFunc)
            :
            peerId(peerId),
            peerType(peerType),
            url(url),
            credentials(std::move(credentials)),
            adapterFunc(std::move(adapterFunc))
        {
        }

        QnUuid peerId;
        nx::vms::api::PeerType peerType = nx::vms::api::PeerType::notDefined;
        nx::utils::Url url;
        std::optional<nx::network::http::Credentials> credentials;
        nx::network::ssl::AdapterFunc adapterFunc;
        QVector<nx::utils::ElapsedTimer> disconnectTimes;
        ConnectionBase::State lastConnectionState = ConnectionBase::State::NotDefined;
    };

    std::vector<RemoteConnection> m_remoteUrls;

    friend struct MiscData;

    QTimer* m_timer = nullptr;

    int m_lastOutgoingIndex = 0;
    int m_connectionTries = 0;
    QElapsedTimer m_outConnectionsTimer;
    std::set<vms::api::PeerData> m_lastAlivePeers;
    std::atomic<bool> m_started{false};
    QMap<QnUuid, Connection::State> m_lastConnectionState;
};

} // namespace p2p
} // namespace nx
