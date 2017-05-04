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
#include "transaction.h"
#include <common/static_common_module.h>

namespace ec2 {

namespace detail {
    class QnDbManager;
}

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

class P2pMessageBus: public QnTransactionMessageBusBase
{
    Q_OBJECT;
    using base_type = QnTransactionMessageBusBase;
public:
    P2pMessageBus(
        detail::QnDbManager* db,
        Qn::PeerType peerType,
        QnCommonModule* commonModule,
        QnJsonTransactionSerializer* jsonTranSerializer,
        QnUbjsonTransactionSerializer* ubjsonTranSerializer);
    virtual ~P2pMessageBus();

    void gotConnectionFromRemotePeer(P2pConnectionPtr connection);

    void addOutgoingConnectionToPeer(const QnUuid& id, const QUrl& url);
    void removeOutgoingConnectionFromPeer(const QnUuid& id);

    // Self peer information
    ApiPeerData localPeer() const;

    virtual void stop() override;


    template<class T>
    void sendTransaction(const QnTransaction<T>& tran)
    {
        NX_ASSERT(tran.command != ApiCommand::NotDefined);
        QnMutexLocker lock(&m_mutex);
        if (m_connections.isEmpty())
            return;
        const ApiPersistentIdData peerId(tran.peerID, tran.persistentInfo.dbID);
        for (const auto& connection: m_connections)
        {
            if (connection->miscData().selectingDataInProgress || !connection->updateSequence(tran))
                continue;
            NX_ASSERT(!(ApiPersistentIdData(connection->remotePeer()) == peerId)); //< loop

            if (nx::utils::log::main()->isToBeLogged(cl_logDEBUG1))
                printTran(connection, tran, P2pConnection::Direction::outgoing);

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

    void printTran(
        const P2pConnectionPtr& connection,
        const QnAbstractTransaction& tran,
        P2pConnection::Direction direction) const;

    bool isSubscribedTo(const ApiPersistentIdData& peer) const;
    qint32 distanceTo(const ApiPersistentIdData& peer) const;
    void commitLazyData();
private:
    QByteArray serializePeersMessage();
    QByteArray serializeCompressedPeers(MessageType messageType, const QVector<PeerNumberType>& peers);
    QByteArray serializeResolvePeerNumberRequest(const QVector<PeerNumberType>& peers);
    QByteArray serializeResolvePeerNumberResponse(const QVector<PeerNumberType>& peers);
    QByteArray serializeSubscribeRequest(
        const QVector<PeerNumberType>& shortValues,
        const QVector<qint32>& sequences);
    P2pConnectionPtr findBestConnectionToSubscribe(
        const QVector<ApiPersistentIdData>& viaList) const;
private:
    void doPeriodicTasks();
    void createOutgoingConnections();
    void sendAlivePeersMessage();
    void cleanupRoutingRecords(const ApiPersistentIdData& id);

    void printPeersMessage();
    void printSubscribeMessage(
        const QnUuid& remoteId,
        const QVector<ApiPersistentIdData>& subscribedTo) const;

    QVector<PeerNumberType> deserializeCompressedPeers(const QByteArray& data, bool* success);
    void processAlivePeersMessage(
        const P2pConnectionPtr& connection,
        const QByteArray& data);
    void deserializeResolvePeerNumberResponse(
        const P2pConnectionPtr& connection,
        const QByteArray& response);
    QVector<SubscribeRecord> deserializeSubscribeForDataUpdatesRequest(
        const QByteArray& data,
        bool* success);

    void addOwnfInfoToPeerList();
    void addOfflinePeersFromDb();

    QMap<ApiPersistentIdData, P2pConnectionPtr> getCurrentSubscription() const;
    void resubscribePeers(QMap<ApiPersistentIdData, P2pConnectionPtr> newSubscription);
    void startStopConnections();
    void doSubscribe();
    P2pConnectionPtr findConnectionById(const ApiPersistentIdData& id) const;

    bool handleResolvePeerNumberRequest(const P2pConnectionPtr& connection, const QByteArray& data);
    bool handleResolvePeerNumberResponse(const P2pConnectionPtr& connection, const QByteArray& data);
    bool handlePeersMessage(const P2pConnectionPtr& connection, const QByteArray& data);
    bool handleSubscribeForDataUpdates(const P2pConnectionPtr& connection, const QByteArray& data);
    bool handlePushTransactionData(const P2pConnectionPtr& connection, const QByteArray& data);

    friend struct GotTransactionFuction;

    void P2pMessageBus::gotTransaction(
        const QnTransaction<ApiUpdateSequenceData> &tran,
        const P2pConnectionPtr& connection);

    template <class T>
    void gotTransaction(const QnTransaction<T>& tran,const P2pConnectionPtr& connection);

    void proxyFillerTransaction(const QnAbstractTransaction& tran);
    bool needSubscribeDelay();
    void connectSignals(const P2pConnectionPtr& connection);
    void dropConnections();
    void resotreAfterDbError();
    bool selectAndSendTransactions(const P2pConnectionPtr& connection, QnTranState newSubscription);
    private slots:
    void at_gotMessage(const QSharedPointer<P2pConnection>& connection, MessageType messageType, const QByteArray& payload);
    void at_stateChanged(const QSharedPointer<P2pConnection>& connection, P2pConnection::State state);
    void at_allDataSent(const QSharedPointer<P2pConnection>& connection);
private:
    QMap<QnUuid, P2pConnectionPtr> m_connections; //< Actual connection list
    QMap<QnUuid, P2pConnectionPtr> m_outgoingConnections; //< Temporary list of outgoing connections
    //QMap<QnUuid, QUrl> m_remoteUrls; //< Url list for outgoing connections
    struct RemoteConnection
    {
        RemoteConnection() {}
        RemoteConnection(const QnUuid& id, const QUrl& url) : id(id), url(url) {}

        QnUuid id;
        QUrl url;
    };

    std::vector<RemoteConnection> m_remoteUrls;

    PeerNumberInfo m_localShortPeerInfo; //< Short numbers created by current peer

    typedef QMap<ApiPersistentIdData, RoutingRecord> RoutingInfo;

    struct AlivePeerInfo
    {
        AlivePeerInfo() {}

        qint32 distanceTo(const ApiPersistentIdData& via) const;

        RoutingInfo routeTo; // key: route to, value - distance in hops
    };
    typedef QMap<ApiPersistentIdData, AlivePeerInfo> AlivePeersMap;
    AlivePeersMap m_alivePeers; //< alive peers in the system. key - route via, value - route to

    // Technically this struct is the same as the previous one. But it is used in another semantic.
    struct RouteToPeerInfo
    {
        RouteToPeerInfo() {}

        qint32 minDistance(QVector<ApiPersistentIdData>* outViaList = nullptr) const;

        RoutingInfo routeVia; // key: route via, value - distance in hops
    };
    typedef QMap<ApiPersistentIdData, RouteToPeerInfo> RouteToPeerMap;


    QTimer* m_timer = nullptr;
    QElapsedTimer m_lastPeerInfoTimer;
    QElapsedTimer m_wantToSubscribeTimer;
    QElapsedTimer m_dbCommitTimer;

    int m_lastOutgoingIndex = 0;
    QElapsedTimer m_outConnectionsTimer;
private:
    RouteToPeerMap allPeersDistances() const;
};

} // ec2
