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
    Q_OBJECT
public:
    P2pMessageBus(
        detail::QnDbManager* db,
        Qn::PeerType peerType,
        QnCommonModule* commonModule);
    virtual ~P2pMessageBus();

    void gotConnectionFromRemotePeer(P2pConnectionPtr connection);

    void addOutgoingConnectionToPeer(QnUuid& id, const QUrl& url);
    void removeOutgoingConnectionFromPeer(QnUuid& id);

    // Self peer information
    ApiPeerData localPeer() const;

    void start();
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

    QVector<PeerNumberType> deserializeCompressedPeers(const QByteArray& data, bool* success);
    void deserializeAlivePeersMessage(
        const P2pConnectionPtr& connection,
        const QByteArray& data);
    void deserializeResolvePeerNumberResponse(
        const P2pConnectionPtr& connection,
        const QByteArray& response);

    void addOwnfInfoToPeerList();
    void addOfflinePeersFromDb();

    QMap<ApiPersistentIdData, P2pConnectionPtr> getCurrentSubscription() const;
    void resubscribePeers(QMap<ApiPersistentIdData, P2pConnectionPtr> newSubscription);
    void doSubscribe();
    P2pConnectionPtr findConnectionById(const ApiPersistentIdData& id) const;

    bool handleResolvePeerNumberRequest(const P2pConnectionPtr& connection, const QByteArray& data);
    bool handleResolvePeerNumberResponse(const P2pConnectionPtr& connection, const QByteArray& data);
    bool handleAlivePeers(const P2pConnectionPtr& connection, const QByteArray& data);
    bool handleSubscribeForDataUpdates(const P2pConnectionPtr& connection, const QByteArray& data);
    bool handlePushTransactionData(const P2pConnectionPtr& connection, const QByteArray& data);

    QVector<qint32> getDbSequences(const QVector<ApiPersistentIdData>& peers);

    friend struct GotTransactionFuction;

    template <class T>
    void gotTransaction(const QnTransaction<T>& tran,const P2pConnectionPtr& connection);
private slots:
    void at_gotMessage(const QSharedPointer<P2pConnection>& connection, MessageType messageType, const QByteArray& payload);
private:
    QMap<QnUuid, P2pConnectionPtr> m_connections; //< Actual connection list
    QMap<QnUuid, P2pConnectionPtr> m_outgoingConnections; //< Temporary list of outgoing connections
    QMap<QnUuid, QUrl> m_remoteUrls; //< Url list for outgoing connections
    PeerNumberInfo m_localShortPeerInfo;

    typedef QMap<ApiPersistentIdData, RoutingRecord> RoutingInfo;
    struct PeerInfo
    {
        PeerInfo() {}

        qint32 distanceVia(const ApiPersistentIdData& via) const;
        qint32 minDistance(QVector<ApiPersistentIdData>* outViaList) const;

        bool isOnline = false;
        RoutingInfo routingInfo; // key: route throw, value - distance in hops
    };
    typedef QMap<ApiPersistentIdData, PeerInfo> PeersMap;

    PeersMap m_allPeers; //< all peers in a system

    QMap<ApiPersistentIdData, P2pConnectionPtr> m_subscriptionList;
    QTimer* m_timer = nullptr;
};

} // ec2
