#include "server_transaction_message_bus.h"
#include "ec_connection_notification_manager.h"

#include <database/db_manager.h>
#include <transaction/transaction_message_bus_priv.h>

using namespace nx;

namespace ec2 {

struct GotTransactionFuction
{
    typedef void result_type;

    template<class T>
    void operator()(
        ServerTransactionMessageBus* bus,
        const QnTransaction<T> &transaction,
        QnTransactionTransport *sender,
        const QnTransactionTransportHeader &transportHeader) const
    {
        bus->gotTransaction(transaction, sender, transportHeader);
    }
};

struct SendTransactionToTransportFuction
{
    typedef void result_type;

    template<class T>
    void operator()(
        ServerTransactionMessageBus* bus,
        const QnTransaction<T> &transaction,
        QnTransactionTransport *sender,
        const QnTransactionTransportHeader &transportHeader) const
    {
        bus->sendTransactionToTransport(transaction, sender, transportHeader);
    }
};

struct SendTransactionToTransportFastFuction
{
    bool operator()(ServerTransactionMessageBus* /*bus*/, Qn::SerializationFormat srcFormat, const QByteArray& serializedTran, QnTransactionTransport *sender,
        const QnTransactionTransportHeader &transportHeader) const
    {
        return sender->sendSerializedTransaction(srcFormat, serializedTran, transportHeader);
    }
};

// --------------------------------- ServerTransactionMessageBus ------------------------------

ServerTransactionMessageBus::ServerTransactionMessageBus(
    vms::api::PeerType peerType,
    QnCommonModule* commonModule,
    QnJsonTransactionSerializer* jsonTranSerializer,
    QnUbjsonTransactionSerializer* ubjsonTranSerializer)
    :
    base_type(peerType, commonModule, jsonTranSerializer, ubjsonTranSerializer)
{
}

ServerTransactionMessageBus::~ServerTransactionMessageBus()
{
}

void ServerTransactionMessageBus::setDatabase(detail::QnDbManager* db)
{
    m_db = db;
}

void ServerTransactionMessageBus::printTranState(const nx::vms::api::TranState& tranState)
{
    if (!nx::utils::log::isToBeLogged(nx::utils::log::Level::debug, QnLog::EC2_TRAN_LOG))
        return;

    for (auto itr = tranState.values.constBegin(); itr != tranState.values.constEnd(); ++itr)
    {
        NX_DEBUG(QnLog::EC2_TRAN_LOG.join(this), lm("key=%1 (dbID=%2) need after=%3"),
            itr.key().id.toString(), itr.key().persistentId.toString(), itr.value());
    }
}

bool ServerTransactionMessageBus::gotAliveData(
    const vms::api::PeerAliveData& aliveData,
    QnTransactionTransport* transport,
    const QnTransactionTransportHeader* ttHeader)
{
    base_type::gotAliveData(aliveData, transport, ttHeader);

    if (transport && transport->isSyncDone() && aliveData.isAlive)
    {
        if (!aliveData.persistentState.values.empty())
        {
            // check current persistent state
            if (!m_db->transactionLog()->contains(aliveData.persistentState))
            {
                NX_DEBUG(QnLog::EC2_TRAN_LOG, lit("DETECT transaction GAP via update message. Resync with peer %1").
                    arg(transport->remotePeer().id.toString()));
                NX_DEBUG(QnLog::EC2_TRAN_LOG.join(this), lit("peer state:"));
                printTranState(aliveData.persistentState);
                resyncWithPeer(transport);
            }
        }
    }

    return true;
}

bool ServerTransactionMessageBus::checkSequence(
    const QnTransactionTransportHeader& transportHeader,
    const QnAbstractTransaction& tran,
    QnTransactionTransport* transport)
{
    if (!base_type::checkSequence(transportHeader, tran, transport))
        return false;

    // 2. check persistent sequence
    if (tran.persistentInfo.isNull())
        return true; // nothing to check

    vms::api::PersistentIdData persistentKey(tran.peerID, tran.persistentInfo.dbID);
    int persistentSeq = m_db->transactionLog()->getLatestSequence(persistentKey);

    if (nx::utils::log::isToBeLogged(nx::utils::log::Level::warning, QnLog::EC2_TRAN_LOG))
    {
        if (!transport->isSyncDone() && transport->isReadSync(ApiCommand::NotDefined) && transportHeader.sender != transport->remotePeer().id)
        {
            NX_WARNING(QnLog::EC2_TRAN_LOG, lit("Got transaction from peer %1 while sync with peer %2 in progress").
                arg(transportHeader.sender.toString()).arg(transport->remotePeer().id.toString()));
        }
    }

    if (tran.persistentInfo.sequence > persistentSeq + 1)
    {
        if (transport->isSyncDone())
        {
            // gap in persistent data detect, do resync
            NX_DEBUG(QnLog::EC2_TRAN_LOG, lit("GAP in persistent data detected! for peer %1 Expected seq=%2, but got seq=%3").
                arg(tran.peerID.toString()).arg(persistentSeq + 1).arg(tran.persistentInfo.sequence));

            if (!transport->remotePeer().isClient() && !vms::api::PeerData::isClient(m_localPeerType))
                queueSyncRequest(transport);
            else
                transport->setState(QnTransactionTransport::Error); // reopen
            return false;
        }
        else
        {
            NX_DEBUG(QnLog::EC2_TRAN_LOG, lit("GAP in persistent data, but sync in progress %1. Expected seq=%2, but got seq=%3").
                arg(tran.peerID.toString()).arg(persistentSeq + 1).arg(tran.persistentInfo.sequence));
        }
    }
    return true;
}

template <class T>
void ServerTransactionMessageBus::sendTransactionToTransport(
    const QnTransaction<T> &tran,
    QnTransactionTransport* transport,
    const QnTransactionTransportHeader &transportHeader)
{
    NX_ASSERT(!tran.isLocal());
    transport->sendTransaction(tran, transportHeader);
}

void ServerTransactionMessageBus::onGotTransactionSyncRequest(
    QnTransactionTransport* sender,
    const QnTransaction<nx::vms::api::SyncRequestData> &tran)
{
    sender->setWriteSync(true);

    QnTransactionTransportHeader ttUnicast;
    ttUnicast.processedPeers << sender->remotePeer().id << commonModule()->moduleGUID();
    ttUnicast.dstPeers << sender->remotePeer().id;
    QnTransactionTransportHeader ttBroadcast(ttUnicast);
    ttBroadcast.flags |= Qn::TT_ProxyToClient;

    QList<QByteArray> serializedTransactions;
    const ErrorCode errorCode = m_db->transactionLog()->getTransactionsAfter(
        tran.params.persistentState,
        sender->remotePeer().peerType == vms::api::PeerType::cloudServer,
        serializedTransactions);

    if (errorCode == ErrorCode::ok)
    {
        NX_DEBUG(QnLog::EC2_TRAN_LOG.join(this),
            lm("got sync request from peer %1. Need transactions after:"),
            sender->remotePeer().id.toString());
        printTranState(tran.params.persistentState);
        NX_DEBUG(QnLog::EC2_TRAN_LOG.join(this), lm("exist %1 new transactions"),
            serializedTransactions.size());

        NX_ASSERT(m_connections.contains(sender->remotePeer().id));
        NX_ASSERT(sender->getState() >= QnTransactionTransport::ReadyForStreaming);
        QnTransaction<nx::vms::api::TranStateResponse> tranSyncResponse(
            ApiCommand::tranSyncResponse,
            commonModule()->moduleGUID());
        tranSyncResponse.params.result = 0;
        sender->sendTransaction(tranSyncResponse, ttUnicast);

        sendRuntimeInfo(sender, ttBroadcast, tran.params.runtimeState);

        using namespace std::placeholders;
        for (const auto& serializedTran : serializedTransactions)
            if (!handleTransaction(
                this,
                Qn::UbjsonFormat,
                serializedTran,
                std::bind(
                    SendTransactionToTransportFuction(),
                    this, _1, sender, ttBroadcast),
                std::bind(
                    SendTransactionToTransportFastFuction(),
                    this, _1, _2, sender, ttBroadcast)))
            {
                sender->setState(QnTransactionTransport::Error);
            }

        QnTransaction<nx::vms::api::TranSyncDoneData> tranSyncDone(
            ApiCommand::tranSyncDone,
            commonModule()->moduleGUID());
        tranSyncResponse.params.result = 0;
        sender->sendTransaction(tranSyncDone, ttUnicast);

        return;
    }
    else
    {
        qWarning() << "Can't execute query for sync with server peer!";
    }
}

void ServerTransactionMessageBus::queueSyncRequest(QnTransactionTransport* transport)
{
    // send sync request
    NX_ASSERT(!transport->isSyncInProgress());
    transport->setReadSync(false);
    transport->setSyncDone(false);

    if (isSyncInProgress())
    {
        transport->setNeedResync(true);
        return;
    }

    transport->setSyncInProgress(true);
    transport->setNeedResync(false);

    QnTransaction<nx::vms::api::SyncRequestData> requestTran(
        ApiCommand::tranSyncRequest,
        commonModule()->moduleGUID());
    requestTran.params.persistentState = m_db->transactionLog()->getTransactionsState();
    requestTran.params.runtimeState = m_runtimeTransactionLog->getTransactionsState();

    NX_DEBUG(QnLog::EC2_TRAN_LOG.join(this), lm("send syncRequest to peer %1"),
        transport->remotePeer().id.toString());

    printTranState(requestTran.params.persistentState);
    transport->sendTransaction(requestTran, vms::api::PeerSet(
            {transport->remotePeer().id, commonModule()->moduleGUID()}));
}

bool ServerTransactionMessageBus::isSyncInProgress() const
{
    for (QnConnectionMap::const_iterator itr = m_connections.begin(); itr != m_connections.end(); ++itr)
    {
        QnTransactionTransport* transport = *itr;
        if (transport->isSyncInProgress())
            return true;
    }
    return false;
}

bool ServerTransactionMessageBus::readFullInfoData(
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
    {
        qWarning() << "Cannot execute query for FullInfoData:" << toString(errorCode);
        return false;
    }
    return true;
}

bool ServerTransactionMessageBus::sendInitialData(QnTransactionTransport* transport)
{
    /** Send all data to the client peers on the connect. */
        vms::api::PeerSet processedPeers({transport->remotePeer().id, commonModule()->moduleGUID()});
    if (!base_type::sendInitialData(transport))
        return false;

    if (transport->remotePeer().peerType == vms::api::PeerType::desktopClient ||
        transport->remotePeer().peerType == vms::api::PeerType::videowallClient ||
        transport->remotePeer().peerType == vms::api::PeerType::mobileClient)
    {
        /** Request all data to be sent to the Desktop Client peers on the connect. */
        QnTransaction<vms::api::FullInfoData> tran;
        tran.command = ApiCommand::getFullInfo;
        tran.peerID = commonModule()->moduleGUID();
        if (!readFullInfoData(transport->getUserAccessData(), transport->remotePeer(), &tran.params))
            return false;

        transport->setWriteSync(true);
        sendRuntimeInfo(transport, processedPeers, {});
        transport->sendTransaction(tran, processedPeers);
        transport->setReadSync(true);
    }
    else if (transport->remotePeer().peerType == vms::api::PeerType::oldMobileClient)
    {
        /** Request all data to be sent to the client peers on the connect. */
        QnTransaction<vms::api::MediaServerDataExList> tranServers;
        tranServers.command = ApiCommand::getMediaServersEx;
        tranServers.peerID = commonModule()->moduleGUID();
        if (dbManager(m_db, transport->getUserAccessData()).doQuery(QnUuid(), tranServers.params) != ErrorCode::ok)
        {
            qWarning() << "Can't execute query for sync with client peer!";
            return false;
        }

        QnTransaction<nx::vms::api::CameraDataExList> tranCameras;
        tranCameras.command = ApiCommand::getCamerasEx;
        tranCameras.peerID = commonModule()->moduleGUID();
        if (dbManager(m_db, transport->getUserAccessData())
            .doQuery(QnCameraDataExQuery(), tranCameras.params) != ErrorCode::ok)
        {
            qWarning() << "Can't execute query for sync with client peer!";
            return false;
        }

        QnTransaction<vms::api::UserDataList> tranUsers;
        tranUsers.command = ApiCommand::getUsers;
        tranUsers.peerID = commonModule()->moduleGUID();
        if (dbManager(m_db, transport->getUserAccessData()).doQuery(QnUuid(), tranUsers.params) != ErrorCode::ok)
        {
            qWarning() << "Can't execute query for sync with client peer!";
            return false;
        }

        QnTransaction<nx::vms::api::LayoutDataList> tranLayouts;
        tranLayouts.command = ApiCommand::getLayouts;
        tranLayouts.peerID = commonModule()->moduleGUID();
        if (dbManager(m_db, transport->getUserAccessData()).doQuery(QnUuid(), tranLayouts.params) != ErrorCode::ok)
        {
            qWarning() << "Can't execute query for sync with client peer!";
            return false;
        }

        QnTransaction<nx::vms::api::ServerFootageDataList> tranCameraHistory;
        tranCameraHistory.command = ApiCommand::getCameraHistoryItems;
        tranCameraHistory.peerID = commonModule()->moduleGUID();
        if (dbManager(m_db, transport->getUserAccessData()).doQuery(nullptr, tranCameraHistory.params) != ErrorCode::ok)
        {
            qWarning() << "Can't execute query for sync with client peer!";
            return false;
        }

        transport->setWriteSync(true);
        sendRuntimeInfo(transport, processedPeers, {});
        transport->sendTransaction(tranServers, processedPeers);
        transport->sendTransaction(tranCameras, processedPeers);
        transport->sendTransaction(tranUsers, processedPeers);
        transport->sendTransaction(tranLayouts, processedPeers);
        transport->sendTransaction(tranCameraHistory, processedPeers);
        transport->setReadSync(true);
    }

    if (!transport->remotePeer().isClient() && !vms::api::PeerData::isClient(m_localPeerType))
        queueSyncRequest(transport);

    return true;
}

void ServerTransactionMessageBus::fillExtraAliveTransactionParams(
    vms::api::PeerAliveData* outAliveData)
{
    if (outAliveData->isAlive && outAliveData->peer.id == commonModule()->moduleGUID())
    {
        outAliveData->persistentState = m_db->transactionLog()->getTransactionsState();
        outAliveData->runtimeState = m_runtimeTransactionLog->getTransactionsState();
    }
}

void ServerTransactionMessageBus::logTransactionState()
{
    NX_DEBUG(QnLog::EC2_TRAN_LOG.join(this), "Current transaction state:");
    printTranState(m_db->transactionLog()->getTransactionsState());
}

void ServerTransactionMessageBus::gotConnectionFromRemotePeer(
    const std::string& connectionGuid,
    ConnectionLockGuard connectionLockGuard,
    std::unique_ptr<nx::network::AbstractStreamSocket> socket,
    ConnectionType::Type connectionType,
    const vms::api::PeerData& remotePeer,
    qint64 remoteSystemIdentityTime,
    const nx::network::http::Request& request,
    const QByteArray& contentEncoding,
    std::function<void()> ttFinishCallback,
    const Qn::UserAccessData &userAccessData)
{
    if (m_restartPending)
        return; // reject incoming connection because of media server is about to restart

    QnTransactionTransport* transport = new QnTransactionTransport(
        this,
        connectionGuid,
        std::move(connectionLockGuard),
        localPeer(),
        remotePeer,
        std::move(socket),
        connectionType,
        request,
        contentEncoding,
        userAccessData);
    transport->setRemoteIdentityTime(remoteSystemIdentityTime);
    transport->setBeforeDestroyCallback(ttFinishCallback);
    connect(transport, &QnTransactionTransport::gotTransaction, this, &ServerTransactionMessageBus::at_gotTransaction, Qt::QueuedConnection);
    connect(transport, &QnTransactionTransport::stateChanged, this, &ServerTransactionMessageBus::at_stateChanged, Qt::QueuedConnection);
    connect(transport, &QnTransactionTransport::remotePeerUnauthorized, this, &ServerTransactionMessageBus::emitRemotePeerUnauthorized, Qt::DirectConnection);

    QnMutexLocker lock(&m_mutex);

    NX_ASSERT(
        std::find_if(
            m_connectingConnections.begin(), m_connectingConnections.end(),
            [&connectionGuid](QnTransactionTransport* connection)
        {
            return connection->connectionGuid() == connectionGuid;
        }
        ) == m_connectingConnections.end());

    transport->moveToThread(thread());
    m_connectingConnections << transport;
    NX_ASSERT(!m_connections.contains(remotePeer.id));
}

void ServerTransactionMessageBus::gotIncomingTransactionsConnectionFromRemotePeer(
    const std::string& connectionGuid,
    std::unique_ptr<nx::network::AbstractStreamSocket> socket,
    const vms::api::PeerData &/*remotePeer*/,
    qint64 /*remoteSystemIdentityTime*/,
    const nx::network::http::Request& request,
    const QByteArray& requestBuf)
{
    if (m_restartPending)
        return; // reject incoming connection because of media server is about to restart

    QnMutexLocker lock(&m_mutex);
    for (QnTransactionTransport* transport : m_connections.values())
    {
        if (transport->connectionGuid() == connectionGuid)
        {
            transport->setIncomingTransactionChannelSocket(
                std::move(socket),
                request,
                requestBuf);
            return;
        }
    }
}

bool ServerTransactionMessageBus::gotTransactionFromRemotePeer(
    const std::string& connectionGuid,
    const nx::network::http::Request& request,
    const QByteArray& requestMsgBody)
{
    if (m_restartPending)
        return false; // reject incoming connection because of media server is about to restart

    QnMutexLocker lock(&m_mutex);

    for (QnTransactionTransport* transport : m_connections.values())
    {
        if (transport->connectionGuid() == connectionGuid)
        {
            transport->receivedTransaction(
                request.headers,
                requestMsgBody);
            return true;
        }
    }

    return false;
}

ErrorCode ServerTransactionMessageBus::updatePersistentMarker(
    const QnTransaction<nx::vms::api::UpdateSequenceData>& tran)
{
    return m_db->transactionLog()->updateSequence(tran.params);
}

void ServerTransactionMessageBus::proxyFillerTransaction(
    const QnAbstractTransaction& tran,
    const QnTransactionTransportHeader& transportHeader)
{
    // proxy filler transaction to avoid gaps in the persistent sequence
    QnTransaction<nx::vms::api::UpdateSequenceData> fillerTran(tran);
    fillerTran.command = ApiCommand::updatePersistentSequence;
    nx::vms::api::SyncMarkerRecordData record;
    record.peerID = tran.peerID;
    record.dbID = tran.persistentInfo.dbID;
    record.sequence = tran.persistentInfo.sequence;
    fillerTran.params.markers.push_back(record);
    m_db->transactionLog()->updateSequence(fillerTran.params);
    proxyTransaction(fillerTran, transportHeader);
}

template <class T>
void ServerTransactionMessageBus::gotTransaction(const QnTransaction<T> &tran, QnTransactionTransport* sender, const QnTransactionTransportHeader &transportHeader)
{
    QnMutexLocker lock(&m_mutex);
    if (processSpecialTransaction(tran, sender, transportHeader))
        return;

    // These ones are 'general' transactions. They will go through the DbManager
    // and also will be notified about via the relevant notification manager.
    if (!tran.persistentInfo.isNull())
    {
        QByteArray serializedTran = m_ubjsonTranSerializer->serializedTransaction(tran);
        ErrorCode errorCode =
            dbManager(m_db, sender->getUserAccessData()).executeTransaction(tran, serializedTran);

        switch (errorCode)
        {
        case ErrorCode::ok:
        case ErrorCode::notImplemented:
            break;
        case ErrorCode::containsBecauseTimestamp:
            proxyFillerTransaction(tran, transportHeader);
        case ErrorCode::containsBecauseSequence:
            return; // do not proxy if transaction already exists
        default:
            NX_WARNING(QnLog::EC2_TRAN_LOG, lit("Can't handle transaction %1: %2. Reopening connection...")
                .arg(ApiCommand::toString(tran.command))
                .arg(ec2::toString(errorCode)));
            sender->setState(QnTransactionTransport::Error);
            return;
        }
    }

    if (m_handler)
        m_handler->triggerNotification(tran, NotificationSource::Remote);
    proxyTransaction(tran, transportHeader);
}


void ServerTransactionMessageBus::handleIncomingTransaction(
    QnTransactionTransport* sender,
    Qn::SerializationFormat tranFormat,
    QByteArray serializedTran,
    const QnTransactionTransportHeader &transportHeader)
{
    using namespace std::placeholders;
    if (!handleTransaction(
        this,
        tranFormat,
        std::move(serializedTran),
        std::bind(GotTransactionFuction(), this, _1, sender, transportHeader),
        [](Qn::SerializationFormat, const QByteArray&) { return false; }))
    {
        sender->setState(QnTransactionTransport::Error);
    }
}

} //namespace ec2
