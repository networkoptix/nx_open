#pragma once

#include <QtCore/QTimer>

#include <memory>
#include <nx/utils/uuid.h>
#include <common/common_module_aware.h>
#include <nx/utils/thread/mutex.h>
#include <transaction/transaction_message_bus_base.h>
#include <nx_ec/data/api_tran_state_data.h>
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

namespace ec2 {
namespace detail {
class QnDbManager;
}
}

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
    Q_OBJECT;
    using base_type = ec2::TransactionMessageBusBase;
public:
    MessageBus(
        ec2::detail::QnDbManager* db,
        Qn::PeerType peerType,
        QnCommonModule* commonModule,
        ec2::QnJsonTransactionSerializer* jsonTranSerializer,
        QnUbjsonTransactionSerializer* ubjsonTranSerializer);
    virtual ~MessageBus();

    //void gotConnectionFromRemotePeer(P2pConnectionPtr connection);
    void gotConnectionFromRemotePeer(
        const ec2::ApiPeerDataEx& remotePeer,
        ec2::ConnectionLockGuard connectionLockGuard,
        nx::network::WebSocketPtr webSocket,
        const Qn::UserAccessData& userAccessData,
        std::function<void()> onConnectionClosedCallback);

    virtual void addOutgoingConnectionToPeer(const QnUuid& id, const nx::utils::Url& url) override;
    virtual void removeOutgoingConnectionFromPeer(const QnUuid& id) override;

    QMap<QnUuid, P2pConnectionPtr> connections() const;
    int connectionTries() const;

    // Self peer information
    ApiPeerData localPeer() const;
    ApiPeerDataEx localPeerEx() const;

    virtual void start() override;
    virtual void stop() override;

    virtual QSet<QnUuid> directlyConnectedClientPeers() const override;
    virtual QSet<QnUuid> directlyConnectedServerPeers() const override;
    virtual QnUuid routeToPeerVia(const QnUuid& dstPeer, int* distance) const override;
    virtual int distanceToPeer(const QnUuid& dstPeer) const override;
    virtual void dropConnections() override;
    virtual QVector<QnTransportConnectionInfo> connectionsInfo() const override;

    void sendTransaction(const ec2::QnTransaction<ApiRuntimeData>& tran)
    {
        QnMutexLocker lock(&m_mutex);
        ApiPersistentIdData peerId(tran.peerID, tran.params.peer.persistentId);
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
    void sendTransaction(const ec2::QnTransaction<T>& tran, const QnPeerSet& dstPeers)
    {
        NX_ASSERT(tran.command != ApiCommand::NotDefined);
        QnMutexLocker lock(&m_mutex);
        sendUnicastTransaction(tran, dstPeers);
    }

    bool isSubscribedTo(const ApiPersistentIdData& peer) const;
    qint32 distanceTo(const ApiPersistentIdData& peer) const;

private:
    template<class T>
    void sendTransactionImpl(
        const P2pConnectionPtr& connection,
        const ec2::QnTransaction<T>& tran,
        TransportHeader transportHeader)
    {
        NX_ASSERT(tran.command != ApiCommand::NotDefined);

        if (!connection->shouldTransactionBeSentToPeer(tran))
            return; //< This peer doesn't handle transactions of such type.

        if (transportHeader.via.find(connection->remotePeer().id) != transportHeader.via.end())
            return; //< Already processed by remote peer

        const ApiPersistentIdData remotePeer(connection->remotePeer());
        const auto& descriptor = ec2::getTransactionDescriptorByTransaction(tran);
        auto remoteAccess = descriptor->checkRemotePeerAccessFunc(
            commonModule(), connection.staticCast<Connection>()->userAccessData(), tran.params);
        if (remoteAccess == RemotePeerAccess::Forbidden)
        {
            NX_VERBOSE(
                this,
                lm("Permission check failed while sending transaction %1 to peer %2")
                .arg(tran.toString())
                .arg(remotePeer.id.toString()));
            return;
        }


        const ApiPersistentIdData peerId(tran.peerID, tran.persistentInfo.dbID);
        const auto context = this->context(connection);
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
            if (!descriptor->isPersistent || context->sendDataInProgress)
                return;
        }

        NX_ASSERT(!(remotePeer == peerId)); //< loop

        if (nx::utils::log::isToBeLogged(cl_logDEBUG1, this))
            printTran(connection, tran, Connection::Direction::outgoing);

        switch (connection->remotePeer().dataFormat)
        {
        case Qn::JsonFormat:
            connection->sendMessage(
                m_jsonTranSerializer->serializedTransactionWithoutHeader(tran) + QByteArray("\r\n"));
            break;
        case Qn::UbjsonFormat:
            if (descriptor->isPersistent)
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
    void sendUnicastTransaction(const QnTransaction<T>& tran, const QnPeerSet& dstPeers)
    {
        QMap<P2pConnectionPtr, TransportHeader> dstByConnection;

        // split dstPeers by connection
        for (const auto& peer : dstPeers)
        {
            qint32 distance = kMaxDistance;
            auto dstPeer = routeToPeerVia(peer, &distance);
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

            switch (connection->remotePeer().dataFormat)
            {
            case Qn::JsonFormat:
                if (transportHeader.dstPeers.size() == 1
                    && transportHeader.dstPeers[0] == connection->remotePeer().id)
                {
                    connection->sendMessage(
                        m_jsonTranSerializer->serializedTransactionWithoutHeader(tran) + QByteArray("\r\n"));
                }
                else
                {
                    NX_ASSERT(0, "Unicast transaction routing error. Transaction skipped.");
                }
                break;
            case Qn::UbjsonFormat:
                transportHeader.via.insert(localPeer().id);
                connection->sendMessage(MessageType::pushImpersistentUnicastTransaction,
                    m_ubjsonTranSerializer->serializedTransactionWithHeader(tran, transportHeader));
                break;
            default:
                qWarning() << "Client has requested data in an unsupported format"
                    << connection->remotePeer().dataFormat;
                break;
            }
        }
    }

    void printTran(
        const P2pConnectionPtr& connection,
        const ec2::QnAbstractTransaction& tran,
        Connection::Direction direction) const;
    void commitLazyData();

    P2pConnectionPtr findBestConnectionToSubscribe(
        const QVector<ApiPersistentIdData>& viaList,
        QMap<P2pConnectionPtr, int> newSubscriptions) const;

    void deleteRemoveUrlById(const QnUuid& id);
private:
    void doPeriodicTasksForServer();
    void doPeriodicTasksForClient();
    void createOutgoingConnections(const QMap<ApiPersistentIdData, P2pConnectionPtr>& currentSubscription);
    void sendAlivePeersMessage(const P2pConnectionPtr& connection = P2pConnectionPtr());

    void printPeersMessage();
    void printSubscribeMessage(
        const QnUuid& remoteId,
        const QVector<ApiPersistentIdData>& subscribedTo,
        const QVector<qint32>& sequences) const;

    void addOwnfInfoToPeerList();
    void addOfflinePeersFromDb();

    void emitPeerFoundLostSignals();

    QMap<ApiPersistentIdData, P2pConnectionPtr> getCurrentSubscription() const;
    void resubscribePeers(QMap<ApiPersistentIdData, P2pConnectionPtr> newSubscription);
    void startStopConnections(const QMap<ApiPersistentIdData, P2pConnectionPtr>& currentSubscription);
    bool hasStartingConnections() const;
    void doSubscribe(const QMap<ApiPersistentIdData, P2pConnectionPtr>& currentSubscription);
    P2pConnectionPtr findConnectionById(const ApiPersistentIdData& id) const;

    bool handleResolvePeerNumberRequest(const P2pConnectionPtr& connection, const QByteArray& data);
    bool handleResolvePeerNumberResponse(const P2pConnectionPtr& connection, const QByteArray& data);
    bool handlePeersMessage(const P2pConnectionPtr& connection, const QByteArray& data);
    bool handleSubscribeForDataUpdates(const P2pConnectionPtr& connection, const QByteArray& data);
    bool handleSubscribeForAllDataUpdates(const P2pConnectionPtr& connection, const QByteArray& data);
    bool handlePushTransactionData(const P2pConnectionPtr& connection, const QByteArray& data, const TransportHeader& header);
    bool handlePushTransactionList(const P2pConnectionPtr& connection, const QByteArray& tranList);
    template <typename Function>
    bool handleTransactionWithHeader(const P2pConnectionPtr& connection, const QByteArray& data, Function function);

    friend struct GotTransactionFuction;
    friend struct GotUnicastTransactionFuction;
    friend struct SendTransactionToTransportFuction;

    void gotTransaction(
        const QnTransaction<ApiUpdateSequenceData> &tran,
        const P2pConnectionPtr& connection,
        const TransportHeader& transportHeader);
    void gotTransaction(
        const QnTransaction<ApiRuntimeData> &tran,
        const P2pConnectionPtr& connection,
        const TransportHeader& transportHeader);

    template <class T>
    void gotTransaction(const QnTransaction<T>& tran,const P2pConnectionPtr& connection, const TransportHeader& transportHeader);

    template <class T>
    void gotUnicastTransaction(
        const QnTransaction<T>& tran,
        const P2pConnectionPtr& connection,
        const TransportHeader& records);

    void proxyFillerTransaction(
        const ec2::QnAbstractTransaction& tran,
        const TransportHeader& transportHeader);
    bool needSubscribeDelay();
    void connectSignals(const P2pConnectionPtr& connection);
    void resotreAfterDbError();

    /**
     * Select next portion of data from transaction log and send it.
     * @param addImplicitData - if parameter is false then select only data from newSubscription list.
     * If parameter is true then add all data from sequence 0 what are not mentioned in the list.
     */
    bool selectAndSendTransactions(
        const P2pConnectionPtr& connection,
        QnTranState newSubscription,
        bool addImplicitData);

private slots:
    void at_gotMessage(QWeakPointer<ConnectionBase> connection, MessageType messageType, const QByteArray& payload);
    void at_stateChanged(QWeakPointer<ConnectionBase> connection, Connection::State state);
    void at_allDataSent(QWeakPointer<ConnectionBase> connection);
    bool pushTransactionList(
        const P2pConnectionPtr& connection,
        const QList<QByteArray>& serializedTransactions);
    void startReading(P2pConnectionPtr connection);
    void sendInitialDataToClient(const P2pConnectionPtr& connection);
    void sendInitialDataToCloud(const P2pConnectionPtr& connection);
    void sendRuntimeData(const P2pConnectionPtr& connection, const QList<ApiPersistentIdData>& peers);
    void cleanupRuntimeInfo(const ec2::ApiPersistentIdData& peer);

    /**  Local connections are not supposed to be shown in 'aliveMessage' */
    bool isLocalConnection(const ApiPersistentIdData& peer) const;
public:
    bool needStartConnection(
        const ApiPersistentIdData& peer,
        const QMap<ApiPersistentIdData, P2pConnectionPtr>& currentSubscription) const;
    bool needStartConnection(
        const QnUuid& peerId,
        const QMap<ApiPersistentIdData, P2pConnectionPtr>& currentSubscription) const;
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
    };

    void setDelayIntervals(const DelayIntervals& intervals);
    DelayIntervals delayIntervals() const;

    QMap<ApiPersistentIdData, ApiRuntimeData> runtimeInfo() const;

    void updateOfflineDistance(
        const P2pConnectionPtr& connection,
        const ApiPersistentIdData& to,
        int sequence);
private:
    QMap<QnUuid, P2pConnectionPtr> m_connections; //< Actual connection list
    QMap<QnUuid, P2pConnectionPtr> m_outgoingConnections; //< Temporary list of outgoing connections

    struct RemoteConnection
    {
        RemoteConnection() {}
        RemoteConnection(const QnUuid& peerId, const nx::utils::Url& url) : peerId(peerId), url(url) {}

        QnUuid peerId;
        nx::utils::Url url;
    };

    std::vector<RemoteConnection> m_remoteUrls;

    PeerNumberInfo m_localShortPeerInfo; //< Short numbers created by current peer

    std::unique_ptr<BidirectionRoutingInfo> m_peers;


    struct MiscData
    {
        MiscData(const MessageBus* owner): owner(owner) {}
        void update();

        int expectedConnections = 0;
        int maxSubscriptionToResubscribe = 0;
        int maxDistanceToUseProxy = 0;
        int newConnectionsAtOnce = 1;
    private:
        const MessageBus* owner;
    } m_miscData;
    friend struct MiscData;

    QTimer* m_timer = nullptr;
    QElapsedTimer m_lastPeerInfoTimer;
    QElapsedTimer m_wantToSubscribeTimer;
    QElapsedTimer m_dbCommitTimer;

    int m_lastOutgoingIndex = 0;
    int m_connectionTries = 0;
    QElapsedTimer m_outConnectionsTimer;
    std::set<ApiPeerData> m_lastAlivePeers;
    QMap<ApiPersistentIdData, ApiRuntimeData> m_lastRuntimeInfo;
    DelayIntervals m_intervals;
};

} // namespace p2p
} // namespace nx
