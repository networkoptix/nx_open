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
#include "routing_helpers.h"
#include "connection_context.h"

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

class MessageBus: public ec2::QnTransactionMessageBusBase
{
    Q_OBJECT;
    using base_type = ec2::QnTransactionMessageBusBase;
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
        nx::network::WebSocketPtr webSocket);

    virtual void addOutgoingConnectionToPeer(const QnUuid& id, const QUrl& url) override;
    virtual void removeOutgoingConnectionFromPeer(const QnUuid& id) override;

    QMap<QnUuid, P2pConnectionPtr> connections() const;
    int connectionTries() const;

    // Self peer information
    ApiPeerData localPeer() const;
    ApiPeerDataEx localPeerEx() const;

    virtual void start() override;
    virtual void stop() override;

    virtual QMap<QnUuid, ApiPeerData> aliveServerPeers() const override;
    virtual QMap<QnUuid, ApiPeerData> aliveClientPeers(int maxDistance = std::numeric_limits<int>::max()) const override;
    virtual QnUuid routeToPeerVia(const QnUuid& dstPeer, int* distance) const override;
    virtual int distanceToPeer(const QnUuid& dstPeer) const override;
    virtual void dropConnections() override;
    virtual QVector<QnTransportConnectionInfo> connectionsInfo() const override;

    template<class T>
    void sendTransaction(const ec2::QnTransaction<T>& tran, const QnPeerSet& dstPeers = QnPeerSet())
    {
        NX_ASSERT(tran.command != ApiCommand::NotDefined);
        QnMutexLocker lock(&m_mutex);
        if (m_connections.isEmpty())
            return;

        if (!dstPeers.isEmpty())
        {
            sendUnicastTransaction(tran, dstPeers);
            return;
        }

        const ApiPersistentIdData peerId(tran.peerID, tran.persistentInfo.dbID);
        for (const auto& connection: m_connections)
        {
            if (context(connection)->selectingDataInProgress || !context(connection)->updateSequence(tran))
                continue;
            NX_ASSERT(!(ApiPersistentIdData(connection->remotePeer()) == peerId)); //< loop

            if (nx::utils::log::isToBeLogged(cl_logDEBUG1, QnLog::P2P_TRAN_LOG))
                printTran(connection, tran, Connection::Direction::outgoing);

            switch (connection->remotePeer().dataFormat)
            {
            case Qn::JsonFormat:
                connection->sendMessage(
                    m_jsonTranSerializer->serializedTransactionWithoutHeader(tran) + QByteArray("\r\n"));
                break;
            case Qn::UbjsonFormat:
                connection->sendMessage(MessageType::pushTransactionData,
                    m_ubjsonTranSerializer->serializedTransactionWithoutHeader(tran));
                break;
            default:
                qWarning() << "Client has requested data in an unsupported format" << connection->remotePeer().dataFormat;
                break;
            }
        }
    }

    template<class T>
    void sendUnicastTransaction(const QnTransaction<T>& tran, const QnPeerSet& dstPeers)
    {
        //todo: implement me
    }

    bool isSubscribedTo(const ApiPersistentIdData& peer) const;
    qint32 distanceTo(const ApiPersistentIdData& peer) const;

private:
    void printTran(
        const P2pConnectionPtr& connection,
        const ec2::QnAbstractTransaction& tran,
        Connection::Direction direction) const;
    void commitLazyData();

    P2pConnectionPtr findBestConnectionToSubscribe(
        const QVector<ApiPersistentIdData>& viaList,
        QMap<P2pConnectionPtr, int> newSubscriptions) const;
private:
    void doPeriodicTasks();
    void createOutgoingConnections(const QMap<ApiPersistentIdData, P2pConnectionPtr>& currentSubscription);
    void sendAlivePeersMessage();

    void printPeersMessage();
    void printSubscribeMessage(
        const QnUuid& remoteId,
        const QVector<ApiPersistentIdData>& subscribedTo) const;

    void addOwnfInfoToPeerList();
    void addOfflinePeersFromDb();

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
    bool handlePushTransactionData(const P2pConnectionPtr& connection, const QByteArray& data);
    bool handlePushTransactionList(const P2pConnectionPtr& connection, const QByteArray& tranList);

    friend struct GotTransactionFuction;
    friend struct SendTransactionToTransportFuction;

    void gotTransaction(
        const QnTransaction<ApiUpdateSequenceData> &tran,
        const P2pConnectionPtr& connection);

    template <class T>
    void gotTransaction(const QnTransaction<T>& tran,const P2pConnectionPtr& connection);

    void proxyFillerTransaction(const ec2::QnAbstractTransaction& tran);
    bool needSubscribeDelay();
    void connectSignals(const P2pConnectionPtr& connection);
    void resotreAfterDbError();
    bool selectAndSendTransactions(const P2pConnectionPtr& connection, QnTranState newSubscription);
    private slots:
    void at_gotMessage(QWeakPointer<Connection> connection, MessageType messageType, const QByteArray& payload);
    void at_stateChanged(QWeakPointer<Connection> connection, Connection::State state);
    void at_allDataSent(QWeakPointer<Connection> connection);
    bool pushTransactionList(
        const P2pConnectionPtr& connection,
        const QList<QByteArray>& serializedTransactions);
    void startReading(P2pConnectionPtr connection);
public:
    bool needStartConnection(
        const ApiPersistentIdData& peer,
        const QMap<ApiPersistentIdData, P2pConnectionPtr>& currentSubscription) const;
    bool needStartConnection(
        const QnUuid& peerId,
        const QMap<ApiPersistentIdData, P2pConnectionPtr>& currentSubscription) const;
    static ConnectionContext* context(const P2pConnectionPtr& connection);
private:
    QMap<QnUuid, P2pConnectionPtr> m_connections; //< Actual connection list
    QMap<QnUuid, P2pConnectionPtr> m_outgoingConnections; //< Temporary list of outgoing connections

    struct RemoteConnection
    {
        RemoteConnection() {}
        RemoteConnection(const QnUuid& peerId, const QUrl& url) : peerId(peerId), url(url) {}

        QnUuid peerId;
        QUrl url;
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
        int newConnectionsAtOnce = 0;
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
};

} // namespace p2p
} // namespace nx
