#include "p2p_server_message_bus.h"
#include <database/db_manager.h>
#include <common/common_module.h>
#include "ec_connection_notification_manager.h"

#include <transaction/transaction_message_bus_priv.h>
#include <utils/common/delayed.h>
#include <nx/network/p2p_transport/i_p2p_transport.h>

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
        ServerMessageBus* bus,
        const QnTransaction<T>& transaction,
        const P2pConnectionPtr& connection,
        const TransportHeader& transportHeader) const
    {
        bus->gotTransaction(transaction, connection, transportHeader);
    }
};

struct SendTransactionToTransportFuction
{
    typedef void result_type;

    template<class T>
    void operator()(
        MessageBus* bus,
        const QnTransaction<T>& transaction,
        const P2pConnectionPtr& connection) const
    {
        vms::api::PersistentIdData tranId(transaction.peerID, transaction.persistentInfo.dbID);
        NX_ASSERT(bus->context(connection)->isRemotePeerSubscribedTo(tranId));
        NX_ASSERT(!(nx::vms::api::PersistentIdData(connection->remotePeer()) == tranId), "Loop detected");

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
            NX_DEBUG(
                this,
                lit("Permission check failed while sending SERIALIZED transaction to peer %1")
                .arg(connection->remotePeer().id.toString()));
            return false;
        }

        connection->sendMessage(MessageType::pushTransactionData, serializedTran);
        return true;
    }
};

// ---------------------- ServerpMessageBus --------------

ServerMessageBus::ServerMessageBus(
    vms::api::PeerType peerType,
    QnCommonModule* commonModule,
    QnJsonTransactionSerializer* jsonTranSerializer,
    QnUbjsonTransactionSerializer* ubjsonTranSerializer)
    :
    base_type(peerType, commonModule, jsonTranSerializer, ubjsonTranSerializer)
{
    m_dbCommitTimer.restart();
}

void ServerMessageBus::setDatabase(ec2::detail::QnDbManager* db)
{
    m_db = db;
}

ServerMessageBus::~ServerMessageBus()
{
    stop();
}

void ServerMessageBus::sendAlivePeersMessage(const P2pConnectionPtr& connection)
{
    auto serializeMessage = [this](const P2pConnectionPtr& connection)
    {
        std::vector<PeerDistanceRecord> records;
        records.reserve(m_peers->allPeerDistances.size());
        const auto localPeer = vms::api::PersistentIdData(this->localPeer());
        for (auto itr = m_peers->allPeerDistances.cbegin(); itr != m_peers->allPeerDistances.cend(); ++itr)
        {
            if (isLocalConnection(itr.key()))
                continue; //< don't show this connection to other servers

            RoutingInfo viaList;
            qint32 minDistance = itr->minDistance(&viaList);


            // Don't broadcast foreign offline distances
            if (minDistance < kMaxDistance && minDistance > kMaxOnlineDistance)
                minDistance = itr->distanceVia(localPeer);

            if (minDistance == kMaxDistance)
                continue;
            const PeerNumberType peerNumber = m_localShortPeerInfo.encode(itr.key());
            PeerNumberType firstViaNumber = kUnknownPeerNumnber;
            if (!viaList.isEmpty() && !viaList.first().firstVia.isNull())
                firstViaNumber = m_localShortPeerInfo.encode(viaList.first().firstVia);
            records.emplace_back(PeerDistanceRecord{peerNumber, minDistance, firstViaNumber});
        }
        NX_ASSERT(!records.empty());

        std::sort(records.begin(), records.end(),
            [](const PeerDistanceRecord& left, const PeerDistanceRecord& right)
            {
                if (left.distance != right.distance)
                    return left.distance < right.distance;
                return left.peerNumber < right.peerNumber;
            });

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
            if (nx::utils::log::isToBeLogged(nx::utils::log::Level::verbose, this))
                printPeersMessage();
            connection->sendMessage(data);
        }
    };

    if (connection)
    {
        sendAlivePeersMessage(connection, serializeMessage(connection));
        return;
    }

    for (const auto& connection : m_connections)
    {
        if (connection->state() != Connection::State::Connected)
            continue;
        if (!context(connection)->isRemoteStarted)
            continue;
        sendAlivePeersMessage(connection, serializeMessage(connection));
    }
}

void ServerMessageBus::doPeriodicTasks()
{
    QnMutexLocker lock(&m_mutex);

    m_miscData.update();

    const QMap<vms::api::PersistentIdData, P2pConnectionPtr>& currentSubscription = getCurrentSubscription();
    createOutgoingConnections(currentSubscription); //< open new connections

    sendAlivePeersMessage();
    startStopConnections(currentSubscription);
    doSubscribe(currentSubscription);
    commitLazyData();
}

void ServerMessageBus::startStopConnections(const QMap<vms::api::PersistentIdData, P2pConnectionPtr>& currentSubscription)
{
    if (hasStartingConnections())
        return;

    // start using connection if need
    int maxStartsAtOnce = m_miscData.newConnectionsAtOnce;

    for (auto& connection : m_connections)
    {
        auto context = this->context(connection);
        if (connection->state() != Connection::State::Connected || context->isLocalStarted)
            continue; //< already in use or not ready yet

        vms::api::PersistentIdData peer = connection->remotePeer();
        if (needStartConnection(peer, currentSubscription))
        {
            context->isLocalStarted = true;
            connection->sendMessage(MessageType::start, QByteArray());
            if (--maxStartsAtOnce == 0)
                return; //< limit start requests at once
        }
    }
}

bool ServerMessageBus::needSubscribeDelay()
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

P2pConnectionPtr ServerMessageBus::findBestConnectionToSubscribe(
    const QList<vms::api::PersistentIdData>& viaList,
    QMap<P2pConnectionPtr, int> newSubscriptions) const
{
    P2pConnectionPtr result;
    int minSubscriptions = std::numeric_limits<int>::max();
    for (const auto& via : viaList)
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

void ServerMessageBus::doSubscribe(const QMap<vms::api::PersistentIdData, P2pConnectionPtr>& currentSubscription)
{
    if (hasStartingConnections())
        return;

    for (const auto& connection : m_connections)
    {
        const auto& data = context(connection);
        if (data->recvDataInProgress)
            return;
    }

    const bool needDelay = needSubscribeDelay();

    QMap<vms::api::PersistentIdData, P2pConnectionPtr> newSubscription = currentSubscription;
    QMap<P2pConnectionPtr, int> tmpNewSubscription; //< this connections will get N new subscriptions soon
    const auto localPeer = this->localPeer();
    bool isUpdated = false;

    const RouteToPeerMap& allPeerDistances = m_peers->allPeerDistances;
    const auto& alivePeers = m_peers->alivePeers;
    auto currentSubscriptionItr = currentSubscription.cbegin();
    for (auto itr = allPeerDistances.constBegin(); itr != allPeerDistances.constEnd(); ++itr)
    {
        const vms::api::PersistentIdData& peer = itr.key();
        if (peer == localPeer)
            continue;

        if (auto connection = findConnectionById(peer))
        {
            if (!connection->remotePeer().isServer())
                continue;
        }

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
            RoutingInfo viaListData;
            info.minDistance(&viaListData);
            const auto viaList = viaListData.keys();

            NX_ASSERT(!viaList.empty());
            // If any of connections with min distance subscribed to us then postpone our subscription.
            // It could happen if neighbor(or closer) peer just goes offline
            if (std::any_of(viaList.begin(), viaList.end(),
                [this, &peer, &localPeer](const vms::api::PersistentIdData& via)
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
                NX_VERBOSE(
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

void ServerMessageBus::commitLazyData()
{
    if (m_dbCommitTimer.hasExpired(commitIntervalMs))
    {
        ec2::detail::QnDbManager::QnDbTransactionLocker dbTran(m_db->getTransaction());
        dbTran.commit();
        m_dbCommitTimer.restart();
    }
    emit lazyDataCommtDone();
}

void ServerMessageBus::stop()
{
    {
        QnMutexLocker lock(&m_mutex);
        if (m_db->getTransaction())
        {
            ec2::detail::QnDbManager::QnDbTransactionLocker dbTran(m_db->getTransaction());
            dbTran.commit();
        }
    }
    base_type::stop();
}

void ServerMessageBus::sendInitialDataToCloud(const P2pConnectionPtr& connection)
{
    const auto& state = m_db->transactionLog()->getTransactionsState();
    if (nx::utils::log::isToBeLogged(nx::utils::log::Level::verbose, this))
    {
        QVector<vms::api::PersistentIdData> data;
        QVector<qint32> indexes;
        for (auto itr = state.values.cbegin(); itr != state.values.cend(); ++itr)
        {
            data << itr.key();
            indexes << itr.value();
        }
        printSubscribeMessage(
            connection->remotePeer().id, data, indexes);
    }

    auto data = serializeSubscribeAllRequest(state);
    data.data()[0] = (quint8)MessageType::subscribeAll;
    connection->sendMessage(data);
    context(connection)->isLocalStarted = true;
}

bool ServerMessageBus::gotPostConnection(
    const vms::api::PeerDataEx& remotePeer,
    std::unique_ptr<nx::network::AbstractStreamSocket> socket,
    nx::Buffer requestBody)
{
    {
        QnMutexLocker lock(&m_mutex);
        auto existingConnectionIt = std::find_if(
            m_connections.cbegin(),
            m_connections.cend(),
            [&remotePeer](const auto& connection)
            {
                return remotePeer.connectionGuid == connection->remotePeer().connectionGuid;
            });

        if (existingConnectionIt == m_connections.cend())
        {
            NX_DEBUG(
                this,
                lm("Got an incoming POST connection with guid %1 but failed to find an "
                    "existing connection with the same guid").args(remotePeer.connectionGuid));
            return false;
        }

        existingConnectionIt.value()->gotPostConnection(std::move(socket), std::move(requestBody));
        return true;
    }

    return false;
}

void ServerMessageBus::gotConnectionFromRemotePeer(
    const vms::api::PeerDataEx& remotePeer,
    ec2::ConnectionLockGuard connectionLockGuard,
    nx::network::P2pTransportPtr p2pTransport,
    const QUrlQuery& requestUrlQuery,
    const Qn::UserAccessData& userAccessData,
    std::function<void()> onConnectionClosedCallback)
{
    P2pConnectionPtr connection(new Connection(
        commonModule(),
        remotePeer,
        localPeerEx(),
        std::move(p2pTransport),
        requestUrlQuery,
        userAccessData,
        std::make_unique<nx::p2p::ConnectionContext>(),
        std::move(connectionLockGuard)));

    QnMutexLocker lock(&m_mutex);

    if (!isStarted())
    {
        NX_DEBUG(this, "P2p message bus is not started yet. Ignore incoming connection");
        return;
    }

    const auto remoteId = connection->remotePeer().id;
    m_peers->removePeer(connection->remotePeer());
    m_connections[remoteId] = connection;
    emitPeerFoundLostSignals();
    connectSignals(connection);
    startReading(connection);

    if (remotePeer.isClient())
    {
        // Clients use simplified logic. They are started and subscribed to all updates immediately.
        // Client don't use any p2p protocol related structures. It just gets transactions.
        context(connection)->isLocalStarted = true;
        m_peers->addRecord(remotePeer, remotePeer, nx::p2p::RoutingRecord(1, localPeer()));
        sendInitialDataToClient(connection);
    }
    context(connection)->onConnectionClosedCallback = onConnectionClosedCallback;

    lock.unlock();
    emit newDirectConnectionEstablished(connection.data());
}

void ServerMessageBus::sendInitialDataToClient(const P2pConnectionPtr& connection)
{
    sendRuntimeData(connection, m_lastRuntimeInfo.keys());

    {
        QnTransaction<vms::api::FullInfoData> tran(commonModule()->moduleGUID());
        tran.command = ApiCommand::getFullInfo;
        if (!readFullInfoData(connection.staticCast<Connection>()->userAccessData(),
            connection->remotePeer(), &tran.params))
        {
            connection->setState(Connection::State::Error);
            return;
        }
        sendTransactionImpl(connection, tran, TransportHeader());
    }
}

bool ServerMessageBus::readFullInfoData(
    const Qn::UserAccessData& userAccess,
    const vms::api::PeerData& remotePeer,
    vms::api::FullInfoData* outData)
{
    ErrorCode errorCode;
    if (remotePeer.peerType == vms::api::PeerType::mobileClient)
        errorCode = dbManager(m_db, userAccess).readFullInfoDataForMobileClient(outData, userAccess.userId);
    else
        errorCode = dbManager(m_db, userAccess).readFullInfoDataComplete(outData);

    if (errorCode != ErrorCode::ok)
        NX_WARNING(this, lm("Cannot execute query for FullInfoData: %1").arg(errorCode));
    return errorCode == ErrorCode::ok;
}

void ServerMessageBus::addOfflinePeersFromDb()
{
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

void ServerMessageBus::printSubscribeMessage(
    const QnUuid& remoteId,
    const QVector<vms::api::PersistentIdData>& subscribedTo,
    const QVector<qint32>& sequences) const
{
    QList<QString> records;
    int index = 0;
    for (const auto& peer : subscribedTo)
    {
        records << lit("\t\t\t\t\t To:  %1(dbId=%2) from: %3")
            .arg(qnStaticCommon->moduleDisplayName(peer.id))
            .arg(peer.persistentId.toString())
            .arg(sequences[index++]);
    }

    NX_VERBOSE(
        this,
        lit("Subscribe:\t %1 ---> %2:\n%3")
        .arg(qnStaticCommon->moduleDisplayName(localPeer().id))
        .arg(qnStaticCommon->moduleDisplayName(remoteId))
        .arg(records.join("\n")));
}

void ServerMessageBus::resubscribePeers(
    QMap<vms::api::PersistentIdData, P2pConnectionPtr> newSubscription)
{
    QMap<P2pConnectionPtr, QVector<vms::api::PersistentIdData>> updatedSubscription;
    for (auto itr = newSubscription.begin(); itr != newSubscription.end(); ++itr)
        updatedSubscription[itr.value()].push_back(itr.key());
    for (auto& value : updatedSubscription)
        std::sort(value.begin(), value.end());
    for (auto connection : m_connections.values())
    {
        const auto& newValue = updatedSubscription.value(connection);
        auto connectionContext = context(connection);
        if (connectionContext->localSubscription != newValue)
        {
            connectionContext->localSubscription = newValue;
            QVector<PeerNumberType> shortValues;
            for (const auto& id : newValue)
                shortValues.push_back(connectionContext->encode(id));

            NX_ASSERT(newValue.contains(connection->remotePeer()));
            auto sequences = m_db->transactionLog()->getTransactionsState(newValue);

            if (nx::utils::log::isToBeLogged(nx::utils::log::Level::verbose, this))
                printSubscribeMessage(connection->remotePeer().id, newValue, sequences);

            QVector<SubscribeRecord> request;
            request.reserve(shortValues.size());
            for (int i = 0; i < shortValues.size(); ++i)
                request.push_back({shortValues[i], sequences[i]});
            auto serializedData = serializeSubscribeRequest(request);
            serializedData.data()[0] = (quint8)(MessageType::subscribeForDataUpdates);
            connection->sendMessage(serializedData);

            connectionContext->recvDataInProgress = true;
        }
    }
}

bool ServerMessageBus::pushTransactionList(
    const P2pConnectionPtr& connection,
    const QList<QByteArray>& tranList)
{
    auto message = serializeTransactionList(tranList, 1);
    message.data()[0] = (quint8)MessageType::pushTransactionList;
    connection->sendMessage(message);
    return true;
}

bool ServerMessageBus::selectAndSendTransactions(
    const P2pConnectionPtr& connection,
    vms::api::TranState newSubscription,
    bool addImplicitData)
{

    QList<QByteArray> serializedTransactions;
    bool isFinished = true;
    if (addImplicitData)
    {
        // Add all implicit records from 0 sequence.
        for (const auto& key : m_db->transactionLog()->getTransactionsState().values.keys())
        {
            if (!newSubscription.values.contains(key))
                newSubscription.values.insert(key, 0);
        }
    }

    if (m_db)
    {
        const bool isCloudServer =
                    connection->remotePeer().peerType == vms::api::PeerType::cloudServer;
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

    if (nx::utils::log::isToBeLogged(nx::utils::log::Level::verbose, this))
    {
        for (const auto& serializedTran : serializedTransactions)
        {
            ec2::QnAbstractTransaction transaction;
            QnUbjsonReader<QByteArray> stream(&serializedTran);
            if (QnUbjson::deserialize(&stream, &transaction))
                printTran(connection, transaction, Connection::Direction::outgoing);
        }
    }

#if 1
    if (connection->remotePeer().peerType == vms::api::PeerType::server
            && connection->remotePeer().dataFormat == Qn::UbjsonFormat)
    {
        pushTransactionList(connection, serializedTransactions);
        if (!serializedTransactions.isEmpty() && isFinished)
            pushTransactionList(connection, QList<QByteArray>());
        return true;
    }
#endif

    using namespace std::placeholders;
    for (const auto& serializedTran : serializedTransactions)
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

void ServerMessageBus::proxyFillerTransaction(
    const ec2::QnAbstractTransaction& tran,
    const TransportHeader& transportHeader)
{
    // proxy filler transaction to avoid gaps in the persistent sequence
    ec2::QnTransaction<nx::vms::api::UpdateSequenceData> fillerTran(tran);
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

void ServerMessageBus::resotreAfterDbError()
{
    // p2pMessageBus uses lazy transactions. It means it does 1 commit per time.
    // In case of DB error transactionLog state is invalid because part of data was rolled back.
    m_db->transactionLog()->init();
    dropConnections();
}

void ServerMessageBus::gotTransaction(
    const QnTransaction<nx::vms::api::UpdateSequenceData>& tran,
    const P2pConnectionPtr& connection,
    const TransportHeader& transportHeader)
{
    base_type::gotTransaction(tran, connection, transportHeader);

    vms::api::PersistentIdData peerId(tran.peerID, tran.persistentInfo.dbID);
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

template <class T>
void ServerMessageBus::gotTransaction(
    const QnTransaction<T>& tran,
    const P2pConnectionPtr& connection,
    const TransportHeader& transportHeader)
{
    if (processSpecialTransaction(tran, connection, transportHeader))
        return;

    vms::api::PersistentIdData peerId(tran.peerID, tran.persistentInfo.dbID);
    const auto transactionDescriptor = getTransactionDescriptorByTransaction(tran);
    if (transactionDescriptor->isPersistent)
    {
        updateOfflineDistance(connection, peerId, tran.persistentInfo.sequence);
        std::unique_ptr<ec2::detail::QnDbManager::QnLazyTransactionLocker> dbTran;
        dbTran.reset(new ec2::detail::QnDbManager::QnLazyTransactionLocker(m_db->getTransaction()));

        auto userAccessData = connection.staticCast<Connection>()->userAccessData();
        QByteArray serializedTran =
            m_ubjsonTranSerializer->serializedTransaction(tran);
        ErrorCode errorCode = dbManager(m_db, userAccessData)
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

            NX_VERBOSE(this,
                lit("Skip transaction because of timestamp: %1, seq: %2, timestamp: %3")
                    .arg(ApiCommand::toString(tran.command))
                    .arg(tran.persistentInfo.sequence)
                    .arg(tran.persistentInfo.timestamp.toString()));
            return;
        case ErrorCode::containsBecauseSequence:

            NX_VERBOSE(this,
                lit("Skip transaction because of sequence: %1, seq: %2, timestamp: %3")
                    .arg(ApiCommand::toString(tran.command))
                    .arg(tran.persistentInfo.sequence)
                    .arg(tran.persistentInfo.timestamp.toString()));
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

    if (m_handler)
    {
        auto amendedTran = tran;
        amendOutputDataIfNeeded(Qn::kSystemAccess, &amendedTran.params);
        m_handler->triggerNotification(amendedTran, NotificationSource::Remote);
    }
}

bool ServerMessageBus::handlePushTransactionData(
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

bool ServerMessageBus::handlePushImpersistentBroadcastTransaction(
    const P2pConnectionPtr& connection,
    const QByteArray& payload)
{
    return handleTransactionWithHeader(
        this,
        connection,
        payload,
        GotTransactionFuction());
}

bool ServerMessageBus::validateRemotePeerData(const vms::api::PeerDataEx& remotePeer)
{
    if (remotePeer.identityTime > commonModule()->systemIdentityTime())
    {
        if (m_restartPending.test_and_set())
            return false; //< Restart pending already queued.

        // Switch to the new systemIdentityTime. It allows to push restored from backup database data.
        NX_INFO(typeid(Connection), lm("Remote peer %1 has database restore time greater then "
            "current peer. Restarting and resync database with remote peer")
            .arg(remotePeer.id.toString()));

        executeDelayed(
            [this, remotePeer]()
            {
                stop();
                commonModule()->setSystemIdentityTime(remotePeer.identityTime, remotePeer.id);
            },
            0, qApp->thread());

        return false;
    }
    return true;

}

} // namespace p2p
} // namespace nx
