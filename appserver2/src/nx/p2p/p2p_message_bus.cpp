#include "p2p_message_bus.h"

#include <nx/utils/std/cmath.h>
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
#include <api/runtime_info_manager.h>
#include <managers/time_manager.h>

namespace {
    int commitIntervalMs = 1000;
    static const int kMaxSelectDataSize = 1024 * 32;
} // namespace

namespace nx {
namespace p2p {

using namespace ec2;

struct GotTransactionFuction
{
    typedef void result_type;

    template<class T>
    void operator()(
        MessageBus *bus,
        const QnTransaction<T>& transaction,
        const P2pConnectionPtr& connection,
        const TransportHeader& transportHeader) const
    {
        bus->gotTransaction(transaction, connection, transportHeader);
    }
};

struct GotUnicastTransactionFuction
{
    typedef void result_type;

    template<class T>
    void operator()(
        MessageBus *bus,
        const QnTransaction<T>& transaction,
        const P2pConnectionPtr& connection,
        const TransportHeader& transportHeader) const
    {
        bus->gotUnicastTransaction(transaction, connection, transportHeader);
    }
};

// ---------------------- P2pMessageBus --------------

MessageBus::MessageBus(
    ec2::detail::QnDbManager* db,
    Qn::PeerType peerType,
    QnCommonModule* commonModule,
    QnJsonTransactionSerializer* jsonTranSerializer,
    QnUbjsonTransactionSerializer* ubjsonTranSerializer)
:
    TransactionMessageBusBase(db, peerType, commonModule, jsonTranSerializer, ubjsonTranSerializer),
    m_miscData(this)
{
    qRegisterMetaType<MessageType>();
    qRegisterMetaType<ConnectionBase::State>("ConnectionBase::State");
    qRegisterMetaType<P2pConnectionPtr>("P2pConnectionPtr");
    qRegisterMetaType<QWeakPointer<ConnectionBase>>("QWeakPointer<ConnectionBase>");

    m_thread->setObjectName("P2pMessageBus");
    connect(m_thread, &QThread::started,
        [this, peerType]()
        {
            if (!m_timer)
            {
                m_timer = new QTimer(this);
                if (ApiPeerData::isServer(peerType))
                    connect(m_timer, &QTimer::timeout, this, &MessageBus::doPeriodicTasksForServer);
                else
                    connect(m_timer, &QTimer::timeout, this, &MessageBus::doPeriodicTasksForClient);
            }
            m_timer->start(500);
        });
    connect(m_thread, &QThread::finished, [this]() { m_timer->stop(); });
    m_dbCommitTimer.restart();
}

MessageBus::~MessageBus()
{
    stop();
}

void MessageBus::dropConnections()
{
    QnMutexLocker lock(&m_mutex);
    m_connections.clear();
    m_outgoingConnections.clear();
    m_remoteUrls.clear();
    if (m_peers)
    {
        m_peers->clear();
        emitPeerFoundLostSignals();
    }
}


void MessageBus::printTran(
    const P2pConnectionPtr& connection,
    const ec2::QnAbstractTransaction& tran,
    Connection::Direction direction) const
{

    auto localPeerName = qnStaticCommon->moduleDisplayName(commonModule()->moduleGUID());
    auto remotePeerName = qnStaticCommon->moduleDisplayName(connection->remotePeer().id);
    QString msgName;
    QString directionName;
    if (direction == Connection::Direction::outgoing)
    {
        msgName = lm("Send");
        directionName = lm("--->");
    }
    else
    {
        msgName = lm("Got");
        directionName = lm("<---");
    }

    NX_DEBUG(
        this,
        lit("%1 tran:\t %2 %3 %4. Command: %5. Seq=%6. Created by %7")
        .arg(msgName)
        .arg(localPeerName)
        .arg(directionName)
        .arg(remotePeerName)
        .arg(toString(tran.command))
        .arg(tran.persistentInfo.sequence)
        .arg(qnStaticCommon->moduleDisplayName(tran.peerID)));
}

void MessageBus::start()
{
    m_localShortPeerInfo.encode(localPeer(), 0);
    m_peers.reset(new BidirectionRoutingInfo(localPeer()));
    addOfflinePeersFromDb();
    m_lastRuntimeInfo[localPeer()] = commonModule()->runtimeInfoManager()->localInfo().data;
    base_type::start();
}

void MessageBus::stop()

{
    {
        QnMutexLocker lock(&m_mutex);
        if (m_db)
        {
            ec2::detail::QnDbManager::QnDbTransactionLocker dbTran(m_db->getTransaction());
            dbTran.commit();
        }
        m_remoteUrls.clear();
    }

    dropConnections();
    base_type::stop();
}

// P2pMessageBus

void MessageBus::addOutgoingConnectionToPeer(const QnUuid& peer, const QUrl& _url)
{
    QnMutexLocker lock(&m_mutex);
    deleteRemoveUrlById(peer);

    QUrl url(_url);
    if (peer == kCloudPeerId)
        url.setPath(ConnectionProcessor::kCloudPathPrefix + ConnectionProcessor::kUrlPath);
    else
        url.setPath(ConnectionProcessor::kUrlPath);

    int pos = nx::utils::random::number((int) 0, (int) m_remoteUrls.size());
    m_remoteUrls.insert(m_remoteUrls.begin() + pos, RemoteConnection(peer, url));
}

void MessageBus::deleteRemoveUrlById(const QnUuid& id)
{
    for (int i = 0; i < m_remoteUrls.size(); ++i)
    {
        if (m_remoteUrls[i].peerId == id)
        {
            m_remoteUrls.erase(m_remoteUrls.begin() + i);
            break;
        }
    }
}

void MessageBus::removeOutgoingConnectionFromPeer(const QnUuid& id)
{
    QnMutexLocker lock(&m_mutex);
    deleteRemoveUrlById(id);

    m_outgoingConnections.remove(id);
    auto itr = m_connections.find(id);
    if (itr != m_connections.end() && itr.value()->direction() == Connection::Direction::outgoing)
        itr.value()->setState(Connection::State::Error);
}

void MessageBus::gotConnectionFromRemotePeer(
    const ec2::ApiPeerDataEx& remotePeer,
    ec2::ConnectionLockGuard connectionLockGuard,
    nx::network::WebSocketPtr webSocket,
    const Qn::UserAccessData& userAccessData,
    std::function<void()> onConnectionClosedCallback)
{
    P2pConnectionPtr connection(new Connection(
        commonModule(),
        remotePeer,
        localPeerEx(),
        std::move(webSocket),
        userAccessData,
        std::make_unique<nx::p2p::ConnectionContext>(),
        std::move(connectionLockGuard)));

    QnMutexLocker lock(&m_mutex);
    const auto remoteId = connection->remotePeer().id;
    m_peers->removePeer(connection->remotePeer());
    m_connections[remoteId] = connection;
    emitPeerFoundLostSignals();
    connectSignals(connection);
    startReading(connection);

    if (remotePeer.isClient())
    {
        // Clients use simplified logic. They are started and subscribed to all updates immediately.
        context(connection)->isLocalStarted = true;
        m_peers->addRecord(remotePeer, remotePeer, nx::p2p::RoutingRecord(1));
        sendInitialDataToClient(connection);
    }
    context(connection)->onConnectionClosedCallback = onConnectionClosedCallback;
}

void MessageBus::sendInitialDataToClient(const P2pConnectionPtr& connection)
{
    sendRuntimeData(connection, m_lastRuntimeInfo.keys());

    {
        QnTransaction<ApiFullInfoData> tran(commonModule()->moduleGUID());
        tran.command = ApiCommand::getFullInfo;
        if (!readApiFullInfoData(connection.staticCast<Connection>()->userAccessData(),
            connection->remotePeer(), &tran.params))
        {
            connection->setState(Connection::State::Error);
            return;
        }
        sendTransactionImpl(connection, tran, TransportHeader());
    }
}

void MessageBus::sendInitialDataToCloud(const P2pConnectionPtr& connection)
{
    if (!m_db)
        return;
    auto data = serializeSubscribeAllRequest(m_db->transactionLog()->getTransactionsState());
    data.data()[0] = (quint8) MessageType::subscribeAll;
    connection->sendMessage(data);
    context(connection)->isLocalStarted = true;
}

void MessageBus::connectSignals(const P2pConnectionPtr& connection)
{
    QnMutexLocker lock(&m_mutex);
    connect(connection.data(), &Connection::stateChanged, this, &MessageBus::at_stateChanged);
    connect(connection.data(), &Connection::gotMessage, this, &MessageBus::at_gotMessage);
    connect(connection.data(), &Connection::allDataSent, this, &MessageBus::at_allDataSent);
}

void MessageBus::createOutgoingConnections(
    const QMap<ApiPersistentIdData, P2pConnectionPtr>& currentSubscription)
{
    if (hasStartingConnections())
        return;
    int intervalMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        m_intervals.outConnectionsInterval).count();
    if (m_outConnectionsTimer.isValid() && !m_outConnectionsTimer.hasExpired(intervalMs))
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
        if (!m_connections.contains(remoteConnection.peerId) &&
            !m_outgoingConnections.contains(remoteConnection.peerId))
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

            P2pConnectionPtr connection(new Connection(
                commonModule(),
                remoteConnection.peerId,
                localPeerEx(),
                remoteConnection.url,
                std::make_unique<ConnectionContext>(),
                std::move(connectionLockGuard)));
            m_outgoingConnections.insert(remoteConnection.peerId, connection);
            ++m_connectionTries;
            connectSignals(connection);
            connection->startConnection();
        }
    }
}

void MessageBus::addOfflinePeersFromDb()
{
    if (!m_db)
        return;
    const auto localPeer = this->localPeer();

    auto persistentState = m_db->transactionLog()->getTransactionsState().values;
    for (auto itr = persistentState.cbegin(); itr != persistentState.cend(); ++itr)
    {
        const auto& peer = itr.key();
        if (peer == localPeer)
            continue;

        const qint32 sequence = itr.value();
        const qint32 localOfflineDistance = kMaxDistance - sequence;
        nx::p2p::RoutingRecord record(localOfflineDistance);
        m_peers->addRecord(localPeer, peer, record);
    }
}

void MessageBus::printPeersMessage()
{
    QList<QString> records;

    for (auto itr = m_peers->allPeerDistances.constBegin(); itr != m_peers->allPeerDistances.constEnd(); ++itr)
    {
        const auto& peer = itr.value();

        qint32 minDistance = peer.minDistance();
        if (minDistance == kMaxDistance)
            continue;
        m_localShortPeerInfo.encode(itr.key());

        records << lit("\t\t\t\t\t To:  %1(dbId=%2). Distance: %3")
            .arg(qnStaticCommon->moduleDisplayName(itr.key().id))
            .arg(itr.key().persistentId.toString())
            .arg(minDistance);
    }

    NX_DEBUG(
        this,
        lit("Peer %1 records:\n%3")
        .arg(qnStaticCommon->moduleDisplayName(localPeer().id))
        .arg(records.join("\n")));
}

void MessageBus::printSubscribeMessage(
    const QnUuid& remoteId,
    const QVector<ApiPersistentIdData>& subscribedTo,
    const QVector<qint32>& sequences) const
{
    QList<QString> records;
    int index = 0;
    for (const auto& peer: subscribedTo)
    {
        records << lit("\t\t\t\t\t To:  %1(dbId=%2) from: %3")
            .arg(qnStaticCommon->moduleDisplayName(peer.id))
            .arg(peer.persistentId.toString())
            .arg(sequences[index++]);
    }

    NX_DEBUG(
        this,
        lit("Subscribe:\t %1 ---> %2:\n%3")
        .arg(qnStaticCommon->moduleDisplayName(localPeer().id))
        .arg(qnStaticCommon->moduleDisplayName(remoteId))
        .arg(records.join("\n")));
}

bool MessageBus::isLocalConnection(const ApiPersistentIdData& peer) const
{
    auto connection = m_connections.value(peer.id);
    if (!connection)
        return false;
    auto peerType = connection->remotePeer().peerType;
    return ec2::ApiPeerData::isClient(peerType) && peerType != Qn::PT_VideowallClient;
}

void MessageBus::sendAlivePeersMessage(const P2pConnectionPtr& connection)
{
    auto serializeMessage = [this](const P2pConnectionPtr& connection)
    {
        QVector<PeerDistanceRecord> records;
        records.reserve(m_peers->allPeerDistances.size());
        const auto localPeer = ApiPersistentIdData(this->localPeer());
        for (auto itr = m_peers->allPeerDistances.cbegin(); itr != m_peers->allPeerDistances.cend(); ++itr)
        {
            if (isLocalConnection(itr.key()))
                continue; //< don't show this connection to other servers

            // Don't send longer route than this peer already has.
            // Otherwise it could generate route loop.
            qint32 minDistance = itr->minDistance();

            if (itr->distanceVia(connection->remotePeer()) == minDistance)
                continue;

            // Don't broadcast foreign offline distances
            if (minDistance < kMaxDistance && minDistance > kMaxOnlineDistance)
                minDistance = itr->distanceVia(localPeer);

            if (minDistance == kMaxDistance)
                continue;
            const qint16 peerNumber = m_localShortPeerInfo.encode(itr.key());
            records.push_back(PeerDistanceRecord(peerNumber, minDistance));
        }
        NX_ASSERT(!records.isEmpty());
        QByteArray data = serializePeersMessage(records, 1);
        data.data()[0] = (quint8)MessageType::alivePeers;
        return data;
    };

    auto sendAlivePeersMessage = [this](const P2pConnectionPtr& connection, const QByteArray& data)
    {
        int peersIntervalMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            m_intervals.sendPeersInfoInterval).count();
        auto connectionContext = context(connection);
        if (connectionContext->localPeersTimer.isValid() && !connectionContext->localPeersTimer.hasExpired(peersIntervalMs))
            return;
        if (data != connectionContext->localPeersMessage)
        {
            connectionContext->localPeersMessage = data;
            connectionContext->localPeersTimer.restart();
            if (nx::utils::log::isToBeLogged(cl_logDEBUG1, this))
                printPeersMessage();
            connection->sendMessage(data);
        }
    };

    if (connection)
    {
        sendAlivePeersMessage(connection, serializeMessage(connection));
        return;
    }

    for (const auto& connection: m_connections)
    {
        if (connection->state() != Connection::State::Connected)
            continue;
        if (!context(connection)->isRemoteStarted)
            continue;
        sendAlivePeersMessage(connection, serializeMessage(connection));
    }
}

QMap<ApiPersistentIdData, P2pConnectionPtr> MessageBus::getCurrentSubscription() const
{
    QMap<ApiPersistentIdData, P2pConnectionPtr> result;
    for (auto itr = m_connections.cbegin(); itr != m_connections.cend(); ++itr)
    {
        auto context = this->context(itr.value());
        for (const auto peer: context->localSubscription)
            result[peer] = itr.value();
    }
    return result;
}

void MessageBus::resubscribePeers(
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
        auto connectionContext = context(connection);
        if (connectionContext->localSubscription != newValue)
        {
            connectionContext->localSubscription = newValue;
            QVector<PeerNumberType> shortValues;
            for (const auto& id: newValue)
                shortValues.push_back(connectionContext->encode(id));

            NX_ASSERT(newValue.contains(connection->remotePeer()));
            auto sequences = m_db->transactionLog()->getTransactionsState(newValue);

            if (nx::utils::log::isToBeLogged(cl_logDEBUG1, this))
                printSubscribeMessage(connection->remotePeer().id, newValue, sequences);

            QVector<SubscribeRecord> request;
            request.reserve(shortValues.size());
            for (int i = 0; i < shortValues.size(); ++i)
                request.push_back(SubscribeRecord(shortValues[i], sequences[i]));
            auto serializedData = serializeSubscribeRequest(request);
            serializedData.data()[0] = (quint8) (MessageType::subscribeForDataUpdates);
            connection->sendMessage(serializedData);

            connectionContext->recvDataInProgress = true;
        }
    }
}

P2pConnectionPtr MessageBus::findConnectionById(const ApiPersistentIdData& id) const
{
    P2pConnectionPtr result =  m_connections.value(id.id);
    return result && result->remotePeer().persistentId == id.persistentId ? result : P2pConnectionPtr();
}

bool MessageBus::needSubscribeDelay()
{
    // If alive peers has been recently received, postpone subscription for some time.
    // This peer could be found via another neighbor.
    if (m_lastPeerInfoTimer.isValid())
    {
        std::chrono::milliseconds lastPeerInfoElapsed(m_lastPeerInfoTimer.elapsed());

        if (lastPeerInfoElapsed < m_intervals.subscribeIntervalLow)
        {
            if (!m_wantToSubscribeTimer.isValid())
                m_wantToSubscribeTimer.restart();
            std::chrono::milliseconds wantToSubscribeElapsed(m_wantToSubscribeTimer.elapsed());
            if (wantToSubscribeElapsed < m_intervals.subscribeIntervalHigh)
                return true;
        }
    }
    m_wantToSubscribeTimer.invalidate();
    return false;
}

bool MessageBus::needStartConnection(
    const ApiPersistentIdData& peer,
    const QMap<ApiPersistentIdData, P2pConnectionPtr>& currentSubscription) const
{
    const RouteToPeerMap& allPeerDistances = m_peers->allPeerDistances;
    qint32 currentDistance = allPeerDistances.value(peer).minDistance();
    const auto& subscribedVia = currentSubscription.value(peer);
    return currentDistance > m_miscData.maxDistanceToUseProxy
        || (subscribedVia && context(subscribedVia)->localSubscription.size() > m_miscData.maxSubscriptionToResubscribe);
}

bool MessageBus::needStartConnection(
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

bool MessageBus::hasStartingConnections() const
{
    for (const auto& connection: m_connections)
    {
        if (connection->remotePeer().isServer())
        {
            auto data = context(connection);
            if (data->isLocalStarted && data->remotePeersMessage.isEmpty())
                return true;
        }
    }
    return false;
}

void MessageBus::startStopConnections(const QMap<ApiPersistentIdData, P2pConnectionPtr>& currentSubscription)
{
    if (hasStartingConnections())
        return;

    // start using connection if need
    int maxStartsAtOnce = m_miscData.newConnectionsAtOnce;

    for (auto& connection: m_connections)
    {
        auto context = this->context(connection);
        if (connection->state() != Connection::State::Connected || context->isLocalStarted)
            continue; //< already in use or not ready yet

        ApiPersistentIdData peer = connection->remotePeer();
        if (needStartConnection(peer, currentSubscription))
        {
            context->isLocalStarted = true;
            connection->sendMessage(MessageType::start, QByteArray());
            if (--maxStartsAtOnce == 0)
                return; //< limit start requests at once
        }
    }
}

P2pConnectionPtr MessageBus::findBestConnectionToSubscribe(
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
        int subscriptions = context(connection)->localSubscription.size() + newSubscriptions.value(connection);
        if (subscriptions < minSubscriptions)
        {
            minSubscriptions = subscriptions;
            result = connection;
        }
    }

    return result;
}

void MessageBus::doSubscribe(const QMap<ApiPersistentIdData, P2pConnectionPtr>& currentSubscription)
{
    if (hasStartingConnections())
        return;

    for (const auto& connection: m_connections)
    {
        const auto& data = context(connection);
        if (data->recvDataInProgress)
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
                [this, &peer, &localPeer](const ApiPersistentIdData& via)
            {
                if (via == localPeer)
                    return true; //< 'subscribedVia' has lost 'peer'. Update subscription later as soon as new minDistance will be discovered.
                auto connection = findConnectionById(via);
                NX_ASSERT(connection);
                if (!connection)
                    return true; //< It shouldn't be.
                return context(connection)->isRemotePeerSubscribedTo(peer);
            }))
            {
                continue;
            }

            auto connection = findBestConnectionToSubscribe(viaList, tmpNewSubscription);
            if (subscribedVia)
            {
                NX_DEBUG(
                    this,
                    lit("Peer %1 is changing subscription to %2. subscribed via %3. new subscribe via %4")
                    .arg(qnStaticCommon->moduleDisplayName(localPeer.id))
                    .arg(qnStaticCommon->moduleDisplayName(peer.id))
                    .arg(qnStaticCommon->moduleDisplayName(subscribedVia->remotePeer().id))
                    .arg(qnStaticCommon->moduleDisplayName(connection->remotePeer().id)));
            }
            newSubscription[peer] = connection;
            ++tmpNewSubscription[connection];
            isUpdated = true;
        }
    }
    if (isUpdated)
        resubscribePeers(newSubscription);
}

void MessageBus::commitLazyData()
{
    if (m_dbCommitTimer.hasExpired(commitIntervalMs))
    {
        ec2::detail::QnDbManager::QnDbTransactionLocker dbTran(m_db->getTransaction());
        dbTran.commit();
        m_dbCommitTimer.restart();
    }
}

void MessageBus::MiscData::update()
{
    expectedConnections = std::max(1, std::max(owner->m_connections.size(), (int) owner->m_remoteUrls.size()));
    maxSubscriptionToResubscribe = std::round(std::sqrt(expectedConnections)) * 2;
    maxDistanceToUseProxy = std::max(2, int(std::sqrt(std::sqrt(expectedConnections))));
    newConnectionsAtOnce = std::max(1, int(std::round(std::sqrt(expectedConnections))) / 2);
}

void MessageBus::doPeriodicTasksForServer()
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

void MessageBus::doPeriodicTasksForClient()
{
    QnMutexLocker lock(&m_mutex);
    createOutgoingConnections(getCurrentSubscription()); //< Open new connections.
}

ApiPeerData MessageBus::localPeer() const
{
    return ApiPeerData(
        commonModule()->moduleGUID(),
        commonModule()->runningInstanceGUID(),
        commonModule()->dbId(),
        m_localPeerType);
}

ApiPeerDataEx MessageBus::localPeerEx() const
{
    ApiPeerDataEx result;

    result.id = commonModule()->moduleGUID();
    result.persistentId = commonModule()->dbId();
    result.instanceId = commonModule()->runningInstanceGUID();
    result.systemId = commonModule()->globalSettings()->localSystemId();
    result.peerType = m_localPeerType;
    result.cloudHost = nx::network::AppInfo::defaultCloudHost();
    result.identityTime = commonModule()->systemIdentityTime();
    result.aliveUpdateIntervalMs = std::chrono::duration_cast<std::chrono::milliseconds>
        (commonModule()->globalSettings()->aliveUpdateInterval()).count();
    result.protoVersion = nx_ec::EC2_PROTO_VERSION;
    result.dataFormat = Qn::UbjsonFormat;
    return result;
}

void MessageBus::startReading(P2pConnectionPtr connection)
{
    context(connection)->encode(ApiPersistentIdData(connection->remotePeer()), 0);
    connection->startReading();
}

void MessageBus::at_stateChanged(
    QWeakPointer<ConnectionBase> weakRef,
    Connection::State /*state*/)
{
    P2pConnectionPtr connection = weakRef.toStrongRef();
    if (!connection)
        return;

    QnMutexLocker lock(&m_mutex);

    const auto& remoteId = connection->remotePeer().id;
    switch (connection->state())
    {
        case Connection::State::Connected:
            if (connection->direction() == Connection::Direction::outgoing)
            {
                m_connections[remoteId] = connection;
                m_outgoingConnections.remove(remoteId);
                emitPeerFoundLostSignals();
                startReading(connection);
            }
            emit newDirectConnectionEstablished(connection.data());
            if (connection->remotePeer().peerType == Qn::PT_CloudServer)
                sendInitialDataToCloud(connection);

            break;
        case Connection::State::Unauthorized:
        case Connection::State::Error:
        {
            NX_DEBUG(
                this,
                lit("Peer %1 has closed connection to %2")
                .arg(qnStaticCommon->moduleDisplayName(localPeer().id))
                .arg(qnStaticCommon->moduleDisplayName(connection->remotePeer().id)));

            if (auto callback = context(connection)->onConnectionClosedCallback)
                callback();
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
            emitPeerFoundLostSignals();
            if (connection->state() == Connection::State::Unauthorized)
                emit remotePeerUnauthorized(connection->remotePeer().id);
            break;
        }
        default:
            break;
    }
}

void MessageBus::at_allDataSent(QWeakPointer<ConnectionBase> weakRef)
{
    P2pConnectionPtr connection = weakRef.toStrongRef();
    if (!connection)
        return;

    QnMutexLocker lock(&m_mutex);
    if (m_connections.value(connection->remotePeer().id) != connection)
        return;
    if (context(connection)->sendDataInProgress)
        selectAndSendTransactions(
            connection,
            context(connection)->remoteSubscription,
            context(connection)->remoteAddImplicitData);
}

void MessageBus::at_gotMessage(
    QWeakPointer<ConnectionBase> weakRef,
    MessageType messageType,
    const nx::Buffer& payload)
{
    P2pConnectionPtr connection = weakRef.toStrongRef();
    if (!connection)
        return;

    QnMutexLocker lock(&m_mutex);
    if (m_connections.value(connection->remotePeer().id) != connection)
        return;


    if (connection->state() == Connection::State::Error)
        return; //< Connection has been closed
    if (nx::utils::log::isToBeLogged(cl_logDEBUG1, this) &&
        messageType != MessageType::pushTransactionData &&
        messageType != MessageType::pushTransactionList)
    {
        auto localPeerName = qnStaticCommon->moduleDisplayName(commonModule()->moduleGUID());
        auto remotePeerName = qnStaticCommon->moduleDisplayName(connection->remotePeer().id);

        NX_DEBUG(
            this,
            lit("Got message:\t %1 <--- %2. Type: %3. Size=%4")
            .arg(localPeerName)
            .arg(remotePeerName)
            .arg(toString(messageType))
            .arg(payload.size() + 1));
    }


    bool result = false;
    auto connectionContext = this->context(connection);
    switch (messageType)
    {
    case MessageType::start:
        connectionContext->isRemoteStarted = true;
        result = true;
        break;
    case MessageType::stop:
        connectionContext->sendDataInProgress = false;
        connectionContext->isRemoteStarted = false;
        connectionContext->remoteSubscription.values.clear();
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
    case MessageType::subscribeAll:
        result = handleSubscribeForAllDataUpdates(connection, payload);
        break;
    case MessageType::pushTransactionData:
        result = handlePushTransactionData(connection, payload, TransportHeader());
        break;
    case MessageType::pushTransactionList:
        result = handlePushTransactionList(connection, payload);
        break;
    case MessageType::pushImpersistentUnicastTransaction:
        result = handleTransactionWithHeader(
            connection,
            payload,
            GotUnicastTransactionFuction());
        break;
    case MessageType::pushImpersistentBroadcastTransaction:
        result = handleTransactionWithHeader(
            connection,
            payload,
            GotTransactionFuction());
        break;
    default:
        NX_ASSERT(0, lm("Unknown message type").arg((int)messageType));
        break;
    }
    if (!result)
        connection->setState(Connection::State::Error);
}

bool MessageBus::handleResolvePeerNumberRequest(const P2pConnectionPtr& connection, const QByteArray& data)
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

bool MessageBus::handleResolvePeerNumberResponse(const P2pConnectionPtr& connection, const QByteArray& data)
{
    bool success = false;
    auto records = deserializeResolvePeerNumberResponse(data, &success);
    if (!success)
        return false;

    auto connectionContext = context(connection);
    for (const auto& record: records)
    {
        connectionContext->encode(record, record.peerNumber);

        auto itr = std::find(
            connectionContext->awaitingNumbersToResolve.begin(),
            connectionContext->awaitingNumbersToResolve.end(),
            record.peerNumber);
        if (itr != connectionContext->awaitingNumbersToResolve.end())
            connectionContext->awaitingNumbersToResolve.erase(itr);
    }

    const QByteArray msg = context(connection)->remotePeersMessage;
    if (!msg.isEmpty())
        handlePeersMessage(connection, msg);
    return true;
}

bool MessageBus::handlePeersMessage(const P2pConnectionPtr& connection, const QByteArray& data)
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
        if (context(connection)->decode(peer.peerNumber).isNull())
            numbersToResolve.push_back(peer.peerNumber);
    }
    context(connection)->remotePeersMessage = data;

    if (numbersToResolve.empty())
    {
        m_peers->removePeer(connection->remotePeer());
        auto& shortPeers = context(connection)->shortPeerInfo;
        for (const auto& peer: peers)
        {
            int distance = peer.distance;
            if (distance < kMaxOnlineDistance)
            {
                ++distance;
                NX_ASSERT(distance != kMaxOnlineDistance);
            }
            m_peers->addRecord(
                connection->remotePeer(),
                shortPeers.decode(peer.peerNumber),
                nx::p2p::RoutingRecord(distance));
        }
        emitPeerFoundLostSignals();
        return true;
    }

    auto connectionContext = context(connection);
    std::sort(numbersToResolve.begin(), numbersToResolve.end());
    QVector<PeerNumberType> moreNumbersToResolve;

    std::set_difference(
        numbersToResolve.begin(),
        numbersToResolve.end(),
        connectionContext->awaitingNumbersToResolve.begin(),
        connectionContext->awaitingNumbersToResolve.end(),
        std::inserter(moreNumbersToResolve, moreNumbersToResolve.begin()));

    if (moreNumbersToResolve.isEmpty())
        return true;

    for (const auto& number: moreNumbersToResolve)
        connectionContext->awaitingNumbersToResolve.push_back(number);
    std::sort(connectionContext->awaitingNumbersToResolve.begin(), connectionContext->awaitingNumbersToResolve.end());

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
        MessageBus* bus,
        const QnTransaction<T>& transaction,
        const P2pConnectionPtr& connection) const
    {
        ApiPersistentIdData tranId(transaction.peerID, transaction.persistentInfo.dbID);
        NX_ASSERT(bus->context(connection)->isRemotePeerSubscribedTo(tranId));
        NX_ASSERT(!(ApiPersistentIdData(connection->remotePeer()) == tranId, "Loop detected"));

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
                qWarning() << "Client has requested data in an unsupported format"
                    << connection->remotePeer().dataFormat;
                break;
        }
    }
};

struct SendTransactionToTransportFastFuction
{
    bool operator()(
        MessageBus* /*bus*/,
        Qn::SerializationFormat /* srcFormat */,
        const QByteArray& serializedTran,
        const P2pConnectionPtr& connection) const
    {
        if (connection.staticCast<Connection>()->userAccessData().userId != Qn::kSystemAccess.userId)
        {
            NX_VERBOSE(
                this,
                lit("Permission check failed while sending SERIALIZED transaction to peer %1")
                .arg(connection->remotePeer().id.toString()));
            return false;
        }

        connection->sendMessage(MessageType::pushTransactionData, serializedTran);
        return true;
    }
};

void MessageBus::sendRuntimeData(
    const P2pConnectionPtr& connection,
    const QList<ApiPersistentIdData>& peers)
{
    for (const auto& peer: peers)
    {
        auto runtimeInfoItr = m_lastRuntimeInfo.find(peer);
        if (runtimeInfoItr != m_lastRuntimeInfo.end())
        {
            QnTransaction<ApiRuntimeData> tran(ApiCommand::runtimeInfoChanged, peer.id);
            tran.params = runtimeInfoItr.value();
            sendTransactionImpl(connection, tran, TransportHeader());
        }
    }
}

bool MessageBus::handleSubscribeForDataUpdates(const P2pConnectionPtr& connection, const QByteArray& data)
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
    context(connection)->remoteAddImplicitData = false;

    // merge current and new subscription
    QnTranState& oldSubscription = context(connection)->remoteSubscription;
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


    NX_ASSERT(!context(connection)->isRemotePeerSubscribedTo(connection->remotePeer()));
    if (context(connection)->sendDataInProgress)
    {
        context(connection)->remoteSubscription = newSubscription;
    }
    else
    {
        if (!selectAndSendTransactions(connection, std::move(newSubscription), false))
            return false;
    }
    sendRuntimeData(connection, context(connection)->remoteSubscription.values.keys());
    return true;
}

bool MessageBus::handleSubscribeForAllDataUpdates(
    const P2pConnectionPtr& connection,
    const QByteArray& data)
{
    NX_ASSERT(connection->remotePeer().peerType == Qn::PT_CloudServer);
    context(connection)->remoteAddImplicitData = true;
    bool success = false;
    QnTranState newSubscription = deserializeSubscribeAllRequest(data, &success);

    if (context(connection)->sendDataInProgress)
    {
        context(connection)->remoteSubscription = newSubscription;
    }
    else
    {
        if (!selectAndSendTransactions(connection, std::move(newSubscription), true))
            return false;
    }
    return true;
}

bool MessageBus::pushTransactionList(
    const P2pConnectionPtr& connection,
    const QList<QByteArray>& tranList)
{
    auto message = serializeTransactionList(tranList, 1);
    message.data()[0] = (quint8)MessageType::pushTransactionList;
    connection->sendMessage(message);
    return true;
}

bool MessageBus::selectAndSendTransactions(
    const P2pConnectionPtr& connection,
    QnTranState newSubscription,
    bool addImplicitData)
{

    QList<QByteArray> serializedTransactions;
    bool isFinished = true;
    if (addImplicitData)
    {
        // Add all implicit records from 0 sequence.
        for (const auto& key: m_db->transactionLog()->getTransactionsState().values.keys())
        {
            if (!newSubscription.values.contains(key))
                newSubscription.values.insert(key, 0);
        }
    }

    if (m_db)
    {
        const bool isCloudServer = connection->remotePeer().peerType == Qn::PeerType::PT_CloudServer;
        const ErrorCode errorCode = m_db->transactionLog()->getExactTransactionsAfter(
            &newSubscription,
            isCloudServer,
            serializedTransactions,
            kMaxSelectDataSize,
            &isFinished);
        if (errorCode != ErrorCode::ok)
        {
            NX_WARNING(
                this,
                lit("P2P message bus failure. Can't select transaction list from database"));
            return false;
        }
    }
    context(connection)->sendDataInProgress = !isFinished;
    context(connection)->remoteSubscription = newSubscription;

    if (nx::utils::log::isToBeLogged(cl_logDEBUG1, this))
    {
        for (const auto& serializedTran: serializedTransactions)
        {
            ec2::QnAbstractTransaction transaction;
            QnUbjsonReader<QByteArray> stream(&serializedTran);
            if (QnUbjson::deserialize(&stream, &transaction))
                printTran(connection, transaction, Connection::Direction::outgoing);
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
            connection->setState(Connection::State::Error);
        }
    }
    return true;
}

void MessageBus::proxyFillerTransaction(
    const ec2::QnAbstractTransaction& tran,
    const TransportHeader& transportHeader)
{
    // proxy filler transaction to avoid gaps in the persistent sequence
    ec2::QnTransaction<ApiUpdateSequenceData> fillerTran(tran);
    fillerTran.command = ApiCommand::updatePersistentSequence;

    auto errCode = m_db->transactionLog()->updateSequence(fillerTran, TransactionLockType::Lazy);
    switch (errCode)
    {
        case ErrorCode::containsBecauseSequence:
            break;
        case ec2::ErrorCode::ok:
            sendTransaction(fillerTran, transportHeader);
            break;
        default:
            resotreAfterDbError();
            break;
    }
}

void MessageBus::updateOfflineDistance(
    const P2pConnectionPtr& connection,
    const ApiPersistentIdData& to,
    int sequence)
{
    const qint32 offlineDistance = kMaxDistance - sequence;

    const auto updateDistance =
        [&](const ApiPeerData& via)
        {
            const qint32 toDistance = m_peers->alivePeers[via].distanceTo(to);
            if (offlineDistance < toDistance)
            {
                nx::p2p::RoutingRecord record(offlineDistance);
                m_peers->addRecord(via, to, record);
            }
        };
    updateDistance(connection->remotePeer());
    updateDistance(localPeer());
}

void MessageBus::gotTransaction(
    const QnTransaction<ApiUpdateSequenceData> &tran,
    const P2pConnectionPtr& connection,
    const TransportHeader& transportHeader)
{
    ApiPersistentIdData peerId(tran.peerID, tran.persistentInfo.dbID);
    //NX_ASSERT(connection->localPeerSubscribedTo(peerId)); //< loop
    //NX_ASSERT(!context(connection)->isRemotePeerSubscribedTo(peerId)); //< loop

    if (nx::utils::log::isToBeLogged(cl_logDEBUG1, this))
        printTran(connection, tran, Connection::Direction::incoming);

    updateOfflineDistance(connection, peerId, tran.persistentInfo.sequence);

    if (!m_db)
        return;
    auto errCode = m_db->transactionLog()->updateSequence(tran, TransactionLockType::Lazy);
    switch (errCode)
    {
        case ErrorCode::containsBecauseSequence:
            break;
        case ErrorCode::ok:
            // Proxy transaction to subscribed peers
            m_peers->updateLocalDistance(peerId, tran.persistentInfo.sequence);
            sendTransaction(tran, transportHeader);
            break;
        default:
            connection->setState(Connection::State::Error);
            resotreAfterDbError();
            break;
    }
}

void MessageBus::cleanupRuntimeInfo(const ec2::ApiPersistentIdData& peer)
{
    // If media server was restarted it could get new DB.
    // At this case we would have two records in m_lastRuntimeInfo list.
    // As soon as 'old' record will be removed resend runtime notification
    // to make sure we emit later runtime version.
    m_lastRuntimeInfo.remove(peer);
    auto itr = m_lastRuntimeInfo.lowerBound(ApiPersistentIdData(peer.id, QnUuid()));
    if (itr != m_lastRuntimeInfo.end() && itr.key().id == peer.id)
    {
        if (m_handler)
        {
            QnTransaction<ApiRuntimeData> tran(ApiCommand::runtimeInfoChanged, peer.id);
            tran.params = itr.value();
            m_handler->triggerNotification(tran, NotificationSource::Remote);
        }
    }
}

void MessageBus::gotTransaction(
    const QnTransaction<ApiRuntimeData> &tran,
    const P2pConnectionPtr& connection,
    const TransportHeader& transportHeader)
{
    if (localPeer().isServer() && !isSubscribedTo(connection->remotePeer()))
        return; // Ignore deprecated transaction.

    ApiPersistentIdData peerId(tran.peerID, tran.params.peer.persistentId);

    if (m_lastRuntimeInfo[peerId] == tran.params)
        return; //< Already processed. Ignore same transaction.

    m_lastRuntimeInfo[peerId] = tran.params;

    if (m_handler)
        m_handler->triggerNotification(tran, NotificationSource::Remote);
    emitPeerFoundLostSignals();
    // Proxy transaction to subscribed peers
    sendTransaction(tran, transportHeader);
}

template <class T>
void MessageBus::gotTransaction(
    const QnTransaction<T>& tran,
    const P2pConnectionPtr& connection,
    const TransportHeader& transportHeader)
{
    if (nx::utils::log::isToBeLogged(cl_logDEBUG1, this))
        printTran(connection, tran, Connection::Direction::incoming);

    ApiPersistentIdData peerId(tran.peerID, tran.persistentInfo.dbID);

    //NX_ASSERT(connection->localPeerSubscribedTo(peerId)); //< loop
    //NX_ASSERT(!context(connection)->isRemotePeerSubscribedTo(peerId)); //< loop
    auto transactionDescriptor = getTransactionDescriptorByTransaction(tran);

    // process special cases
    switch (tran.command)
    {
        // TODO: move it to the global setting param or emit this data via NotificationManager
        case ApiCommand::forcePrimaryTimeServer:
            m_timeSyncManager->onGotPrimariTimeServerTran(tran);
            if (localPeer().isServer())
                sendTransaction(tran, transportHeader); //< Proxy
            return;
        case ApiCommand::broadcastPeerSyncTime:
            m_timeSyncManager->resyncTimeWithPeer(tran.peerID);
            return; // do not proxy.
        default:
            break; //< Not a special case
    }

    if (m_db)
    {
        if (transactionDescriptor->isPersistent)
        {
            updateOfflineDistance(connection, peerId, tran.persistentInfo.sequence);
            std::unique_ptr<ec2::detail::QnDbManager::QnLazyTransactionLocker> dbTran;
            dbTran.reset(new ec2::detail::QnDbManager::QnLazyTransactionLocker(m_db->getTransaction()));

            QByteArray serializedTran =
                m_ubjsonTranSerializer->serializedTransaction(tran);
            ErrorCode errorCode = dbManager(m_db, connection.staticCast<Connection>()->userAccessData())
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
                proxyFillerTransaction(tran, transportHeader);
                return;
            case ErrorCode::containsBecauseSequence:
                dbTran->commit();
                return; // do not proxy if transaction already exists
            default:
                NX_WARNING(
                    this,
                    lit("Can't handle transaction %1: %2. Reopening connection...")
                    .arg(ApiCommand::toString(tran.command))
                    .arg(ec2::toString(errorCode)));
                dbTran.reset(); //< rollback db data
                connection->setState(Connection::State::Error);
                resotreAfterDbError();
                return;
            }
        }

        // Proxy transaction to subscribed peers
        sendTransaction(tran, transportHeader);
    }

    if (m_handler)
        m_handler->triggerNotification(tran, NotificationSource::Remote);
}

template <class T>
void MessageBus::gotUnicastTransaction(
    const QnTransaction<T>& tran,
    const P2pConnectionPtr& connection,
    const TransportHeader& header)
{
    if (nx::utils::log::isToBeLogged(cl_logDEBUG1, this))
        printTran(connection, tran, Connection::Direction::incoming);

    std::set<QnUuid> unprocessedPeers;
    for (auto& peer: header.dstPeers)
    {
        if (peer == localPeer().id)
        {
            if (m_handler)
                m_handler->triggerNotification(tran, NotificationSource::Remote);
            continue;
        }
        unprocessedPeers.insert(peer);
    }

    // Split dstPeers by connection.
    QMap<P2pConnectionPtr, TransportHeader> dstByConnection;
    for (const auto& dstPeer: unprocessedPeers)
    {
        QVector<ApiPersistentIdData> via;
        int distance = kMaxDistance;
        QnUuid dstPeerId = routeToPeerVia(dstPeer, &distance);
        if (distance > kMaxOnlineDistance || dstPeerId.isNull())
        {
            NX_WARNING(this, lm("Drop unicast transaction because no route found"));
            continue;
        }
        if (auto& dstConnection = m_connections.value(dstPeerId))
            dstByConnection[dstConnection].dstPeers.push_back(dstPeer);
        else
            NX_ASSERT(0, lm("Drop unicast transaction. Can't find connection to route"));
    }
    for (TransportHeader& newHeader: dstByConnection)
        newHeader.via = header.via;
    sendUnicastTransactionImpl(tran, dstByConnection);
}

void MessageBus::resotreAfterDbError()
{
    // p2pMessageBus uses lazy transactions. It means it does 1 commit per time.
    // In case of DB error transactionLog state is invalid because part of data was rolled back.
    m_db->transactionLog()->init();
    dropConnections();
}

bool MessageBus::handlePushTransactionList(const P2pConnectionPtr& connection, const QByteArray& data)
{
    if (data.isEmpty())
    {
        context(connection)->recvDataInProgress = false;
        return true; //< eof pushTranList reached
    }
    bool success = false;
    auto tranList =  deserializeTransactionList(data, &success);
    if (!success)
        return false;
    for (const auto& transaction: tranList)
    {
        if (!handlePushTransactionData(connection, transaction, TransportHeader()))
            return false;
    }
    return true;
}

template <typename Function>
bool MessageBus::handleTransactionWithHeader(
    const P2pConnectionPtr& connection,
    const QByteArray& data,
    Function function)
{
    int headerSize = 0;
    TransportHeader header;
    if (connection->remotePeer().dataFormat == Qn::UbjsonFormat)
        header = deserializeTransportHeader(data, &headerSize);
    else
        header.dstPeers.push_back(localPeer().id);
    using namespace std::placeholders;
    return handleTransaction(
        this,
        connection->remotePeer().dataFormat,
        data.mid(headerSize),
        std::bind(function, this, _1, connection, header),
        [](Qn::SerializationFormat, const QByteArray&) { return false; });

    return true;
}

bool MessageBus::handlePushTransactionData(
    const P2pConnectionPtr& connection,
    const QByteArray& serializedTran,
    const TransportHeader& header)
{
    using namespace std::placeholders;
    return handleTransaction(
        this,
        connection->remotePeer().dataFormat,
        std::move(serializedTran),
        std::bind(GotTransactionFuction(), this, _1, connection, header),
        [](Qn::SerializationFormat, const QByteArray&) { return false; });
}

bool MessageBus::isSubscribedTo(const ApiPersistentIdData& peer) const
{
    QnMutexLocker lock(&m_mutex);
    if (ApiPersistentIdData(localPeer()) == peer)
        return true;
    for (const auto& connection: m_connections)
    {
        if (connection->state() != Connection::State::Connected)
            continue;
        if (context(connection)->isLocalPeerSubscribedTo(peer))
            return true;
    }
    return false;
}

qint32 MessageBus::distanceTo(const ApiPersistentIdData& peer) const
{
    QnMutexLocker lock(&m_mutex);
    if (ApiPersistentIdData(localPeer()) == peer)
        return 0;
    return m_peers->distanceTo(peer);
}

QMap<QnUuid, P2pConnectionPtr> MessageBus::connections() const
{
    QnMutexLocker lock(&m_mutex);
    return m_connections;
}

int MessageBus::connectionTries() const
{
    QnMutexLocker lock(&m_mutex);
    return m_connectionTries;
}

QSet<QnUuid> MessageBus::directlyConnectedClientPeers() const
{
    QSet<QnUuid> result;
    for (const auto& connection: m_connections)
    {
        if (connection->remotePeer().isClient())
            result.insert(connection->remotePeer().id);
    }
    return result;
}

QSet<QnUuid> MessageBus::directlyConnectedServerPeers() const
{
    QnMutexLocker lock(&m_mutex);
    return m_connections.keys().toSet();
}

QnUuid MessageBus::routeToPeerVia(const QnUuid& peerId, int* distance) const
{
    if (localPeer().id == peerId)
    {
        *distance = 0;
        return QnUuid();
    }

    QVector<ApiPersistentIdData> via;
    *distance = m_peers->distanceTo(peerId, &via);
    return via.isEmpty() ? QnUuid() : via[0].id;
}

int MessageBus::distanceToPeer(const QnUuid& peerId) const
{
    QnMutexLocker lock(&m_mutex);
    if (localPeer().id == peerId)
        return 0;
    return m_peers->distanceTo(peerId);
}

ConnectionContext* MessageBus::context(const P2pConnectionPtr& connection)
{
    return static_cast<ConnectionContext*> (connection->opaqueObject());
}

QVector<QnTransportConnectionInfo> MessageBus::connectionsInfo() const
{
    QVector<QnTransportConnectionInfo> result;
    QnMutexLocker lock(&m_mutex);

    for (const auto& connection: m_connections)
    {
        QnTransportConnectionInfo info;
        info.url = connection->remoteAddr();
        info.state = toString(connection->state());
        info.isIncoming = connection->direction() == Connection::Direction::incoming;
        info.remotePeerId = connection->remotePeer().id;
        info.isStarted = context(connection)->isLocalStarted;
        info.subscription = context(connection)->localSubscription;
        result.push_back(info);
    }

    auto remoteUrls = m_remoteUrls;
    remoteUrls.erase(std::remove_if(remoteUrls.begin(), remoteUrls.end(),
        [this](const RemoteConnection& data)
        {
            return m_connections.contains(data.peerId);
        }),
        remoteUrls.end());

    for (const auto& peer: remoteUrls)
    {
        QnTransportConnectionInfo info;
        info.url = peer.url;
        info.state = lit("Not opened");
        info.isIncoming = false;
        info.remotePeerId = peer.peerId;
        result.push_back(info);
    }

    return result;
}

void MessageBus::emitPeerFoundLostSignals()
{
    std::set<ApiPeerData> newAlivePeers;

    for (const auto& connection: m_connections)
        newAlivePeers.insert(connection->remotePeer());

    for (auto itr = m_peers->allPeerDistances.constBegin(); itr != m_peers->allPeerDistances.constEnd(); ++itr)
    {
        const auto& peer = itr.key();
        if (peer == ApiPersistentIdData(localPeer()))
            continue;
        if (itr->minDistance() < kMaxOnlineDistance)
        {
            const auto peerData = m_lastRuntimeInfo.value(peer);
            if (!peerData.peer.isNull())
                newAlivePeers.insert(peerData.peer);
        }
    }

    std::vector<ApiPeerData> newPeers;
    std::set_difference(
        newAlivePeers.begin(),
        newAlivePeers.end(),
        m_lastAlivePeers.begin(),
        m_lastAlivePeers.end(),
        std::inserter(newPeers, newPeers.begin()));

    std::vector<ApiPeerData> lostPeers;
    std::set_difference(
        m_lastAlivePeers.begin(),
        m_lastAlivePeers.end(),
        newAlivePeers.begin(),
        newAlivePeers.end(),
        std::inserter(lostPeers, lostPeers.begin()));

    for (const auto& peer: newPeers)
    {
        NX_DEBUG(this,
            lit("Peer %1 has found peer %2")
            .arg(qnStaticCommon->moduleDisplayName(localPeer().id))
            .arg(qnStaticCommon->moduleDisplayName(peer.id)));
        emit peerFound(peer.id, peer.peerType);
    }

    for (const auto& peer: lostPeers)
    {
        cleanupRuntimeInfo(peer);

        ApiPeerData samePeer(ApiPersistentIdData(peer.id, QnUuid()), peer.peerType);
        auto samePeerItr = newAlivePeers.lower_bound(samePeer);
        bool hasSimilarPeer = samePeerItr != newAlivePeers.end() && samePeerItr->id == peer.id;
        if (!hasSimilarPeer)
        {
            NX_DEBUG(this,
                lit("Peer %1 has lost peer %2")
                .arg(qnStaticCommon->moduleDisplayName(localPeer().id))
                .arg(qnStaticCommon->moduleDisplayName(peer.id)));
            emit peerLost(peer.id, peer.peerType);
        }
    }

    m_lastAlivePeers = newAlivePeers;
}

void MessageBus::setDelayIntervals(const DelayIntervals& intervals)
{
    QnMutexLocker lock(&m_mutex);
    m_intervals = intervals;
}

MessageBus::DelayIntervals MessageBus::delayIntervals() const
{
    QnMutexLocker lock(&m_mutex);
    return m_intervals;
}

QMap<ApiPersistentIdData, ApiRuntimeData> MessageBus::runtimeInfo() const
{
    QnMutexLocker lock(&m_mutex);
    return m_lastRuntimeInfo;
}

} // namespace p2p
} // namespace nx
