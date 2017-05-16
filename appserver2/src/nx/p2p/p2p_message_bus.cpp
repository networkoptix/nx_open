#include "p2p_message_bus.h"

#include <common/common_module.h>
#include <utils/media/bitStream.h>
#include <utils/common/synctime.h>
#include <database/db_manager.h>
#include "ec_connection_notification_manager.h"
#include <transaction/transaction_message_bus_priv.h>
#include <nx/utils/random.h>
#include <transaction/ubjson_transaction_serializer.h>
#include <transaction/json_transaction_serializer.h>
#include <api/global_settings.h>
#include <nx_ec/ec_proto_version.h>
#include <utils/math/math.h>
#include <http/p2p_connection_listener.h>
#include <nx/p2p/p2p_serialization.h>

namespace {

    // todo: move these timeouts to the globalSettings

    // How often send 'peers' message to the peer if something is changed
    // As soon as connection is opened first message is sent immediately
    std::chrono::seconds sendPeersInfoInterval(15);

    // If new connection is recently established/closed, don't sent subscribe request to the peer
    std::chrono::seconds subscribeIntervalLow(3);

    // If new connections always established/closed too long time, send subscribe request anyway
    std::chrono::seconds subscribeIntervalHigh(15);

    int commitIntervalMs = 1000;
    static const int kMaxSelectDataSize = 1024 * 32;

    static const int kOutConnectionsInterval = 1000;

} // namespace

namespace nx {
namespace p2p {

using namespace ec2;

// ---------------------- P2pMessageBus --------------

P2pMessageBus::P2pMessageBus(
    ec2::detail::QnDbManager* db,
    Qn::PeerType peerType,
    QnCommonModule* commonModule,
    QnJsonTransactionSerializer* jsonTranSerializer,
    QnUbjsonTransactionSerializer* ubjsonTranSerializer)
:
    QnTransactionMessageBusBase(db, peerType, commonModule, jsonTranSerializer, ubjsonTranSerializer),
    m_miscData(this)
{
    qRegisterMetaType<MessageType>();
    qRegisterMetaType<P2pConnection::State>("P2pConnection::State");
    qRegisterMetaType<P2pConnectionPtr>("P2pConnectionPtr");
    qRegisterMetaType<QWeakPointer<P2pConnection>>("QWeakPointer<P2pConnection>");

    m_thread->setObjectName("P2pMessageBus");
    m_timer = new QTimer();
    connect(m_timer, &QTimer::timeout, this, &P2pMessageBus::doPeriodicTasks);
    m_timer->start(500);
    m_dbCommitTimer.restart();
}

P2pMessageBus::~P2pMessageBus()
{
    stop();
    delete m_timer;
}

void P2pMessageBus::dropConnections()
{
    QnMutexLocker lock(&m_mutex);
    m_connections.clear();
    m_outgoingConnections.clear();
    if (m_peers)
        m_peers->clear();
}


void P2pMessageBus::printTran(
    const P2pConnectionPtr& connection,
    const ec2::QnAbstractTransaction& tran,
    P2pConnection::Direction direction) const
{

    auto localPeerName = qnStaticCommon->moduleDisplayName(commonModule()->moduleGUID());
    auto remotePeerName = qnStaticCommon->moduleDisplayName(connection->remotePeer().id);
    QString msgName;
    QString directionName;
    if (direction == P2pConnection::Direction::outgoing)
    {
        msgName = lm("Send");
        directionName = lm("--->");
    }
    else
    {
        msgName = lm("Got");
        directionName = lm("<---");
    }

    NX_LOG(QnLog::P2P_TRAN_LOG,
        lit("%1 tran:\t %2 %3 %4. Command: %5. Created by %6")
        .arg(msgName)
        .arg(localPeerName)
        .arg(directionName)
        .arg(remotePeerName)
        .arg(toString(tran.command))
        .arg(qnStaticCommon->moduleDisplayName(tran.peerID)),
        cl_logDEBUG1);
}

void P2pMessageBus::start()
{
    m_localShortPeerInfo.encode(localPeer(), 0);
    m_peers.reset(new BidirectionRoutingInfo(localPeer()));
    addOfflinePeersFromDb();

    base_type::start();
}

void P2pMessageBus::stop()

{
    {
        QnMutexLocker lock(&m_mutex);
        m_remoteUrls.clear();
    }

    dropConnections();
    base_type::stop();
}

// P2pMessageBus

void P2pMessageBus::addOutgoingConnectionToPeer(const QnUuid& peer, const QUrl& _url)
{
    QnMutexLocker lock(&m_mutex);

    QUrl url(_url);
    url.setPath(P2pConnectionProcessor::kUrlPath);

    int pos = nx::utils::random::number((int) 0, (int) m_remoteUrls.size());
    m_remoteUrls.insert(m_remoteUrls.begin() + pos, RemoteConnection(peer, url));
}

void P2pMessageBus::removeOutgoingConnectionFromPeer(const QnUuid& id)
{
    QnMutexLocker lock(&m_mutex);
    for (int i = 0; i < m_remoteUrls.size() - 1; ++i)
    {
        if (m_remoteUrls[i].peerId == id)
        {
            m_remoteUrls.erase(m_remoteUrls.begin() + i);
            break;
        }
    }

    m_outgoingConnections.remove(id);
    auto itr = m_connections.find(id);
    if (itr != m_connections.end() && itr.value()->direction() == P2pConnection::Direction::outgoing)
        itr.value()->setState(P2pConnection::State::Error);
}

void P2pMessageBus::gotConnectionFromRemotePeer(P2pConnectionPtr connection)
{
    QnMutexLocker lock(&m_mutex);

    const auto remoteId = connection->remotePeer().id;
    m_peers->removePeer(connection->remotePeer());
    m_connections[remoteId] = connection;
    connectSignals(connection);
    connection->startReading();
}

void P2pMessageBus::connectSignals(const P2pConnectionPtr& connection)
{
    QnMutexLocker lock(&m_mutex);
    connect(connection.data(), &P2pConnection::stateChanged, this, &P2pMessageBus::at_stateChanged);
    connect(connection.data(), &P2pConnection::gotMessage, this, &P2pMessageBus::at_gotMessage);
    connect(connection.data(), &P2pConnection::allDataSent, this, &P2pMessageBus::at_allDataSent);
}

void P2pMessageBus::createOutgoingConnections(const QMap<ApiPersistentIdData, P2pConnectionPtr>& currentSubscription)
{
    if (hasStartingConnections())
        return;

    if (m_outConnectionsTimer.isValid() && !m_outConnectionsTimer.hasExpired(kOutConnectionsInterval))
        return;
    m_outConnectionsTimer.restart();

    auto itr = m_remoteUrls.begin();

    for (int i = 0; i < m_remoteUrls.size(); ++i)
    {
        if (m_outgoingConnections.size() >= m_miscData.newConnectionsAtOnce)
            return; //< wait a bit

        int pos = m_lastOutgoingIndex % m_remoteUrls.size();
        ++m_lastOutgoingIndex;

        const RemoteConnection& remoteConnection = m_remoteUrls[pos];
        if (!m_connections.contains(remoteConnection.peerId) && !m_outgoingConnections.contains(remoteConnection.peerId))
        {
            if (!needStartConnection(remoteConnection.peerId, currentSubscription))
            {
                continue;
            }

            {
                // This check is redundant (can be ommited). But it reduce network race condition time.
                // So, it reduce frequency of in/out conflict and network traffic a bit.
                if (m_connectionGuardSharedState.contains(remoteConnection.peerId))
                    continue; //< incoming connection in progress
            }

            ConnectionLockGuard connectionLockGuard(
                commonModule()->moduleGUID(),
                connectionGuardSharedState(),
                remoteConnection.peerId,
                ConnectionLockGuard::Direction::Outgoing);

            P2pConnectionPtr connection(new P2pConnection(
                commonModule(),
                remoteConnection.peerId,
                localPeerEx(),
                std::move(connectionLockGuard),
                remoteConnection.url));
            m_outgoingConnections.insert(remoteConnection.peerId, connection);
            ++m_connectionTries;
            connectSignals(connection);
            connection->startConnection();
        }
    }
}

void P2pMessageBus::addOfflinePeersFromDb()
{
    if (!m_db)
        return;
    const auto localPeer = this->localPeer();

    auto persistentState = m_db->transactionLog()->getTransactionsState().values;
    const qint64 currentTimeMs = qnSyncTime->currentMSecsSinceEpoch();
    for (auto itr = persistentState.cbegin(); itr != persistentState.cend(); ++itr)
    {
        const auto& peer = itr.key();
        if (peer == localPeer)
            continue;

        const qint32 sequence = itr.value();
        const qint32 localOfflineDistance = kMaxDistance - sequence;
        nx::p2p::RoutingRecord record(localOfflineDistance, currentTimeMs);
        m_peers->addRecord(localPeer, peer, record);
    }
}

void P2pMessageBus::printPeersMessage()
{
    QList<QString> records;

    for (auto itr = m_peers->allPeerDistances.constBegin(); itr != m_peers->allPeerDistances.constEnd(); ++itr)
    {
        const auto& peer = itr.value();

        qint32 minDistance = peer.minDistance();
        if (minDistance == kMaxDistance)
            continue;
        qint16 peerNumber = m_localShortPeerInfo.encode(itr.key());

        records << lit("\t\t\t\t\t To:  %1(dbId=%2). Distance: %3")
            .arg(qnStaticCommon->moduleDisplayName(itr.key().id))
            .arg(itr.key().persistentId.toString())
            .arg(minDistance);
    }

    NX_LOG(QnLog::P2P_TRAN_LOG,
        lit("Peer %1 records:\n%3")
        .arg(qnStaticCommon->moduleDisplayName(localPeer().id))
        .arg(records.join("\n")),
        cl_logDEBUG1);
}

void P2pMessageBus::printSubscribeMessage(
    const QnUuid& remoteId,
    const QVector<ApiPersistentIdData>& subscribedTo) const
{
    QList<QString> records;

    for (const auto& peer: subscribedTo)
    {
        records << lit("\t\t\t\t\t To:  %1(dbId=%2)")
            .arg(qnStaticCommon->moduleDisplayName(peer.id))
            .arg(peer.persistentId.toString());
    }

    NX_LOG(QnLog::P2P_TRAN_LOG, lit("Subscribe:\t %1 ---> %2:\n%3")
        .arg(qnStaticCommon->moduleDisplayName(localPeer().id))
        .arg(qnStaticCommon->moduleDisplayName(remoteId))
        .arg(records.join("\n")),
        cl_logDEBUG1);
}

void P2pMessageBus::sendAlivePeersMessage()
{
    QVector<PeerDistanceRecord> records;
    records.reserve(m_peers->allPeerDistances.size());
    for (auto itr = m_peers->allPeerDistances.cbegin(); itr != m_peers->allPeerDistances.cend(); ++itr)
    {
        qint32 minDistance = itr->minDistance();
        if (minDistance == kMaxDistance)
            continue;
        const qint16 peerNumber = m_localShortPeerInfo.encode(itr.key());
        records.push_back(PeerDistanceRecord(peerNumber, minDistance));
    }
    QByteArray data = serializePeersMessage(records, 1);
    data.data()[0] = (quint8) MessageType::alivePeers;

    int peersIntervalMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        sendPeersInfoInterval).count();
    for (const auto& connection : m_connections)
    {
        if (connection->state() != P2pConnection::State::Connected)
            continue;
        if (!connection->miscData().isRemoteStarted)
            continue;

        auto& miscData = connection->miscData();
        if (miscData.localPeersTimer.isValid() && !miscData.localPeersTimer.hasExpired(peersIntervalMs))
            continue;
        if (data != miscData.localPeersMessage)
        {
            miscData.localPeersMessage = data;
            miscData.localPeersTimer.restart();
            if (nx::utils::log::isToBeLogged(cl_logDEBUG1, QnLog::P2P_TRAN_LOG))
                printPeersMessage();
            connection->sendMessage(data);
        }
    }
}

QMap<ApiPersistentIdData, P2pConnectionPtr> P2pMessageBus::getCurrentSubscription() const
{
    QMap<ApiPersistentIdData, P2pConnectionPtr> result;
    for (auto itr = m_connections.cbegin(); itr != m_connections.cend(); ++itr)
    {
        for (const auto peer: itr.value()->miscData().localSubscription)
            result[peer] = itr.value();
    }
    return result;
}

void P2pMessageBus::resubscribePeers(
    QMap<ApiPersistentIdData, P2pConnectionPtr> newSubscription)
{
    QMap<P2pConnectionPtr, QVector<ApiPersistentIdData>> updatedSubscription;
    for (auto itr = newSubscription.begin(); itr != newSubscription.end(); ++itr)
        updatedSubscription[itr.value()].push_back(itr.key());
    for (auto& value: updatedSubscription)
        std::sort(value.begin(), value.end());
    for (auto connection: m_connections.values())
    {
        const auto& newValue = updatedSubscription.value(connection);
        auto& miscData = connection->miscData();
        if (miscData.localSubscription != newValue)
        {
            miscData.localSubscription = newValue;
            QVector<PeerNumberType> shortValues;
            for (const auto& id: newValue)
                shortValues.push_back(connection->encode(id));
            if (nx::utils::log::isToBeLogged(cl_logDEBUG1, QnLog::P2P_TRAN_LOG))
                printSubscribeMessage(connection->remotePeer().id, miscData.localSubscription);

            NX_ASSERT(newValue.contains(connection->remotePeer()));
            auto sequences = m_db->transactionLog()->getTransactionsState(newValue);
            QVector<SubscribeRecord> request;
            request.reserve(shortValues.size());
            for (int i = 0; i < shortValues.size(); ++i)
                request.push_back(SubscribeRecord(shortValues[i], sequences[i]));
            connection->sendMessage(serializeSubscribeRequest(request));
            miscData.remoteSelectingDataInProgress = true;
        }
    }
}

P2pConnectionPtr P2pMessageBus::findConnectionById(const ApiPersistentIdData& id) const
{
    P2pConnectionPtr result =  m_connections.value(id.id);
    return result && result->remotePeer().persistentId == id.persistentId ? result : P2pConnectionPtr();
}

bool P2pMessageBus::needSubscribeDelay()
{
    // If alive peers has been recently received, postpone subscription for some time.
    // This peer could be found via another neighbor.
    if (m_lastPeerInfoTimer.isValid())
    {
        std::chrono::milliseconds lastPeerInfoElapsed(m_lastPeerInfoTimer.elapsed());

        if (lastPeerInfoElapsed < subscribeIntervalLow)
        {
            if (!m_wantToSubscribeTimer.isValid())
                m_wantToSubscribeTimer.restart();
            std::chrono::milliseconds wantToSubscribeElapsed(m_wantToSubscribeTimer.elapsed());
            if (wantToSubscribeElapsed < subscribeIntervalHigh)
                return true;
        }
    }
    m_wantToSubscribeTimer.invalidate();
    return false;
}

bool P2pMessageBus::needStartConnection(
    const ApiPersistentIdData& peer,
    const QMap<ApiPersistentIdData, P2pConnectionPtr>& currentSubscription) const
{
    const RouteToPeerMap& allPeerDistances = m_peers->allPeerDistances;
    qint32 currentDistance = allPeerDistances.value(peer).minDistance();
    const auto& subscribedVia = currentSubscription.value(peer);
    return currentDistance > m_miscData.maxDistanceToUseProxy
        || (subscribedVia && subscribedVia->miscData().localSubscription.size() > m_miscData.maxSubscriptionToResubscribe);
}

bool P2pMessageBus::needStartConnection(
    const QnUuid& peerId,
    const QMap<ApiPersistentIdData, P2pConnectionPtr>& currentSubscription) const
{
    const RouteToPeerMap& allPeerDistances = m_peers->allPeerDistances;

    bool result = true;
    auto itr = allPeerDistances.lowerBound(ApiPersistentIdData(peerId, QnUuid()));
    for (; itr != allPeerDistances.end() && itr.key().id == peerId; ++itr)
    {
        result &= needStartConnection(itr.key(), currentSubscription);
    }
    return result;
}

bool P2pMessageBus::hasStartingConnections() const
{
    for (const auto& connection : m_connections)
    {
        const auto& data = connection->miscData();
        if (data.isLocalStarted && data.remotePeersMessage.isEmpty())
            return true;
    }
    return false;
}

void P2pMessageBus::startStopConnections(const QMap<ApiPersistentIdData, P2pConnectionPtr>& currentSubscription)
{
    const RouteToPeerMap& allPeerDistances = m_peers->allPeerDistances;

    if (hasStartingConnections())
        return;

    // start using connection if need
    int maxStartsAtOnce = m_miscData.newConnectionsAtOnce;

    for (auto& connection: m_connections)
    {
        if (connection->state() != P2pConnection::State::Connected ||  connection->miscData().isLocalStarted)
            continue; //< already in use or not ready yet

        ApiPersistentIdData peer = connection->remotePeer();
        if (needStartConnection(peer, currentSubscription))
        {
            connection->miscData().isLocalStarted = true;
            connection->miscData().sendStartTimer.restart();
            connection->sendMessage(MessageType::start, QByteArray());
            if (--maxStartsAtOnce == 0)
                return; //< limit start requests at once
        }
    }
}

P2pConnectionPtr P2pMessageBus::findBestConnectionToSubscribe(
    const QVector<ApiPersistentIdData>& viaList,
    QMap<P2pConnectionPtr, int> newSubscriptions) const
{
    P2pConnectionPtr result;
    int minSubscriptions = std::numeric_limits<int>::max();
    for (const auto& via: viaList)
    {
        auto connection = findConnectionById(via);
        NX_ASSERT(connection);
        if (!connection)
            continue;
        int subscriptions = connection->miscData().localSubscription.size() + newSubscriptions.value(connection);
        if (subscriptions < minSubscriptions)
        {
            minSubscriptions = subscriptions;
            result = connection;
        }
    }

    return result;
}

void P2pMessageBus::doSubscribe(const QMap<ApiPersistentIdData, P2pConnectionPtr>& currentSubscription)
{
    if (hasStartingConnections())
        return;

    for (const auto& connection: m_connections)
    {
        const auto& data = connection->miscData();
        if (data.remoteSelectingDataInProgress)
            return;
    }


    const bool needDelay = needSubscribeDelay();

    QMap<ApiPersistentIdData, P2pConnectionPtr> newSubscription = currentSubscription;
    QMap<P2pConnectionPtr, int> tmpNewSubscription; //< this connections will get N new subscriptions soon
    const auto localPeer = this->localPeer();
    bool isUpdated = false;

    const RouteToPeerMap& allPeerDistances = m_peers->allPeerDistances;
    const auto& alivePeers =  m_peers->alivePeers;
    auto currentSubscriptionItr = currentSubscription.cbegin();
    for (auto itr = allPeerDistances.constBegin(); itr != allPeerDistances.constEnd(); ++itr)
    {
        const ApiPersistentIdData& peer = itr.key();
        if (peer == localPeer)
            continue;

        //auto subscribedVia = currentSubscription.value(peer);
        while (currentSubscriptionItr != currentSubscription.cend() && currentSubscriptionItr.key() < peer)
            ++currentSubscriptionItr;
        P2pConnectionPtr subscribedVia;
        if (currentSubscriptionItr != currentSubscription.cend() && currentSubscriptionItr.key() == peer)
            subscribedVia = currentSubscriptionItr.value();

        qint32 subscribedDistance =
            alivePeers[subscribedVia ? subscribedVia->remotePeer() : localPeer].distanceTo(peer);

        const auto& info = itr.value();
        qint32 minDistance = info.minDistance();
        if (minDistance < subscribedDistance)
        {
            if (minDistance > 1)
            {
                if (needDelay)
                    continue; //< allow only direct subscription if network configuration are still changing
            }
            QVector<ApiPersistentIdData> viaList;
            info.minDistance(&viaList);

            NX_ASSERT(!viaList.empty());
            // If any of connections with min distance subscribed to us then postpone our subscription.
            // It could happen if neighbor(or closer) peer just goes offline
            if (std::any_of(viaList.begin(), viaList.end(),
                [this, &peer](const ApiPersistentIdData& via)
            {
                auto connection = findConnectionById(via);
                NX_ASSERT(connection);
                return connection->remotePeerSubscribedTo(peer);
            }))
            {
                continue;
            }

            auto connection = findBestConnectionToSubscribe(viaList, tmpNewSubscription);
            if (subscribedVia)
            {
                NX_LOG(QnLog::P2P_TRAN_LOG,
                    lit("Peer %1 is changing subscription to %2. subscribed via %3. new subscribe via %4")
                    .arg(qnStaticCommon->moduleDisplayName(localPeer.id))
                    .arg(qnStaticCommon->moduleDisplayName(peer.id))
                    .arg(qnStaticCommon->moduleDisplayName(subscribedVia->remotePeer().id))
                    .arg(qnStaticCommon->moduleDisplayName(connection->remotePeer().id)),
                    cl_logDEBUG1);
            }
            newSubscription[peer] = connection;
            ++tmpNewSubscription[connection];
            isUpdated = true;
        }
    }
    if (isUpdated)
        resubscribePeers(newSubscription);
}

void P2pMessageBus::commitLazyData()
{
    if (m_dbCommitTimer.hasExpired(commitIntervalMs))
    {
        ec2::detail::QnDbManager::QnDbTransactionLocker dbTran(m_db->getTransaction());
        dbTran.commit();
        m_dbCommitTimer.restart();
    }
}

void P2pMessageBus::MiscData::update()
{
    expectedConnections = std::max(1, std::max(owner->m_connections.size(), (int) owner->m_remoteUrls.size()));
    maxSubscriptionToResubscribe = std::round(std::sqrt(expectedConnections)) * 2;
    maxDistanceToUseProxy = std::max(2, int(std::sqrt(std::sqrt(expectedConnections))));
    newConnectionsAtOnce = std::max(1, int(std::round(std::sqrt(expectedConnections))) / 2);
}

void P2pMessageBus::doPeriodicTasks()
{
    QnMutexLocker lock(&m_mutex);

    m_miscData.update();

    const QMap<ApiPersistentIdData, P2pConnectionPtr>& currentSubscription = getCurrentSubscription();
    createOutgoingConnections(currentSubscription); //< open new connections

    sendAlivePeersMessage();
    startStopConnections(currentSubscription);
    doSubscribe(currentSubscription);
    commitLazyData();
}

ApiPeerData P2pMessageBus::localPeer() const
{
    return ApiPeerData(
        commonModule()->moduleGUID(),
        commonModule()->runningInstanceGUID(),
        commonModule()->dbId(),
        m_localPeerType);
}

ApiPeerDataEx P2pMessageBus::localPeerEx() const
{
    ApiPeerDataEx result;

    result.id = commonModule()->moduleGUID();
    result.persistentId = commonModule()->dbId();
    result.instanceId = commonModule()->runningInstanceGUID();
    result.systemId = commonModule()->globalSettings()->localSystemId();
    result.peerType = Qn::PT_Server;
    result.cloudHost = nx::network::AppInfo::defaultCloudHost();
    result.identityTime = commonModule()->systemIdentityTime();
    result.keepAliveTimeout = commonModule()->globalSettings()->connectionKeepAliveTimeout().count();
    result.protoVersion = nx_ec::EC2_PROTO_VERSION;
    result.dataFormat = Qn::UbjsonFormat;
    return result;
}

void P2pMessageBus::at_stateChanged(
    QWeakPointer<P2pConnection> weakRef,
    P2pConnection::State state)
{
    P2pConnectionPtr connection = weakRef.toStrongRef();
    if (!connection)
        return;

    QnMutexLocker lock(&m_mutex);

    const auto& remoteId = connection->remotePeer().id;
    switch (connection->state())
    {
        case P2pConnection::State::Connected:
            if (connection->direction() == P2pConnection::Direction::outgoing)
            {
                m_connections[remoteId] = connection;
                m_outgoingConnections.remove(remoteId);
                connection->startReading();
            }
            break;
        case P2pConnection::State::Error:
        {
            NX_LOG(QnLog::P2P_TRAN_LOG, lit("Peer %1 has closed connection to %2")
                .arg(qnStaticCommon->moduleDisplayName(localPeer().id))
                .arg(qnStaticCommon->moduleDisplayName(connection->remotePeer().id)),
                cl_logDEBUG1);


            auto outgoingConnection = m_outgoingConnections.value(remoteId);
            if (outgoingConnection == connection)
            {
                m_outgoingConnections.remove(remoteId);
            }
            else
            {
                auto connectedConnection = m_connections.value(remoteId);
                if (connectedConnection == connection)
                {
                    m_peers->removePeer(connection->remotePeer());
                    m_connections.remove(remoteId);
                }
            }
            break;
        }
        default:
            break;
    }
}

void P2pMessageBus::at_allDataSent(QWeakPointer<P2pConnection> weakRef)
{
    P2pConnectionPtr connection = weakRef.toStrongRef();
    if (!connection)
        return;

    QnMutexLocker lock(&m_mutex);
    if (m_connections.value(connection->remotePeer().id) != connection)
        return;
    if (connection->miscData().selectingDataInProgress)
        selectAndSendTransactions(connection, connection->miscData().remoteSubscription);

}

void P2pMessageBus::at_gotMessage(
    QWeakPointer<P2pConnection> weakRef,
    MessageType messageType,
    const nx::Buffer& payload)
{
    P2pConnectionPtr connection = weakRef.toStrongRef();
    if (!connection)
        return;

    QnMutexLocker lock(&m_mutex);
    if (m_connections.value(connection->remotePeer().id) != connection)
        return;


    if (connection->state() == P2pConnection::State::Error)
        return; //< Connection has been closed
    if (nx::utils::log::isToBeLogged(cl_logDEBUG1, QnLog::P2P_TRAN_LOG) &&
        messageType != MessageType::pushTransactionData &&
        messageType != MessageType::pushTransactionList)
    {
        auto localPeerName = qnStaticCommon->moduleDisplayName(commonModule()->moduleGUID());
        auto remotePeerName = qnStaticCommon->moduleDisplayName(connection->remotePeer().id);

        NX_LOG(QnLog::P2P_TRAN_LOG, lit("Got message:\t %1 <--- %2. Type: %3. Size=%4")
            .arg(localPeerName)
            .arg(remotePeerName)
            .arg(toString(messageType))
            .arg(payload.size() + 1),
            cl_logDEBUG1);
    }


    bool result = false;
    switch (messageType)
    {
    case MessageType::start:
        connection->miscData().isRemoteStarted = true;
        result = true;
        break;
    case MessageType::stop:
        connection->miscData().selectingDataInProgress = false;
        connection->miscData().isRemoteStarted = false;
        connection->miscData().remoteSubscription.values.clear();
        break;
    case MessageType::resolvePeerNumberRequest:
        result = handleResolvePeerNumberRequest(connection, payload);
        break;
    case MessageType::resolvePeerNumberResponse:
        result = handleResolvePeerNumberResponse(connection, payload);
        break;
    case MessageType::alivePeers:
        result = handlePeersMessage(connection, payload);
        break;
    case MessageType::subscribeForDataUpdates:
        result = handleSubscribeForDataUpdates(connection, payload);
        break;
    case MessageType::pushTransactionData:
        result = handlePushTransactionData(connection, payload);
        break;
    case MessageType::pushTransactionList:
        result = handlePushTransactionList(connection, payload);
        break;
    default:
        NX_ASSERT(0, lm("Unknown message type").arg((int)messageType));
        break;
    }
    if (!result)
        connection->setState(P2pConnection::State::Error);
}

bool P2pMessageBus::handleResolvePeerNumberRequest(const P2pConnectionPtr& connection, const QByteArray& data)
{
    bool success = false;
    QVector<PeerNumberType> request = deserializeCompressedPeers(data, &success);
    if (!success)
        return false;

    QVector<PeerNumberResponseRecord> response;
    response.reserve(request.size());
    for (const auto& peer : request)
        response.push_back(PeerNumberResponseRecord(peer, m_localShortPeerInfo.decode(peer)));

    auto responseData = serializeResolvePeerNumberResponse(response, 1);
    responseData.data()[0] = (quint8) MessageType::resolvePeerNumberResponse;
    connection->sendMessage(responseData);
    return true;
}

bool P2pMessageBus::handleResolvePeerNumberResponse(const P2pConnectionPtr& connection, const QByteArray& data)
{
    bool success = false;
    auto records = deserializeResolvePeerNumberResponse(data, &success);
    if (!success)
        return false;

    auto& miscData = connection->miscData();
    for (const auto& record: records)
    {
        connection->encode(record, record.peerNumber);

        auto itr = std::find(
            miscData.awaitingNumbersToResolve.begin(),
            miscData.awaitingNumbersToResolve.end(),
            record.peerNumber);
        if (itr != miscData.awaitingNumbersToResolve.end())
            miscData.awaitingNumbersToResolve.erase(itr);
    }

    const QByteArray msg = connection->miscData().remotePeersMessage;
    if (!msg.isEmpty())
        handlePeersMessage(connection, msg);
    return true;
}

bool P2pMessageBus::handlePeersMessage(const P2pConnectionPtr& connection, const QByteArray& data)
{
    NX_ASSERT(!data.isEmpty());

    bool success = false;
    auto peers = deserializePeersMessage(data, &success);
    if (!success)
        return false;

    m_lastPeerInfoTimer.restart();

    QVector<PeerNumberType> numbersToResolve;
    BitStreamReader reader((const quint8*)data.data(), data.size());
    for (const auto& peer: peers)
    {
        if (connection->decode(peer.peerNumber).isNull())
            numbersToResolve.push_back(peer.peerNumber);
    }
    connection->miscData().remotePeersMessage = data;

    if (numbersToResolve.empty())
    {
        m_peers->removePeer(connection->remotePeer());
        auto& shortPeers = connection->shortPeers();
        qint64 timeMs = qnSyncTime->currentMSecsSinceEpoch();
        for (const auto& peer: peers)
        {
            m_peers->addRecord(
                connection->remotePeer(),
                shortPeers.decode(peer.peerNumber),
                nx::p2p::RoutingRecord(peer.distance + 1, timeMs));
        }
        return true;
    }

    auto& miscData = connection->miscData();
    std::sort(numbersToResolve.begin(), numbersToResolve.end());
    QVector<PeerNumberType> moreNumbersToResolve;

    std::set_difference(
        numbersToResolve.begin(),
        numbersToResolve.end(),
        miscData.awaitingNumbersToResolve.begin(),
        miscData.awaitingNumbersToResolve.end(),
        std::inserter(moreNumbersToResolve, moreNumbersToResolve.begin()));

    if (moreNumbersToResolve.isEmpty())
        return true;

    for (const auto& number: moreNumbersToResolve)
        miscData.awaitingNumbersToResolve.push_back(number);
    std::sort(miscData.awaitingNumbersToResolve.begin(), miscData.awaitingNumbersToResolve.end());

    auto serializedData = serializeCompressedPeers(moreNumbersToResolve, 1);
    serializedData.data()[0] = (quint8) MessageType::resolvePeerNumberRequest;
    connection->sendMessage(serializedData);
    return true;
}

struct SendTransactionToTransportFuction
{
    typedef void result_type;

    template<class T>
    void operator()(
        P2pMessageBus* bus,
        const QnTransaction<T> &transaction,
        const P2pConnectionPtr& connection) const
    {
        ApiPersistentIdData tranId(transaction.peerID, transaction.persistentInfo.dbID);
        NX_ASSERT(connection->remotePeerSubscribedTo(tranId));
        NX_ASSERT(!(ApiPersistentIdData(connection->remotePeer()) == tranId));

        switch (connection->remotePeer().dataFormat)
        {
            case Qn::JsonFormat:
                connection->sendMessage(
                    bus->jsonTranSerializer()->serializedTransactionWithoutHeader(transaction) + QByteArray("\r\n"));
                break;
            case Qn::UbjsonFormat:
                connection->sendMessage(MessageType::pushTransactionData,
                    bus->ubjsonTranSerializer()->serializedTransactionWithoutHeader(transaction));
                break;
            default:
                qWarning() << "Client has requested data in an unsupported format" << connection->remotePeer().dataFormat;
                break;
        }
    }
};

struct SendTransactionToTransportFastFuction
{
    bool operator()(
        P2pMessageBus* /*bus*/,
        Qn::SerializationFormat /* srcFormat */,
        const QByteArray& serializedTran,
        const P2pConnectionPtr& connection) const
    {
        connection->sendMessage(MessageType::pushTransactionData, serializedTran);
        return true;
    }
};


bool P2pMessageBus::handleSubscribeForDataUpdates(const P2pConnectionPtr& connection, const QByteArray& data)
{
    bool success = false;
    QVector<SubscribeRecord> request = deserializeSubscribeRequest(data, &success);
    if (!success)
        return false;
    QnTranState newSubscription;
    for (const auto& shortPeer : request)
    {
        const auto& id = m_localShortPeerInfo.decode(shortPeer.peer);
        NX_ASSERT(!id.isNull());
        newSubscription.values.insert(id, shortPeer.sequence);
    }

    // merge current and new subscription
    QnTranState& oldSubscription = connection->miscData().remoteSubscription;

    auto itrOldSubscription = oldSubscription.values.begin();
    for (auto itr = newSubscription.values.begin(); itr != newSubscription.values.end(); ++itr)
    {
        while (itrOldSubscription != oldSubscription.values.end() && itrOldSubscription.key() < itr.key())
            ++itrOldSubscription;

        if (itrOldSubscription != oldSubscription.values.end() && itrOldSubscription.key() == itr.key())
        {
            itr.value() = std::max(itr.value(), itrOldSubscription.value());
        }
    }
    NX_ASSERT(!connection->remotePeerSubscribedTo(connection->remotePeer()));
    if (connection->miscData().selectingDataInProgress)
    {
        connection->miscData().remoteSubscription = newSubscription;
        return true;
    }

    return selectAndSendTransactions(connection, std::move(newSubscription));
}

bool P2pMessageBus::pushTransactionList(
    const P2pConnectionPtr& connection,
    const QList<QByteArray>& tranList)
{
    auto message = serializeTransactionList(tranList, 1);
    message.data()[0] = (quint8)MessageType::pushTransactionList;
    connection->sendMessage(message);
    return true;
}

bool P2pMessageBus::selectAndSendTransactions(
    const P2pConnectionPtr& connection,
    QnTranState newSubscription)
{

    QList<QByteArray> serializedTransactions;
    bool isFinished = false;
    const ErrorCode errorCode = m_db->transactionLog()->getExactTransactionsAfter(
        &newSubscription,
        connection->remotePeer().peerType == Qn::PeerType::PT_CloudServer,
        serializedTransactions,
        kMaxSelectDataSize,
        &isFinished);
    connection->miscData().selectingDataInProgress = !isFinished;
    if (errorCode != ErrorCode::ok)
    {
        NX_LOG(
            QnLog::P2P_TRAN_LOG,
            lit("P2P message bus failure. Can't select transaction list from database"),
            cl_logWARNING);
        return false;
    }
    connection->miscData().remoteSubscription = newSubscription;


    if (nx::utils::log::isToBeLogged(cl_logDEBUG1, QnLog::P2P_TRAN_LOG))
    {
        for (const auto& serializedTran: serializedTransactions)
        {
            ec2::QnAbstractTransaction transaction;
            QnUbjsonReader<QByteArray> stream(&serializedTran);
            if (QnUbjson::deserialize(&stream, &transaction))
                printTran(connection, transaction, P2pConnection::Direction::outgoing);
        }
    }

#if 1
    if (connection->remotePeer().peerType == Qn::PT_Server &&
        connection->remotePeer().dataFormat == Qn::UbjsonFormat)
    {
        pushTransactionList(connection, serializedTransactions);
        if (!serializedTransactions.isEmpty() && isFinished)
            pushTransactionList(connection, QList<QByteArray>());
        return true;
    }
#endif

    using namespace std::placeholders;
    for (const auto& serializedTran: serializedTransactions)
    {
        if (!handleTransaction(
            this,
            connection->remotePeer().dataFormat,
            serializedTran,
            std::bind(
                SendTransactionToTransportFuction(),
                this, _1, connection),
            std::bind(
                SendTransactionToTransportFastFuction(),
                this, _1, _2, connection)))
        {
            connection->setState(P2pConnection::State::Error);
        }
    }
    return true;
}

void P2pMessageBus::proxyFillerTransaction(const ec2::QnAbstractTransaction& tran)
{
    // proxy filler transaction to avoid gaps in the persistent sequence
    ec2::QnTransaction<ApiUpdateSequenceData> fillerTran(tran);
    fillerTran.command = ApiCommand::updatePersistentSequence;

    auto errCode = m_db->transactionLog()->updateSequence(fillerTran, TranLockType::Lazy);
    switch (errCode)
    {
        case ErrorCode::containsBecauseSequence:
            break;
        case ec2::ErrorCode::ok:
            sendTransaction(fillerTran);
            break;
        default:
            resotreAfterDbError();
            break;
    }
}

void P2pMessageBus::gotTransaction(
    const QnTransaction<ApiUpdateSequenceData> &tran,
    const P2pConnectionPtr& connection)
{
    ApiPersistentIdData peerId(tran.peerID, tran.persistentInfo.dbID);
    //NX_ASSERT(connection->localPeerSubscribedTo(peerId)); //< loop
    NX_ASSERT(!connection->remotePeerSubscribedTo(peerId)); //< loop

    if (nx::utils::log::isToBeLogged(cl_logDEBUG1, QnLog::P2P_TRAN_LOG))
        printTran(connection, tran, P2pConnection::Direction::incoming);

    if (!m_db)
        return;
    auto errCode = m_db->transactionLog()->updateSequence(tran, TranLockType::Lazy);
    switch (errCode)
    {
        case ErrorCode::containsBecauseSequence:
            break;
        case ErrorCode::ok:
            // Proxy transaction to subscribed peers
            m_peers->updateLocalDistance(peerId, tran.persistentInfo.sequence);
            sendTransaction(tran);
            break;
        default:
            connection->setState(P2pConnection::State::Error);
            resotreAfterDbError();
            break;
    }
}

template <class T>
void P2pMessageBus::gotTransaction(
    const QnTransaction<T> &tran,
    const P2pConnectionPtr& connection)
{
    if (nx::utils::log::isToBeLogged(cl_logDEBUG1, QnLog::P2P_TRAN_LOG))
        printTran(connection, tran, P2pConnection::Direction::incoming);

    ApiPersistentIdData peerId(tran.peerID, tran.persistentInfo.dbID);

    //NX_ASSERT(connection->localPeerSubscribedTo(peerId)); //< loop
    NX_ASSERT(!connection->remotePeerSubscribedTo(peerId)); //< loop


    if (m_db)
    {
        if (!tran.persistentInfo.isNull())
        {

            std::unique_ptr<ec2::detail::QnDbManager::QnLazyTransactionLocker> dbTran;
            dbTran.reset(new ec2::detail::QnDbManager::QnLazyTransactionLocker(m_db->getTransaction()));

            QByteArray serializedTran =
                m_ubjsonTranSerializer->serializedTransaction(tran);
            ErrorCode errorCode = dbManager(m_db, connection->getUserAccessData())
                .executeTransactionNoLock(tran, serializedTran);
            switch (errorCode)
            {
            case ErrorCode::ok:
                dbTran->commit();
                m_peers->updateLocalDistance(peerId, tran.persistentInfo.sequence);
                break;
            case ErrorCode::containsBecauseTimestamp:
                dbTran->commit();
                m_peers->updateLocalDistance(peerId, tran.persistentInfo.sequence);
                proxyFillerTransaction(tran);
                return;
            case ErrorCode::containsBecauseSequence:
                dbTran->commit();
                return; // do not proxy if transaction already exists
            default:
                NX_LOG(
                    QnLog::P2P_TRAN_LOG,
                    lit("Can't handle transaction %1: %2. Reopening connection...")
                    .arg(ApiCommand::toString(tran.command))
                    .arg(ec2::toString(errorCode)),
                    cl_logWARNING);
                dbTran.reset(); //< rollback db data
                connection->setState(P2pConnection::State::Error);
                resotreAfterDbError();
                return;
            }
        }

        // Proxy transaction to subscribed peers
        sendTransaction(tran);
    }

    if (m_handler)
        m_handler->triggerNotification(tran, NotificationSource::Remote);
}

void P2pMessageBus::resotreAfterDbError()
{
    // p2pMessageBus uses lazy transactions. It means it does 1 commit per time.
    // In case of DB error transactionLog state is invalid because part of data was rolled back.
    m_db->transactionLog()->init();
    dropConnections();
}

struct GotTransactionFuction
{
    typedef void result_type;

    template<class T>
    void operator()(
        P2pMessageBus *bus,
        const QnTransaction<T> &transaction,
        const P2pConnectionPtr& connection) const
    {
        bus->gotTransaction(transaction, connection);
    }
};


bool P2pMessageBus::handlePushTransactionList(const P2pConnectionPtr& connection, const QByteArray& data)
{
    if (data.isEmpty())
    {
        connection->miscData().remoteSelectingDataInProgress = false;
        return true; //< eof pushTranList reached
    }
    bool success = false;
    auto tranList =  deserializeTransactionList(data, &success);
    if (!success)
        return false;
    for (const auto& transaction: tranList)
    {
        if (!handlePushTransactionData(connection, transaction))
            return false;
    }
    return true;
}

bool P2pMessageBus::handlePushTransactionData(const P2pConnectionPtr& connection, const QByteArray& serializedTran)
{
    using namespace std::placeholders;
    return handleTransaction(
        this,
        connection->remotePeer().dataFormat,
        std::move(serializedTran),
        std::bind(GotTransactionFuction(), this, _1, connection),
        [](Qn::SerializationFormat, const QByteArray&) { return false; });
}

bool P2pMessageBus::isSubscribedTo(const ApiPersistentIdData& peer) const
{
    QnMutexLocker lock(&m_mutex);
    if (ApiPersistentIdData(localPeer()) == peer)
        return true;
    for (const auto& connection: m_connections)
    {
        if (connection->state() != P2pConnection::State::Connected)
            continue;
        if (connection->localPeerSubscribedTo(peer))
            return true;
    }
    return false;
}

qint32 P2pMessageBus::distanceTo(const ApiPersistentIdData& peer) const
{
    QnMutexLocker lock(&m_mutex);
    if (ApiPersistentIdData(localPeer()) == peer)
        return 0;
    return m_peers->distanceTo(peer);
}

QMap<QnUuid, P2pConnectionPtr> P2pMessageBus::connections() const
{
    QnMutexLocker lock(&m_mutex);
    return m_connections;
}

int P2pMessageBus::connectionTries() const
{
    QnMutexLocker lock(&m_mutex);
    return m_connectionTries;
}

QMap<QnUuid, ApiPeerData> P2pMessageBus::aliveServerPeers() const
{
    // todo: implement me
    return QMap<QnUuid, ApiPeerData>();
}

QMap<QnUuid, ApiPeerData> P2pMessageBus::aliveClientPeers(int maxDistance) const
{
    // todo: implement me
    return QMap<QnUuid, ApiPeerData>();
}

QnUuid P2pMessageBus::routeToPeerVia(const QnUuid& dstPeer, int* distance) const
{
    // todo: implement me
    return QnUuid();
}

int P2pMessageBus::distanceToPeer(const QnUuid& dstPeer) const
{
    // todo: implement me
    return 0;
}

} // namespace p2p
} // namespace nx
