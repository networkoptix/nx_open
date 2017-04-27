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

    void addOutgoingConnectionToPeer(QnUuid& id, const QUrl& url);
    void removeOutgoingConnectionFromPeer(QnUuid& id);

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
            if (!connection->remotePeerSubscribedTo(peerId))
                continue;
            NX_ASSERT(!(ApiPersistentIdData(connection->remotePeer()) == peerId)); //< loop

            if (QnLog::instance()->logLevel() >= cl_logDEBUG1)
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

private:
    QByteArray serializePeersMessage();
    QByteArray serializeCompressedPeers(MessageType messageType, const QVector<PeerNumberType>& peers);
    QByteArray serializeResolvePeerNumberRequest(const QVector<PeerNumberType>& peers);
    QByteArray serializeResolvePeerNumberResponse(const QVector<PeerNumberType>& peers);
    QByteArray serializeSubscribeRequest(
        const QVector<PeerNumberType>& shortValues,
        const QVector<qint32>& sequences);
private:
    void doPeriodicTasks();
    void processTemporaryOutgoingConnections();
    void removeClosedConnections();
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
private slots:
    void at_gotMessage(const QSharedPointer<P2pConnection>& connection, MessageType messageType, const QByteArray& payload);
    void at_stateChanged(const QSharedPointer<P2pConnection>& connection, P2pConnection::State state);
private:
    QMap<QnUuid, P2pConnectionPtr> m_connections; //< Actual connection list
    QMap<QnUuid, P2pConnectionPtr> m_outgoingConnections; //< Temporary list of outgoing connections
    QMap<QnUuid, QUrl> m_remoteUrls; //< Url list for outgoing connections
    PeerNumberInfo m_localShortPeerInfo; //< Short numbers created by current peer

    typedef QMap<ApiPersistentIdData, RoutingRecord> RoutingInfo;
    struct PeerInfo
    {
        PeerInfo() {}

        qint32 distanceVia(const ApiPersistentIdData& via) const;
        qint32 minDistance(QVector<ApiPersistentIdData>* outViaList) const;

        RoutingInfo routingInfo; // key: route throw, value - distance in hops
    };
    typedef QMap<ApiPersistentIdData, PeerInfo> PeersMap;

    PeersMap m_allPeers; //< all peers in a system

    QMap<ApiPersistentIdData, P2pConnectionPtr> m_subscriptionList;
    QTimer* m_timer = nullptr;
    QElapsedTimer m_lastPeerInfoTimer;
    QElapsedTimer m_lastSubscribeTimer;
};

} // ec2
