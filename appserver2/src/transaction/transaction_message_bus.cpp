
#include "transaction_message_bus.h"

#include <QtCore/QTimer>

#include "remote_ec_connection.h"
#include "utils/common/systemerror.h"
#include "ec2_connection.h"
#include "common/common_module.h"

#include <transaction/transaction_transport.h>

#include "api/app_server_connection.h"
#include "api/runtime_info_manager.h"
#include "nx_ec/data/api_server_alive_data.h"
#include "utils/common/synctime.h"
#include "utils/network/global_module_finder.h"
#include "utils/network/router.h"
#include "nx_ec/data/api_server_alive_data.h"
#include "ec_connection_notification_manager.h"
#include "nx_ec/data/api_camera_data.h"
#include "nx_ec/data/api_resource_data.h"
#include "managers/time_manager.h"

#include <utils/common/checked_cast.h>
#include "utils/common/warnings.h"


#define TRANSACTION_MESSAGE_BUS_DEBUG

namespace ec2
{

static const int RECONNECT_TIMEOUT = 1000 * 5;
static const int ALIVE_UPDATE_INTERVAL = 1000 * 60;

struct GotTransactionFuction {
    typedef void result_type;

    template<class T> 
    void operator()(QnTransactionMessageBus *bus, const QnTransaction<T> &transaction, QnTransactionTransport *sender, const QnTransactionTransportHeader &transportHeader) const {
        bus->gotTransaction(transaction, sender, transportHeader);
    }
};

struct SendTransactionToTransportFuction {
    typedef void result_type;

    template<class T> 
    void operator()(QnTransactionMessageBus *bus, const QnTransaction<T> &transaction, QnTransactionTransport *sender, const QnTransactionTransportHeader &transportHeader) const {
        bus->sendTransactionToTransport(transaction, sender, transportHeader);
    }
};

template<class T, class Function>
bool handleTransactionParams(QnUbjsonReader<QByteArray> *stream, const QnAbstractTransaction &abstractTransaction, Function function) 
{
    QnTransaction<T> transaction(abstractTransaction);
    if (!QnUbjson::deserialize(stream, &transaction.params)) 
        return false;
            
    function(transaction);
    return true;
}

template<class Function>
bool handleTransaction(const QByteArray &serializedTransaction, const Function &function)
{
    QnAbstractTransaction transaction;
    QnUbjsonReader<QByteArray> stream(&serializedTransaction);
    if (!QnUbjson::deserialize(&stream, &transaction)) {
        qnWarning("Ignore bad transaction data. size=%1.", serializedTransaction.size());
        return false;
    }

    switch (transaction.command) {
    case ApiCommand::getFullInfo:           return handleTransactionParams<ApiFullInfoData>         (&stream, transaction, function);
    case ApiCommand::setResourceStatus:     return handleTransactionParams<ApiSetResourceStatusData>(&stream, transaction, function);
    case ApiCommand::setResourceParams:     return handleTransactionParams<ApiResourceParamsData>   (&stream, transaction, function);
    case ApiCommand::saveResource:          return handleTransactionParams<ApiResourceData>         (&stream, transaction, function);
    case ApiCommand::removeResource:        return handleTransactionParams<ApiIdData>               (&stream, transaction, function);
    case ApiCommand::setPanicMode:          return handleTransactionParams<ApiPanicModeData>        (&stream, transaction, function);
    case ApiCommand::saveCamera:            return handleTransactionParams<ApiCameraData>           (&stream, transaction, function);
    case ApiCommand::saveCameras:           return handleTransactionParams<ApiCameraDataList>       (&stream, transaction, function);
    case ApiCommand::removeCamera:          return handleTransactionParams<ApiIdData>               (&stream, transaction, function);
    case ApiCommand::addCameraHistoryItem:  return handleTransactionParams<ApiCameraServerItemData> (&stream, transaction, function);
    case ApiCommand::saveMediaServer:       return handleTransactionParams<ApiMediaServerData>      (&stream, transaction, function);
    case ApiCommand::removeMediaServer:     return handleTransactionParams<ApiIdData>               (&stream, transaction, function);
    case ApiCommand::saveUser:              return handleTransactionParams<ApiUserData>             (&stream, transaction, function);
    case ApiCommand::removeUser:            return handleTransactionParams<ApiIdData>               (&stream, transaction, function);
    case ApiCommand::saveBusinessRule:      return handleTransactionParams<ApiBusinessRuleData>     (&stream, transaction, function);
    case ApiCommand::removeBusinessRule:    return handleTransactionParams<ApiIdData>               (&stream, transaction, function);
    case ApiCommand::saveLayouts:           return handleTransactionParams<ApiLayoutDataList>       (&stream, transaction, function);
    case ApiCommand::saveLayout:            return handleTransactionParams<ApiLayoutData>           (&stream, transaction, function);
    case ApiCommand::removeLayout:          return handleTransactionParams<ApiIdData>               (&stream, transaction, function);
    case ApiCommand::saveVideowall:         return handleTransactionParams<ApiVideowallData>        (&stream, transaction, function);
    case ApiCommand::removeVideowall:       return handleTransactionParams<ApiIdData>               (&stream, transaction, function);
    case ApiCommand::videowallControl:      return handleTransactionParams<ApiVideowallControlMessageData>(&stream, transaction, function);
    case ApiCommand::addStoredFile:
    case ApiCommand::updateStoredFile:      return handleTransactionParams<ApiStoredFileData>       (&stream, transaction, function);
    case ApiCommand::removeStoredFile:      return handleTransactionParams<ApiStoredFilePath>       (&stream, transaction, function);
    case ApiCommand::broadcastBusinessAction:
    case ApiCommand::execBusinessAction:    return handleTransactionParams<ApiBusinessActionData>   (&stream, transaction, function);
    case ApiCommand::addLicenses:           return handleTransactionParams<ApiLicenseDataList>      (&stream, transaction, function);
    case ApiCommand::addLicense:            
    case ApiCommand::removeLicense:         return handleTransactionParams<ApiLicenseData>          (&stream, transaction, function);
    case ApiCommand::resetBusinessRules:    return handleTransactionParams<ApiResetBusinessRuleData>(&stream, transaction, function);
    case ApiCommand::uploadUpdate:          return handleTransactionParams<ApiUpdateUploadData>     (&stream, transaction, function);
    case ApiCommand::uploadUpdateResponce:  return handleTransactionParams<ApiUpdateUploadResponceData>(&stream, transaction, function);
    case ApiCommand::installUpdate:         return handleTransactionParams<ApiUpdateInstallData>    (&stream, transaction, function);
    case ApiCommand::addCameraBookmarkTags:
    case ApiCommand::removeCameraBookmarkTags:
                                            return handleTransactionParams<ApiCameraBookmarkTagDataList>(&stream, transaction, function);

    case ApiCommand::moduleInfo:            return handleTransactionParams<ApiModuleData>           (&stream, transaction, function);
    case ApiCommand::moduleInfoList:        return handleTransactionParams<ApiModuleDataList>       (&stream, transaction, function);

    case ApiCommand::discoverPeer:          return handleTransactionParams<ApiDiscoverPeerData>     (&stream, transaction, function);
    case ApiCommand::addDiscoveryInformation:
    case ApiCommand::removeDiscoveryInformation:
                                            return handleTransactionParams<ApiDiscoveryDataList>    (&stream, transaction, function);

    case ApiCommand::addConnection:
    case ApiCommand::removeConnection:
                                            return handleTransactionParams<ApiConnectionData>       (&stream, transaction, function);
    case ApiCommand::availableConnections:  return handleTransactionParams<ApiConnectionDataList>   (&stream, transaction, function);

    case ApiCommand::changeSystemName:      return handleTransactionParams<ApiSystemNameData>       (&stream, transaction, function);

    case ApiCommand::lockRequest:
    case ApiCommand::lockResponse:
    case ApiCommand::unlockRequest:         return handleTransactionParams<ApiLockData>             (&stream, transaction, function); 
    case ApiCommand::peerAliveInfo:         return handleTransactionParams<ApiPeerAliveData>        (&stream, transaction, function);
    case ApiCommand::tranSyncRequest:       return handleTransactionParams<QnTranState>             (&stream, transaction, function);
    case ApiCommand::tranSyncResponse:      return handleTransactionParams<QnTranStateResponse>     (&stream, transaction, function);
    case ApiCommand::runtimeInfoChanged:    return handleTransactionParams<ApiRuntimeData>          (&stream, transaction, function);
    case ApiCommand::broadcastPeerSystemTime: return handleTransactionParams<ApiPeerSystemTimeData> (&stream, transaction, function);
    case ApiCommand::forcePrimaryTimeServer:  return handleTransactionParams<ApiIdData>             (&stream, transaction, function);

    default:
        qWarning() << "Transaction type " << transaction.command << " is not implemented for delivery! Implement me!";
        Q_ASSERT_X(0, Q_FUNC_INFO, "Transaction type is not implemented for delivery! Implement me!");
        return false;
    }
}



// --------------------------------- QnTransactionMessageBus ------------------------------
static QnTransactionMessageBus* m_globalInstance = 0;

QnTransactionMessageBus* QnTransactionMessageBus::instance()
{
    return m_globalInstance;
}

QnTransactionMessageBus::QnTransactionMessageBus()
: 
    m_localPeer(QUuid(), Qn::PT_Server),
    m_binaryTranSerializer(new QnBinaryTransactionSerializer()),
    m_jsonTranSerializer(new QnJsonTransactionSerializer()),
    m_ubjsonTranSerializer(new QnUbjsonTransactionSerializer()),
	m_handler(nullptr),
    m_timer(nullptr), 
    m_mutex(QMutex::Recursive),
    m_thread(nullptr)
{
    if (m_thread)
        return;
    m_thread = new QThread();
    m_thread->setObjectName("QnTransactionMessageBusThread");
    moveToThread(m_thread);
    qRegisterMetaType<QnTransactionTransport::State>(); // TODO: #Elric #EC2 registration
    qRegisterMetaType<QnAbstractTransaction>("QnAbstractTransaction");
    m_timer = new QTimer();
    connect(m_timer, &QTimer::timeout, this, &QnTransactionMessageBus::at_timer);
    m_timer->start(500);
    m_aliveSendTimer.invalidate();

    assert( m_globalInstance == nullptr );
    m_globalInstance = this;
}

void QnTransactionMessageBus::start()
{
    assert(!m_localPeer.id.isNull());
    if (!m_thread->isRunning())
        m_thread->start();
}

QnTransactionMessageBus::~QnTransactionMessageBus()
{
    m_globalInstance = nullptr;

    if (m_thread) {
        m_thread->exit();
        m_thread->wait();
    }
    delete m_thread;
    delete m_timer;
}

void QnTransactionMessageBus::addAlivePeerInfo(ApiPeerData peerData, const QUuid& gotFromPeer)
{
    AlivePeersMap::iterator itr = m_alivePeers.find(peerData.id);
    if (itr == m_alivePeers.end()) 
    {
        AlivePeerInfo peer(peerData);
        if (!gotFromPeer.isNull())
            peer.proxyVia << gotFromPeer;
        m_alivePeers.insert(peerData.id, peer);
    }
    else {
        AlivePeerInfo& currentValue = itr.value();
        if (!currentValue.proxyVia.isEmpty() && !gotFromPeer.isNull())
            currentValue.proxyVia << gotFromPeer; // if peer is accessible directly (proxyVia is empty), do not extend proxy list
    }
}

void QnTransactionMessageBus::removeAlivePeer(const QUuid& id, bool sendTran, bool isRecursive)
{
    // 1. remove peer from alivePeers map

    m_lastTranSeq.remove(id);

    auto itr = m_alivePeers.find(id);
    if (itr == m_alivePeers.end())
        return;
    
    handlePeerAliveChanged(itr.value().peer, false, sendTran);
    m_alivePeers.erase(itr);

    // 2. remove peers proxy via current peer
    if (isRecursive)
        return;
    QSet<QUuid> morePeersToRemove;
    for (auto itr = m_alivePeers.begin(); itr != m_alivePeers.end(); ++itr)
    {
        AlivePeerInfo& otherPeer = itr.value();
        if (otherPeer.proxyVia.contains(id)) {
            otherPeer.proxyVia.remove(id);
            if (otherPeer.proxyVia.isEmpty()) {
                morePeersToRemove << otherPeer.peer.id;
            }
        }
    }
    foreach(const QUuid& p, morePeersToRemove)
        removeAlivePeer(p, true, true);
}

void QnTransactionMessageBus::processAliveData(const ApiPeerAliveData &aliveData, const QUuid& gotFromPeer)
{
#ifdef TRANSACTION_MESSAGE_BUS_DEBUG
    qDebug() << "received peerAlive transaction" << aliveData.peer.id << aliveData.peer.peerType;
#endif
    if (aliveData.peer.id == m_localPeer.id)
        return; // ignore himself

    // proxy alive info from non-direct connected host
    bool isPeerExist = m_alivePeers.contains(aliveData.peer.id);
    if (aliveData.isAlive) 
    {
        addAlivePeerInfo(ApiPeerData(aliveData.peer.id, aliveData.peer.peerType), gotFromPeer);
        if (!isPeerExist) 
            emit peerFound(aliveData);
    }
    else 
    {
        if (isPeerExist) {
            removeAlivePeer(aliveData.peer.id, false);
        }
    }
}

void QnTransactionMessageBus::onGotServerAliveInfo(const QnTransaction<ApiPeerAliveData> &tran, const QUuid& gotFromPeer)
{
    processAliveData(tran.params, gotFromPeer);
}

void QnTransactionMessageBus::onGotServerRuntimeInfo(const QnTransaction<ApiRuntimeData> &tran, const QUuid& gotFromPeer)
{
    processAliveData(ApiPeerAliveData(tran.params.peer, true), gotFromPeer);
}

void QnTransactionMessageBus::at_gotTransaction(const QByteArray &serializedTran, const QnTransactionTransportHeader &transportHeader)
{
    QnTransactionTransport* sender = checked_cast<QnTransactionTransport*>(this->sender());
    if (!sender || sender->getState() != QnTransactionTransport::ReadyForStreaming)
        return;

    Q_ASSERT(transportHeader.processedPeers.contains(sender->remotePeer().id));

    using namespace std::placeholders;
    if(!handleTransaction(serializedTran, std::bind(GotTransactionFuction(), this, _1, sender, transportHeader)))
        sender->setState(QnTransactionTransport::Error);
}


// ------------------ QnTransactionMessageBus::CustomHandler -------------------


void QnTransactionMessageBus::onGotDistributedMutexTransaction(const QnTransaction<ApiLockData>& tran) {
    if(tran.command == ApiCommand::lockRequest)
        emit gotLockRequest(tran.params);
    else if(tran.command == ApiCommand::lockResponse) 
        emit gotLockResponse(tran.params);
}

void QnTransactionMessageBus::onGotTransactionSyncResponse(QnTransactionTransport* sender, const QnTransaction<QnTranStateResponse> &tran) {
    Q_UNUSED(tran)
	sender->setReadSync(true);

    sendConnectionsData();
    sendModulesData();
}

template <class T>
void QnTransactionMessageBus::sendTransactionToTransport(const QnTransaction<T> &tran, QnTransactionTransport* transport, const QnTransactionTransportHeader &transportHeader) {
    Q_ASSERT(!tran.isLocal);
    transport->sendTransaction(tran, transportHeader);
}
        
template <class T>
void QnTransactionMessageBus::gotTransaction(const QnTransaction<T> &tran, QnTransactionTransport* sender, const QnTransactionTransportHeader &transportHeader) 
{
    QMutexLocker lock(&m_mutex);

    AlivePeersMap:: iterator itr = m_alivePeers.find(tran.peerID);
    if (itr != m_alivePeers.end())
        itr.value().lastActivity.restart();

    if (m_lastTranSeq[tran.peerID] >= transportHeader.sequence)
        return; // already processed
    m_lastTranSeq[tran.peerID] = transportHeader.sequence;

    if (transportHeader.dstPeers.isEmpty() || transportHeader.dstPeers.contains(m_localPeer.id)) {
#ifdef TRANSACTION_MESSAGE_BUS_DEBUG
        qDebug() << "got transaction " << ApiCommand::toString(tran.command) << "from" << tran.peerID << "transport seq=" << transportHeader.sequence 
                 << "time=" << tran.persistentInfo.timestamp << "db seq=" << tran.persistentInfo.sequence;
#endif
        // process system transactions
        switch(tran.command) {
        case ApiCommand::lockRequest:
        case ApiCommand::lockResponse:
        case ApiCommand::unlockRequest: 
            {
                onGotDistributedMutexTransaction(tran);
                break;
            }
        case ApiCommand::tranSyncRequest:
            onGotTransactionSyncRequest(sender, tran);
            return;
        case ApiCommand::tranSyncResponse:
            onGotTransactionSyncResponse(sender, tran);
            return;
        case ApiCommand::runtimeInfoChanged:
            onGotServerRuntimeInfo(tran, sender->remotePeer().id);
            break;
        case ApiCommand::peerAliveInfo:
            onGotServerAliveInfo(tran, sender->remotePeer().id);
            break;
        case ApiCommand::forcePrimaryTimeServer:
            TimeSynchronizationManager::instance()->primaryTimeServerChanged( tran );
            break;
        case ApiCommand::broadcastPeerSystemTime:
            TimeSynchronizationManager::instance()->peerSystemTimeReceived( tran );
            break;
        default:
            // general transaction
            if (!sender->isReadSync(tran.command))
                return;

            if (!tran.persistentInfo.isNull() && transactionLog && transactionLog->contains(tran))
                return; // already processed
            if (!tran.persistentInfo.isNull() && dbManager)
            {
                QByteArray serializedTran = QnUbjsonTransactionSerializer::instance()->serializedTransaction(tran);
                ErrorCode errorCode = dbManager->executeTransaction( tran, serializedTran );
                if( errorCode != ErrorCode::ok && errorCode != ErrorCode::skipped)
                {
                    qWarning() << "Can't handle transaction" << ApiCommand::toString(tran.command) << "reopen connection";
                    sender->setState(QnTransactionTransport::Error);
                    return;
                }
            }

            if( m_handler )
                m_handler->triggerNotification(tran);

            // this is required to allow client place transactions directly into transaction message bus
            if (tran.command == ApiCommand::getFullInfo)
                sender->setWriteSync(true);
            break;
        }
    }
    else {
#ifdef TRANSACTION_MESSAGE_BUS_DEBUG
        qDebug() << "skip transaction " << ApiCommand::toString(tran.command) << "for peers" << transportHeader.dstPeers;
#endif
    }

    // proxy incoming transaction to other peers.
    if (!transportHeader.dstPeers.isEmpty() && (transportHeader.dstPeers - transportHeader.processedPeers).isEmpty()) {
        emit transactionProcessed(tran);
        return; // all dstPeers already processed
    }

#ifdef TRANSACTION_MESSAGE_BUS_DEBUG
    qDebug() << "proxy transaction " << ApiCommand::toString(tran.command) << " to " << transportHeader.dstPeers;
#endif

    QnPeerSet processedPeers = transportHeader.processedPeers + connectedPeers(tran.command);
    processedPeers << m_localPeer.id;

    for(QnConnectionMap::iterator itr = m_connections.begin(); itr != m_connections.end(); ++itr) 
    {
        QnTransactionTransportPtr transport = *itr;
        if (transportHeader.processedPeers.contains(transport->remotePeer().id) || !transport->isReadyToSend(tran.command)) 
            continue;

        //Q_ASSERT(transport->remotePeer().id != tran.peerID);
        QnTransactionTransportHeader newHeader(processedPeers, transportHeader.dstPeers, transportHeader.sequence);
        transport->sendTransaction(tran, newHeader);
    }

    emit transactionProcessed(tran);
}

void QnTransactionMessageBus::printTranState(const QnTranState& tranState)
{
    for(auto itr = tranState.values.constBegin(); itr != tranState.values.constEnd(); ++itr)
        qDebug() << "key=" << itr.key().peerID << "(dbID=" << itr.key().dbID << ") need after=" << itr.value();
}

void QnTransactionMessageBus::onGotTransactionSyncRequest(QnTransactionTransport* sender, const QnTransaction<QnTranState> &tran)
{
    sender->setWriteSync(true);

    QnTransactionTransportHeader ttUnicast;
    ttUnicast.processedPeers << sender->remotePeer().id << m_localPeer.id;
    ttUnicast.dstPeers << sender->remotePeer().id;

    QnTransactionTransportHeader ttBroadcast(ttUnicast.processedPeers);
    for(auto itr = m_connections.constBegin(); itr != m_connections.constEnd(); ++itr) {
        if (itr.value()->isReadyToSend(ApiCommand::NotDefined))
            ttBroadcast.processedPeers << itr.key(); // dst peer should not proxy transactions to already connected peers
    }

    QList<QByteArray> serializedTransactions;
    const ErrorCode errorCode = transactionLog->getTransactionsAfter(tran.params, serializedTransactions);
    if (errorCode == ErrorCode::ok) 
    {
#ifdef TRANSACTION_MESSAGE_BUS_DEBUG
        qDebug() << "got sync request from peer" << sender->remotePeer().id << ". Need transactions after:";
        printTranState(tran.params);
        qDebug() << "exist " << serializedTransactions.size() << "new transactions";
#endif
        QnTransaction<QnTranStateResponse> tran(ApiCommand::tranSyncResponse);
        tran.params.result = 0;
        QByteArray chunkData;
        
        sender->sendTransaction(tran, ttUnicast);

        sendRuntimeInfo(sender, ttBroadcast); // send as broadcast

        using namespace std::placeholders;
        foreach(const QByteArray& serializedTran, serializedTransactions)
            if(!handleTransaction(serializedTran, std::bind(SendTransactionToTransportFuction(), this, _1, sender, ttBroadcast)))
                sender->setState(QnTransactionTransport::Error);
        return;
    }
    else {
        qWarning() << "Can't execute query for sync with server peer!";
    }
}      

void QnTransactionMessageBus::queueSyncRequest(QnTransactionTransport* transport)
{
    // send sync request
    transport->setReadSync(false);
    QnTransaction<QnTranState> requestTran(ApiCommand::tranSyncRequest);
    requestTran.params = transactionLog->getTransactionsState();
    transport->sendTransaction(requestTran, QnPeerSet() << transport->remotePeer().id << m_localPeer.id);
}

bool QnTransactionMessageBus::doHandshake(QnTransactionTransport* transport)
{
    /** Send all data to the client peers on the connect. */
    QnPeerSet processedPeers = QnPeerSet() << transport->remotePeer().id << m_localPeer.id;
    if (transport->remotePeer().peerType == Qn::PT_DesktopClient || 
        transport->remotePeer().peerType == Qn::PT_VideowallClient)     //TODO: #GDM #VW do not send fullInfo, just required part of it
    {
        /** Request all data to be sent to the client peers on the connect. */
        QnTransaction<ApiFullInfoData> tran;
        tran.command = ApiCommand::getFullInfo;
        tran.peerID = m_localPeer.id;
        if (dbManager->doQuery(nullptr, tran.params) != ErrorCode::ok) {
            qWarning() << "Can't execute query for sync with client peer!";
            return false;
        }

        QnTransaction<ApiModuleDataList> tranModules = prepareModulesDataTransaction();
        tranModules.peerID = m_localPeer.id;

        transport->setWriteSync(true);
        sendRuntimeInfo(transport, processedPeers);
        transport->sendTransaction(tran, processedPeers);
        transport->sendTransaction(tranModules, processedPeers);
        transport->setReadSync(true);
    } else if (transport->remotePeer().peerType == Qn::PT_MobileClient) {
        /** Request all data to be sent to the client peers on the connect. */
        QnTransaction<ApiMediaServerDataList> tranServers;
        tranServers.command = ApiCommand::getMediaServers;
        tranServers.peerID = m_localPeer.id;
        if (dbManager->doQuery(nullptr, tranServers.params) != ErrorCode::ok) {
            qWarning() << "Can't execute query for sync with client peer!";
            return false;
        }

        ec2::ApiCameraDataList cameras;
        if (dbManager->doQuery(QUuid(), cameras) != ErrorCode::ok) {
            qWarning() << "Can't execute query for sync with client peer!";
            return false;
        }
        QnTransaction<ApiCameraDataList> tranCameras;
        tranCameras.command = ApiCommand::getCameras;
        tranCameras.peerID = m_localPeer.id;

        // filter out desktop cameras
        auto desktopCameraResourceType = qnResTypePool->desktopCameraResourceType();
        QUuid desktopCameraTypeId = desktopCameraResourceType ? desktopCameraResourceType->getId() : QUuid();
        if (desktopCameraTypeId.isNull()) {
            tranCameras.params = cameras;
        } else {
            tranCameras.params.reserve(cameras.size());  //usually, there are only a few desktop cameras relatively to total cameras count
            std::copy_if(cameras.cbegin(), cameras.cend(), std::back_inserter(tranCameras.params), [&desktopCameraTypeId](const ec2::ApiCameraData &camera){
                return camera.typeId != desktopCameraTypeId;
            });
        }

        QnTransaction<ApiUserDataList> tranUsers;
        tranUsers.command = ApiCommand::getUsers;
        tranUsers.peerID = m_localPeer.id;
        if (dbManager->doQuery(nullptr, tranUsers.params) != ErrorCode::ok) {
            qWarning() << "Can't execute query for sync with client peer!";
            return false;
        }

        QnTransaction<ApiLayoutDataList> tranLayouts;
        tranLayouts.command = ApiCommand::getLayouts;
        tranLayouts.peerID = m_localPeer.id;
        if (dbManager->doQuery(nullptr, tranLayouts.params) != ErrorCode::ok) {
            qWarning() << "Can't execute query for sync with client peer!";
            return false;
        }

        QnTransaction<ApiCameraServerItemDataList> tranCameraHistory;
        tranCameraHistory.command = ApiCommand::getCameraHistoryItems;
        tranCameraHistory.peerID = m_localPeer.id;
        if (dbManager->doQuery(nullptr, tranCameraHistory.params) != ErrorCode::ok) {
            qWarning() << "Can't execute query for sync with client peer!";
            return false;
        }

        transport->setWriteSync(true);
        sendRuntimeInfo(transport, processedPeers);
        transport->sendTransaction(tranServers,         processedPeers);
        transport->sendTransaction(tranCameras,         processedPeers);
        transport->sendTransaction(tranUsers,           processedPeers);
        transport->sendTransaction(tranLayouts,         processedPeers);
        transport->sendTransaction(tranCameraHistory,   processedPeers);
        transport->setReadSync(true);
    }



    if (!transport->remotePeer().isClient() && !m_localPeer.isClient())
        queueSyncRequest(transport);

    return true;
}

void QnTransactionMessageBus::connectToPeerLost(const QUuid& id)
{
    if (!m_alivePeers.contains(id)) 
        return;    
    
    removeAlivePeer(id, true);
}

void QnTransactionMessageBus::connectToPeerEstablished(const ApiPeerData &peer)
{
    if (m_alivePeers.contains(peer.id)) 
        return;

    addAlivePeerInfo(peer);
    handlePeerAliveChanged(peer, true, true);
}

void QnTransactionMessageBus::handlePeerAliveChanged(const ApiPeerData &peer, bool isAlive, bool sendTran) 
{
    ApiPeerAliveData aliveData;
    aliveData.peer.id = peer.id;
    aliveData.peer.peerType = peer.peerType;
    aliveData.isAlive = isAlive;

    if (sendTran)
    {
        QnTransaction<ApiPeerAliveData> tran(ApiCommand::peerAliveInfo);
        tran.params = aliveData;
        sendTransaction(tran);
#ifdef TRANSACTION_MESSAGE_BUS_DEBUG
        qDebug() << "sending peerAlive info" << peer.id << peer.peerType;
#endif
    }

    if( peer.id == qnCommon->moduleGUID() )
        return; //sending keep-alive

    if (isAlive)
        emit peerFound(aliveData);
    else
        emit peerLost(aliveData);
}

void QnTransactionMessageBus::sendConnectionsData()
{
    if (!QnRouter::instance())
        return;

    QnTransaction<ApiConnectionDataList> transaction(ApiCommand::availableConnections);

    QMultiHash<QUuid, QnRouter::Endpoint> connections = QnRouter::instance()->connections();
    for (auto it = connections.begin(); it != connections.end(); ++it) {
        ApiConnectionData connection;
        connection.discovererId = it.key();
        connection.peerId = it->id;
        connection.host = it->host;
        connection.port = it->port;
        transaction.params.push_back(connection);
    }

    sendTransaction(transaction);
}

QnTransaction<ApiModuleDataList> QnTransactionMessageBus::prepareModulesDataTransaction() const {
    QnTransaction<ApiModuleDataList> transaction(ApiCommand::moduleInfoList);

    foreach (const QnModuleInformation &moduleInformation, QnGlobalModuleFinder::instance()->foundModules()) {
        ApiModuleData data;
        QnGlobalModuleFinder::fillApiModuleData(moduleInformation, &data);
        data.isAlive = true;
        data.discoverers = QnGlobalModuleFinder::instance()->discoverers(data.id);
        transaction.params.push_back(data);
    }

    return transaction;
}

void QnTransactionMessageBus::sendModulesData()
{
    if (!QnGlobalModuleFinder::instance())
        return;

    QnTransaction<ApiModuleDataList> transaction = prepareModulesDataTransaction();
    sendTransaction(transaction);
}

//TODO #ak use SocketAddress instead of this function. It will reduce QString instanciation count
static QString getUrlAddr(const QUrl& url) { return url.host() + QString::number(url.port()); }

bool QnTransactionMessageBus::isPeerUsing(const QUrl& url)
{
    QString addr1 = getUrlAddr(url);
    for (int i = 0; i < m_connectingConnections.size(); ++i)
    {
        QString addr2 = getUrlAddr(m_connectingConnections[i]->remoteAddr());
        if (addr2 == addr1)
            return true;
    }
    for(QnConnectionMap::iterator itr = m_connections.begin(); itr != m_connections.end(); ++itr) {
        QnTransactionTransportPtr transport =  itr.value();
        if (getUrlAddr(transport->remoteAddr()) == addr1)
            return true;
    }
    return false;
}

void QnTransactionMessageBus::at_stateChanged(QnTransactionTransport::State )
{
    QMutexLocker lock(&m_mutex);
    QnTransactionTransport* transport = (QnTransactionTransport*) sender();
    if (!transport)
        return; // already removed
    

    switch (transport->getState()) 
    {
    case QnTransactionTransport::Error: 
        transport->close();
        break;
    case QnTransactionTransport::Connected:
    {
        bool found = false;
        for (int i = 0; i < m_connectingConnections.size(); ++i) 
        {
            if (m_connectingConnections[i] == transport) {
                Q_ASSERT(!m_connections.contains(transport->remotePeer().id));
                m_connections[transport->remotePeer().id] = m_connectingConnections[i];
                emit newDirectConnectionEstablished( m_connectingConnections[i] );
                m_connectingConnections.removeAt(i);
                found = true;
                break;
            }
        }
        Q_ASSERT(found);
        m_lastTranSeq[transport->remotePeer().id] = 0;
        transport->setState(QnTransactionTransport::ReadyForStreaming);
        // if sync already done or in progress do not send new request
        if (doHandshake(transport))
            connectToPeerEstablished(transport->remotePeer());
        else
            transport->close();
        break;
    }
    case QnTransactionTransport::Closed:
        for (int i = m_connectingConnections.size() -1; i >= 0; --i) 
        {
            QnTransactionTransportPtr transportPtr = m_connectingConnections[i];
            if (transportPtr == transport) {
                m_connectingConnections.removeAt(i);
                return;
            }
        }

        for(QnConnectionMap::iterator itr = m_connections.begin(); itr != m_connections.end(); ++itr)
        {
            QnTransactionTransportPtr transportPtr = itr.value();
            if (transportPtr == transport) {
                connectToPeerLost(transport->remotePeer().id);
                m_connections.erase(itr);
                break;
            }
        }

        break;

    default:
        break;
    }
}

void QnTransactionMessageBus::at_timer()
{
    doPeriodicTasks();
}

void QnTransactionMessageBus::doPeriodicTasks()
{
    QMutexLocker lock(&m_mutex);

    // add new outgoing connections
    qint64 currentTime = qnSyncTime->currentMSecsSinceEpoch();
    for (QMap<QUrl, RemoteUrlConnectInfo>::iterator itr = m_remoteUrls.begin(); itr != m_remoteUrls.end(); ++itr)
    {
        const QUrl& url = itr.key();
        if (currentTime - itr.value().lastConnectedTime > RECONNECT_TIMEOUT && !isPeerUsing(url)) 
        {
            if (m_connections.contains(itr.value().peer))
                continue;

            itr.value().lastConnectedTime = currentTime;
            QnTransactionTransportPtr transport(new QnTransactionTransport(m_localPeer));
            connect(transport.data(), &QnTransactionTransport::gotTransaction, this, &QnTransactionMessageBus::at_gotTransaction,  Qt::QueuedConnection);
            connect(transport.data(), &QnTransactionTransport::stateChanged, this, &QnTransactionMessageBus::at_stateChanged,  Qt::QueuedConnection);
            transport->doOutgoingConnect(url);
            m_connectingConnections << transport;
        }
    }

    // send keep-alive if we connected to cloud
    if( !m_aliveSendTimer.isValid() )
        m_aliveSendTimer.restart();
    if (m_aliveSendTimer.elapsed() > ALIVE_UPDATE_INTERVAL) {
        m_aliveSendTimer.restart();
        handlePeerAliveChanged(m_localPeer, true, true);
#ifdef TRANSACTION_MESSAGE_BUS_DEBUG
    qDebug() << "Current transaction state:";
    if (transactionLog)
        printTranState(transactionLog->getTransactionsState());
#endif
    }

    // check if some server not accessible any more
    for (AlivePeersMap::iterator itr = m_alivePeers.begin(); itr != m_alivePeers.end(); ++itr)
    {
        if (itr.value().lastActivity.elapsed() > ALIVE_UPDATE_INTERVAL * 2)
        {
            itr.value().lastActivity.restart();
            foreach(QSharedPointer<QnTransactionTransport> transport, m_connectingConnections) {
                if (transport->remotePeer().id == itr.key()) {
                    qWarning() << "No alive info during timeout. reconnect to peer" << transport->remotePeer().id;
                    transport->setState(QnTransactionTransport::Error);
                }
            }

            foreach(QSharedPointer<QnTransactionTransport> transport, m_connections.values()) {
                if (transport->remotePeer().id == itr.key() && transport->remotePeer().peerType == Qn::PT_Server) {
                    qWarning() << "No alive info during timeout. reconnect to peer" << transport->remotePeer().id;
                    transport->setState(QnTransactionTransport::Error);
                }
            }
        }
    }
}

void QnTransactionMessageBus::sendRuntimeInfo(QnTransactionTransport* transport, const QnTransactionTransportHeader& transportHeader)
{
    foreach (const QnPeerRuntimeInfo &info, QnRuntimeInfoManager::instance()->items()->getItems())
    {
        QnTransaction<ApiRuntimeData> tran(ApiCommand::runtimeInfoChanged);
        tran.params = info.data;
        transport->sendTransaction(tran, transportHeader);
    }
}

void QnTransactionMessageBus::gotConnectionFromRemotePeer(const QSharedPointer<AbstractStreamSocket>& socket, const ApiPeerData &remotePeer)
{
    if (!dbManager)
    {
        qWarning() << "This peer connected to remote Server. Ignoring incoming connection";
        return;
    }

    QnTransactionTransportPtr transport(new QnTransactionTransport(m_localPeer, remotePeer, socket));
    connect(transport.data(), &QnTransactionTransport::gotTransaction, this, &QnTransactionMessageBus::at_gotTransaction,  Qt::QueuedConnection);
    connect(transport.data(), &QnTransactionTransport::stateChanged, this, &QnTransactionMessageBus::at_stateChanged,  Qt::QueuedConnection);

    QMutexLocker lock(&m_mutex);
    m_connectingConnections << transport;
    transport->setState(QnTransactionTransport::Connected);
    Q_ASSERT(!m_connections.contains(remotePeer.id));
}

void QnTransactionMessageBus::addConnectionToPeer(const QUrl& url, const QUuid& peer)
{
    QMutexLocker lock(&m_mutex);
    m_remoteUrls.insert(url, RemoteUrlConnectInfo(peer));
    QTimer::singleShot(0, this, SLOT(doPeriodicTasks()));
}

void QnTransactionMessageBus::removeConnectionFromPeer(const QUrl& url)
{
    QMutexLocker lock(&m_mutex);
    m_remoteUrls.remove(url);
    QString urlStr = getUrlAddr(url);
    foreach(QnTransactionTransportPtr transport, m_connections.values())
    {
        if (getUrlAddr(transport->remoteAddr()) == urlStr) {
            qWarning() << "Disconnected from peer" << url;
            transport->setState(QnTransactionTransport::Error);
        }
    }
}

void QnTransactionMessageBus::dropConnections()
{
    QMutexLocker lock(&m_mutex);
    m_remoteUrls.clear();
    foreach(const QnTransactionTransportPtr &transport, m_connections) {
        qWarning() << "Disconnected from peer" << transport->remoteAddr();
        transport->setState(QnTransactionTransport::Error);
    }
}

QnTransactionMessageBus::AlivePeersMap QnTransactionMessageBus::alivePeers() const
{
    QMutexLocker lock(&m_mutex);
    return m_alivePeers;
}

QnPeerSet QnTransactionMessageBus::connectedPeers(ApiCommand::Value command) const
{
    QnPeerSet result;
    for(QnConnectionMap::const_iterator itr = m_connections.begin(); itr != m_connections.end(); ++itr)
    {
        QnTransactionTransportPtr transport = *itr;
        if (transport->isReadyToSend(command))
            result << transport->remotePeer().id;
    }

    return result;
}

QnTransactionMessageBus::AlivePeersMap QnTransactionMessageBus::aliveServerPeers() const
{
    QMutexLocker lock(&m_mutex);
    AlivePeersMap result;
    for(AlivePeersMap::const_iterator itr = m_alivePeers.begin(); itr != m_alivePeers.end(); ++itr)
    {
        if (itr->peer.isClient())
            continue;

        result.insert(itr.key(), itr.value());
    }
    
    return result;
}

void QnTransactionMessageBus::setLocalPeer(const ApiPeerData localPeer) {
    m_localPeer = localPeer;
}

ec2::ApiPeerData QnTransactionMessageBus::localPeer() const {
    return m_localPeer;
}

void QnTransactionMessageBus::sendTransaction(const QnTransaction<ec2::ApiVideowallControlMessageData>& tran, const QnPeerSet& dstPeers /*= QnPeerSet()*/)
{
    //TODO: #GDM #VW fill dstPeers based on m_alivePeers info
    Q_ASSERT(tran.command == ApiCommand::videowallControl);
    QMutexLocker lock(&m_mutex);
    sendTransactionInternal(tran, QnTransactionTransportHeader(connectedPeers(tran.command) << m_localPeer.id, dstPeers));
}

}
