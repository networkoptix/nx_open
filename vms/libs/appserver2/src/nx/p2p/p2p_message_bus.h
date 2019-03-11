#pragma once

#include <memory>

#include <QtCore/QTimer>

#include <nx/utils/uuid.h>
#include <common/common_module_aware.h>
#include <nx/utils/thread/mutex.h>
#include <transaction/transaction_message_bus_base.h>
#include <nx/vms/api/data/tran_state_data.h>
#include "p2p_connection.h"
#include "p2p_fwd.h"
#include <transaction/transaction.h>
#include <common/static_common_module.h>
#include <transaction/ubjson_transaction_serializer.h>
#include <transaction/json_transaction_serializer.h>
#include <transaction/transaction_descriptor.h>
#include "routing_helpers.h"
#include "connection_context.h"
#include "p2p_serialization.h"
#include <transaction/amend_transaction_data.h>
#include <nx/utils/elapsed_timer.h>

namespace nx {
namespace p2p {

using ResolvePeerNumberMessageType = QVector<PeerNumberType>;

struct AlivePeersRecord
{
    PeerNumberType peerNumber = 0;
    quint16 distance = 0;
};
using AlivePeersMessagType = QVector<AlivePeersRecord>;

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
        vms::api::PeerType peerType,
        QnCommonModule* commonModule,
        ec2::QnJsonTransactionSerializer* jsonTranSerializer,
        QnUbjsonTransactionSerializer* ubjsonTranSerializer);
    ~MessageBus() override;

    virtual void addOutgoingConnectionToPeer(
        const QnUuid& id,
        nx::vms::api::PeerType peerType,
        const nx::utils::Url& url) override;
    virtual void removeOutgoingConnectionFromPeer(const QnUuid& id) override;

    QMap<QnUuid, P2pConnectionPtr> connections() const;
    int connectionTries() const;

    // Self peer information
    vms::api::PeerData localPeer() const;
    vms::api::PeerDataEx localPeerEx() const;

    virtual void start() override;
    virtual void stop() override;

    virtual QSet<QnUuid> directlyConnectedClientPeers() const override;
    virtual QSet<QnUuid> directlyConnectedServerPeers() const override;
    virtual QnUuid routeToPeerVia(
        const QnUuid& dstPeer, int* distance, nx::network::SocketAddress* knownPeerAddress) const override;
    virtual int distanceToPeer(const QnUuid& dstPeer) const override;
    virtual void dropConnections() override;
    virtual QVector<QnTransportConnectionInfo> connectionsInfo() const override;

    void sendTransaction(const ec2::QnTransaction<vms::api::RuntimeData>& tran)
    {
        QnMutexLocker lock(&m_mutex);
        vms::api::PersistentIdData peerId(tran.peerID, tran.params.peer.persistentId);
        m_lastRuntimeInfo[peerId] = tran.params;
        for (const auto& connection : m_connections)
            sendTransactionImpl(connection, tran, TransportHeader());
    }

    template<class T>
    void sendTransaction(const ec2::QnTransaction<T>& tran)
    {
        QnMutexLocker lock(&m_mutex);
        for (const auto& connection: m_connections)
            sendTransactionImpl(connection, tran, TransportHeader());
    }

    template<class T>
    void sendTransaction(const ec2::QnTransaction<T>& tran, const TransportHeader& header)
    {
        QnMutexLocker lock(&m_mutex);
        for (const auto& connection : m_connections)
            sendTransactionImpl(connection, tran, header);
    }

    template<class T>
    void sendTransaction(const ec2::QnTransaction<T>& tran, const vms::api::PeerSet& dstPeers)
    {
        NX_ASSERT(tran.command != ApiCommand::NotDefined);
        QnMutexLocker lock(&m_mutex);
        sendUnicastTransaction(tran, dstPeers);
    }

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
        TransportHeader transportHeader)
    {
        NX_ASSERT(srcTran.command != ApiCommand::NotDefined);

        if (!connection->shouldTransactionBeSentToPeer(srcTran))
            return; //< This peer doesn't handle transactions of such type.

        if (transportHeader.via.find(connection->remotePeer().id) != transportHeader.via.end())
            return; //< Already processed by remote peer

        const vms::api::PersistentIdData remotePeer(connection->remotePeer());
        const auto& descriptor = ec2::getTransactionDescriptorByTransaction(srcTran);
        auto remoteAccess = descriptor->checkRemotePeerAccessFunc(
            commonModule(), connection.staticCast<Connection>()->userAccessData(), srcTran.params);
        if (remoteAccess == RemotePeerAccess::Forbidden)
        {
            NX_VERBOSE(
                this,
                lm("Permission check failed while sending transaction %1 to peer %2")
                .arg(srcTran.toString())
                .arg(remotePeer.id.toString()));
            return;
        }

        const vms::api::PersistentIdData peerId(srcTran.peerID, srcTran.persistentInfo.dbID);
        const auto context = this->context(connection);

        ec2::QnTransaction<T> modifiedTran;
        if (connection->remotePeer().isClient())
        {
            modifiedTran = srcTran;
            if (ec2::amendOutputDataIfNeeded(connection.staticCast<Connection>()->userAccessData(),
                    &modifiedTran.params))
            {
                // Make persistent info null in case if data has been amended. We don't want such
                // transactions be checked against serialized transactions cache.
                modifiedTran.persistentInfo = ec2::QnAbstractTransaction::PersistentInfo();
            }
        }
        const ec2::QnTransaction<T>& tran(connection->remotePeer().isClient() ? modifiedTran : srcTran);

        if (connection->remotePeer().isServer())
        {
            if (descriptor->isPersistent)
            {
                if (context->sendDataInProgress || !context->updateSequence(tran))
                    return;
            }
            else
            {
                if (!context->isRemotePeerSubscribedTo(tran.peerID))
                    return;
            }
        }
        else if (remotePeer == peerId)
        {
            return;
        }
        else if (connection->remotePeer().isCloudServer())
        {
            if (!descriptor->isPersistent || context->sendDataInProgress || !context->updateSequence(tran))
                return;
        }

        NX_ASSERT(!(remotePeer == peerId)); //< loop

        if (nx::utils::log::isToBeLogged(nx::utils::log::Level::debug, this))
            printTran(connection, tran, Connection::Direction::outgoing);

        switch (connection->remotePeer().dataFormat)
        {
        case Qn::JsonFormat:
            connection->sendMessage(
                m_jsonTranSerializer->serializedTransactionWithoutHeader(tran) + QByteArray("\r\n"));
            break;
        case Qn::UbjsonFormat:
            if (connection->remotePeer().isClient())
            {
                connection->sendMessage(
                    m_ubjsonTranSerializer->serializedTransactionWithoutHeader(tran));
            }
            else if (descriptor->isPersistent)
            {
                connection->sendMessage(MessageType::pushTransactionData,
                    m_ubjsonTranSerializer->serializedTransactionWithoutHeader(tran));
            }
            else
            {
                TransportHeader header(transportHeader);
                header.via.insert(localPeer().id);
                connection->sendMessage(MessageType::pushImpersistentBroadcastTransaction,
                    m_ubjsonTranSerializer->serializedTransactionWithHeader(tran, header));
            }
            break;
        default:
            qWarning() << "Client has requested data in an unsupported format"
                << connection->remotePeer().dataFormat;
            break;
        }
    }

    template<class T>
    void sendUnicastTransaction(const QnTransaction<T>& tran, const vms::api::PeerSet& dstPeers)
    {
        QMap<P2pConnectionPtr, TransportHeader> dstByConnection;

        // split dstPeers by connection
        for (const auto& peer : dstPeers)
        {
            qint32 distance = kMaxDistance;
            auto dstPeer = routeToPeerVia(peer, &distance, /*address*/ nullptr);
            if (auto& connection = m_connections.value(dstPeer))
                dstByConnection[connection].dstPeers.push_back(peer);
        }
        sendUnicastTransactionImpl(tran, dstByConnection);
    }

    template<class T>
    void sendUnicastTransactionImpl(
        const QnTransaction<T>& tran,
        const QMap<P2pConnectionPtr, TransportHeader>& dstByConnection)
    {
        for (auto itr = dstByConnection.begin(); itr != dstByConnection.end(); ++itr)
        {
            const auto& connection = itr.key();
            TransportHeader transportHeader = itr.value();

            if (transportHeader.via.find(connection->remotePeer().id) != transportHeader.via.end())
                continue; //< Already processed by remote peer

            if (!connection->shouldTransactionBeSentToPeer(tran))
            {
                NX_ASSERT(false,
                    lm("Transaction '%1' can't be send to peer %2 (%3)")
                    .arg(toString(tran.command))
                    .arg(connection->remotePeer().id.toString())
                    .arg(QnLexical::serialized(connection->remotePeer().peerType)));
                return; //< This peer doesn't handle transactions of such type.
            }

            if (connection->remotePeer().isClient())
            {
                if (transportHeader.dstPeers.size() != 1
                    || transportHeader.dstPeers[0] != connection->remotePeer().id)
                {
                    NX_ASSERT(0, lm("Unicast transaction routing error. "
                        "Transaction %1 skipped. remotePeer: %2")
                        .args(tran.command, connection->remotePeer().id));
                    return;
                }
                switch (connection->remotePeer().dataFormat)
                {
                    case Qn::JsonFormat:
                            connection->sendMessage(
                                m_jsonTranSerializer->serializedTransactionWithoutHeader(tran) + QByteArray("\r\n"));
                        break;
                    case Qn::UbjsonFormat:
                        connection->sendMessage(
                            m_ubjsonTranSerializer->serializedTransactionWithoutHeader(tran));
                        break;
                    default:
                        NX_WARNING(this,
                            lm("Client has requested data in an unsupported format %1")
                            .arg(connection->remotePeer().dataFormat));
                        break;
                }
            }
            else
            {
                switch (connection->remotePeer().dataFormat)
                {
                    case Qn::UbjsonFormat:
                        transportHeader.via.insert(localPeer().id);
                        connection->sendMessage(MessageType::pushImpersistentUnicastTransaction,
                            m_ubjsonTranSerializer->serializedTransactionWithHeader(tran, transportHeader));
                        break;
                    default:
                        NX_WARNING(this, lm("Server has requested data in an unsupported format %1")
                            .arg(connection->remotePeer().dataFormat));
                        break;
                    }
            }
        }
    }

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
    const TransportHeader& header);
    virtual bool handlePushImpersistentBroadcastTransaction(
    const P2pConnectionPtr& connection,
    const QByteArray& payload);
protected:
    QMap<vms::api::PersistentIdData, P2pConnectionPtr> getCurrentSubscription() const;

    /**  Local connections are not supposed to be shown in 'aliveMessage' */
    bool isLocalConnection(const vms::api::PersistentIdData& peer) const;
    void createOutgoingConnections(const QMap<vms::api::PersistentIdData, P2pConnectionPtr>& currentSubscription);
    bool hasStartingConnections() const;
    void printPeersMessage();
    P2pConnectionPtr findConnectionById(const vms::api::PersistentIdData& id) const;
    void emitPeerFoundLostSignals();
    void connectSignals(const P2pConnectionPtr& connection);
    void startReading(P2pConnectionPtr connection);
    void sendRuntimeData(const P2pConnectionPtr& connection, const QList<vms::api::PersistentIdData>& peers);

    template<typename T>
    bool processSpecialTransaction(
        const QnTransaction<T>& tran,
        const nx::p2p::P2pConnectionPtr& connection,
        const nx::p2p::TransportHeader& transportHeader)
    {
        if (nx::utils::log::isToBeLogged(nx::utils::log::Level::verbose, this))
            printTran(connection, tran, Connection::Direction::incoming);

        vms::api::PersistentIdData peerId(tran.peerID, tran.persistentInfo.dbID);

        // Process special cases.
        switch (tran.command)
        {
            case ApiCommand::runtimeInfoChanged:
                processRuntimeInfo(tran, connection, transportHeader);
                return true;
            default:
                return false; //< Not a special case.
        }
    }

    void gotTransaction(
        const QnTransaction<vms::api::UpdateSequenceData> &tran,
        const P2pConnectionPtr& connection,
        const TransportHeader& transportHeader);

    void processRuntimeInfo(
        const QnTransaction<vms::api::RuntimeData> &tran,
        const P2pConnectionPtr& connection,
        const TransportHeader& transportHeader);

    template <class T>
    void gotTransaction(const QnTransaction<T>& tran, const P2pConnectionPtr& connection, const TransportHeader& transportHeader);
private:
    void sendAlivePeersMessage(const P2pConnectionPtr& connection = P2pConnectionPtr());

    void doSubscribe(const QMap<vms::api::PersistentIdData, P2pConnectionPtr>& currentSubscription);

    bool handleResolvePeerNumberRequest(const P2pConnectionPtr& connection, const QByteArray& data);
    bool handleResolvePeerNumberResponse(const P2pConnectionPtr& connection, const QByteArray& data);
    bool handlePeersMessage(const P2pConnectionPtr& connection, const QByteArray& data);
    bool handleSubscribeForDataUpdates(const P2pConnectionPtr& connection, const QByteArray& data);
    bool handleSubscribeForAllDataUpdates(const P2pConnectionPtr& connection, const QByteArray& data);
    bool handlePushTransactionList(const P2pConnectionPtr& connection, const QByteArray& tranList);

    friend struct GotTransactionFuction;
    friend struct GotUnicastTransactionFuction;
    friend struct SendTransactionToTransportFuction;

    template <class T>
    void gotUnicastTransaction(
        const QnTransaction<T>& tran,
        const P2pConnectionPtr& connection,
        const TransportHeader& records);

    /*
     * In P2P mode a Client gets transactions only, without any protocol related system messages.
     * It causes client do not receive peerFound/peerLost signals from messageBus any more.
     * This function sends removeRuntimeInfoData transactions to the all connected clients
     * about peer with specified id.
     */
    void sendRuntimeInfoRemovedToClients(const QnUuid& id);
private slots:
    void at_gotMessage(QWeakPointer<ConnectionBase> connection, MessageType messageType, const QByteArray& payload);
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

        std::chrono::milliseconds unauthorizedConnectTimeout = std::chrono::seconds(10);
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
        RemoteConnection(const QnUuid& peerId, const nx::utils::Url& url) : peerId(peerId), url(url) {}

        QnUuid peerId;
        nx::utils::Url url;
        nx::utils::ElapsedTimer unauthorizedTimer;
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
