
#include "transaction_message_bus.h"

#include <QtCore/QTimer>
#include <QTextStream>

#include "remote_ec_connection.h"
#include "utils/common/systemerror.h"
#include "ec2_connection.h"
#include "common/common_module.h"

#include <transaction/transaction_transport.h>

#include "api/app_server_connection.h"
#include "api/runtime_info_manager.h"
#include "nx_ec/data/api_server_alive_data.h"
#include "utils/common/log.h"
#include "utils/common/synctime.h"
#include "utils/network/global_module_finder.h"
#include "utils/network/router.h"
#include "nx_ec/data/api_server_alive_data.h"
#include "ec_connection_notification_manager.h"
#include "nx_ec/data/api_camera_data.h"
#include "nx_ec/data/api_camera_data_ex.h"
#include "nx_ec/data/api_resource_data.h"
#include "nx_ec/data/api_resource_type_data.h"
#include "managers/time_manager.h"

#include <utils/common/checked_cast.h>
#include "utils/common/warnings.h"

#define TRANSACTION_MESSAGE_BUS_DEBUG

namespace ec2
{

static const int RECONNECT_TIMEOUT = 1000 * 5;
static const int ALIVE_UPDATE_INTERVAL = 1000 * 60;
static const int DISCOVERED_PEER_TIMEOUT = 1000 * 60 * 3;

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

struct SendTransactionToTransportFastFuction {
    bool operator()(QnTransactionMessageBus *bus, Qn::SerializationFormat srcFormat, const QByteArray& serializedTran, QnTransactionTransport *sender, const QnTransactionTransportHeader &transportHeader) const 
    {
        Q_UNUSED(bus)
        return sender->sendSerializedTransaction(srcFormat, serializedTran, transportHeader);
    }
};

typedef std::function<bool (Qn::SerializationFormat, const QByteArray&)> FastFunctionType;

template<class T, class Function>
bool handleTransactionParams(const QByteArray &serializedTransaction, QnUbjsonReader<QByteArray> *stream, const QnAbstractTransaction &abstractTransaction, 
                             Function function, FastFunctionType fastFunction)
{
    if (fastFunction(Qn::UbjsonFormat, serializedTransaction)) {
        return true; // process transaction directly without deserialize
    }

    QnTransaction<T> transaction(abstractTransaction);
    if (!QnUbjson::deserialize(stream, &transaction.params)) 
        return false;
    if (!abstractTransaction.persistentInfo.isNull())
        QnUbjsonTransactionSerializer::instance()->addToCache(abstractTransaction.persistentInfo, serializedTransaction);
    function(transaction);
    return true;
}

template<class Function>
bool handleTransaction(const QByteArray &serializedTransaction, const Function &function, FastFunctionType fastFunction)
{
    QnAbstractTransaction transaction;
    QnUbjsonReader<QByteArray> stream(&serializedTransaction);
    if (!QnUbjson::deserialize(&stream, &transaction)) {
        qnWarning("Ignore bad transaction data. size=%1.", serializedTransaction.size());
        return false;
    }

    switch (transaction.command) {
    case ApiCommand::getFullInfo:           return handleTransactionParams<ApiFullInfoData>         (serializedTransaction, &stream, transaction, function, fastFunction);
    case ApiCommand::setResourceStatus:     return handleTransactionParams<ApiResourceStatusData>(serializedTransaction, &stream, transaction, function, fastFunction);
    case ApiCommand::setResourceParam:      return handleTransactionParams<ApiResourceParamWithRefData>   (serializedTransaction, &stream, transaction, function, fastFunction);
    case ApiCommand::saveResource:          return handleTransactionParams<ApiResourceData>         (serializedTransaction, &stream, transaction, function, fastFunction);
    case ApiCommand::setPanicMode:          return handleTransactionParams<ApiPanicModeData>        (serializedTransaction, &stream, transaction, function, fastFunction);
    case ApiCommand::saveCamera:            return handleTransactionParams<ApiCameraData>           (serializedTransaction, &stream, transaction, function, fastFunction);
    case ApiCommand::saveCameras:           return handleTransactionParams<ApiCameraDataList>       (serializedTransaction, &stream, transaction, function, fastFunction);
    case ApiCommand::saveCameraUserAttributes:
        return handleTransactionParams<ApiCameraAttributesData> (serializedTransaction, &stream, transaction, function, fastFunction);
    case ApiCommand::saveCameraUserAttributesList:
        return handleTransactionParams<ApiCameraAttributesDataList> (serializedTransaction, &stream, transaction, function, fastFunction);
    case ApiCommand::saveServerUserAttributes:
        return handleTransactionParams<ApiMediaServerUserAttributesData> (serializedTransaction, &stream, transaction, function, fastFunction);
    case ApiCommand::saveServerUserAttributesList:
        return handleTransactionParams<ApiMediaServerUserAttributesDataList> (serializedTransaction, &stream, transaction, function, fastFunction);
    case ApiCommand::removeBusinessRule:
    case ApiCommand::removeResource:
    case ApiCommand::removeUser:
    case ApiCommand::removeLayout:
    case ApiCommand::removeVideowall:
    case ApiCommand::removeStorage:
    case ApiCommand::removeCamera:          
    case ApiCommand::removeMediaServer:     return handleTransactionParams<ApiIdData>               (serializedTransaction, &stream, transaction, function, fastFunction);
    case ApiCommand::removeCameraHistoryItem:
    case ApiCommand::addCameraHistoryItem:  return handleTransactionParams<ApiCameraServerItemData> (serializedTransaction, &stream, transaction, function, fastFunction);
    case ApiCommand::saveMediaServer:       return handleTransactionParams<ApiMediaServerData>      (serializedTransaction, &stream, transaction, function, fastFunction);
    case ApiCommand::saveStorage:           return handleTransactionParams<ApiStorageData>          (serializedTransaction, &stream, transaction, function, fastFunction);
    case ApiCommand::saveUser:              return handleTransactionParams<ApiUserData>             (serializedTransaction, &stream, transaction, function, fastFunction);
    case ApiCommand::saveBusinessRule:      return handleTransactionParams<ApiBusinessRuleData>     (serializedTransaction, &stream, transaction, function, fastFunction);
    case ApiCommand::saveLayouts:           return handleTransactionParams<ApiLayoutDataList>       (serializedTransaction, &stream, transaction, function, fastFunction);
    case ApiCommand::saveLayout:            return handleTransactionParams<ApiLayoutData>           (serializedTransaction, &stream, transaction, function, fastFunction);
    case ApiCommand::saveVideowall:         return handleTransactionParams<ApiVideowallData>        (serializedTransaction, &stream, transaction, function, fastFunction);
    case ApiCommand::videowallControl:      return handleTransactionParams<ApiVideowallControlMessageData>(serializedTransaction, &stream, transaction, function, fastFunction);
    case ApiCommand::addStoredFile:
    case ApiCommand::updateStoredFile:      return handleTransactionParams<ApiStoredFileData>       (serializedTransaction, &stream, transaction, function, fastFunction);
    case ApiCommand::removeStoredFile:      return handleTransactionParams<ApiStoredFilePath>       (serializedTransaction, &stream, transaction, function, fastFunction);
    case ApiCommand::broadcastBusinessAction:
    case ApiCommand::execBusinessAction:    return handleTransactionParams<ApiBusinessActionData>   (serializedTransaction, &stream, transaction, function, fastFunction);
    case ApiCommand::addLicenses:           return handleTransactionParams<ApiLicenseDataList>      (serializedTransaction, &stream, transaction, function, fastFunction);
    case ApiCommand::addLicense:            
    case ApiCommand::removeLicense:         return handleTransactionParams<ApiLicenseData>          (serializedTransaction, &stream, transaction, function, fastFunction);
    case ApiCommand::resetBusinessRules:    return handleTransactionParams<ApiResetBusinessRuleData>(serializedTransaction, &stream, transaction, function, fastFunction);
    case ApiCommand::uploadUpdate:          return handleTransactionParams<ApiUpdateUploadData>     (serializedTransaction, &stream, transaction, function, fastFunction);
    case ApiCommand::uploadUpdateResponce:  return handleTransactionParams<ApiUpdateUploadResponceData>(serializedTransaction, &stream, transaction, function, fastFunction);
    case ApiCommand::installUpdate:         return handleTransactionParams<ApiUpdateInstallData>    (serializedTransaction, &stream, transaction, function, fastFunction);
    case ApiCommand::addCameraBookmarkTags:
    case ApiCommand::removeCameraBookmarkTags:
                                            return handleTransactionParams<ApiCameraBookmarkTagDataList>(serializedTransaction, &stream, transaction, function, fastFunction);

    case ApiCommand::moduleInfo:            return handleTransactionParams<ApiModuleData>           (serializedTransaction, &stream, transaction, function, fastFunction);
    case ApiCommand::moduleInfoList:        return handleTransactionParams<ApiModuleDataList>       (serializedTransaction, &stream, transaction, function, fastFunction);

    case ApiCommand::discoverPeer:          return handleTransactionParams<ApiDiscoverPeerData>     (serializedTransaction, &stream, transaction, function, fastFunction);
    case ApiCommand::addDiscoveryInformation:
    case ApiCommand::removeDiscoveryInformation:
                                            return handleTransactionParams<ApiDiscoveryData>        (serializedTransaction, &stream, transaction, function, fastFunction);
    case ApiCommand::getDiscoveryData:      return handleTransactionParams<ApiDiscoveryDataList>    (serializedTransaction, &stream, transaction, function, fastFunction);

    case ApiCommand::changeSystemName:      return handleTransactionParams<ApiSystemNameData>       (serializedTransaction, &stream, transaction, function, fastFunction);

    case ApiCommand::lockRequest:
    case ApiCommand::lockResponse:
    case ApiCommand::unlockRequest:         return handleTransactionParams<ApiLockData>             (serializedTransaction, &stream, transaction, function, fastFunction); 
    case ApiCommand::peerAliveInfo:         return handleTransactionParams<ApiPeerAliveData>        (serializedTransaction, &stream, transaction, function, fastFunction);
    case ApiCommand::tranSyncRequest:       return handleTransactionParams<ApiSyncRequestData>      (serializedTransaction, &stream, transaction, function, fastFunction);
    case ApiCommand::tranSyncResponse:      return handleTransactionParams<QnTranStateResponse>     (serializedTransaction, &stream, transaction, function, fastFunction);
    case ApiCommand::tranSyncDone:          return handleTransactionParams<ApiTranSyncDoneData>     (serializedTransaction, &stream, transaction, function, fastFunction);
    case ApiCommand::runtimeInfoChanged:    return handleTransactionParams<ApiRuntimeData>          (serializedTransaction, &stream, transaction, function, fastFunction);
    case ApiCommand::broadcastPeerSystemTime: return handleTransactionParams<ApiPeerSystemTimeData> (serializedTransaction, &stream, transaction, function, fastFunction);
    case ApiCommand::forcePrimaryTimeServer:  return handleTransactionParams<ApiIdData>             (serializedTransaction, &stream, transaction, function, fastFunction);
    case ApiCommand::getKnownPeersSystemTime: return handleTransactionParams<ApiPeerSystemTimeDataList> (serializedTransaction, &stream, transaction, function, fastFunction);
    case ApiCommand::updatePersistentSequence:          return handleTransactionParams<ApiUpdateSequenceData>         (serializedTransaction, &stream, transaction, function, fastFunction);

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
    m_localPeer(qnCommon->moduleGUID(), qnCommon->runningInstanceGUID(), Qn::PT_Server),
    //m_binaryTranSerializer(new QnBinaryTransactionSerializer()),
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
    connect(QnRuntimeTransactionLog::instance(), &QnRuntimeTransactionLog::runtimeDataUpdated, this, &QnTransactionMessageBus::at_runtimeDataUpdated);
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

void QnTransactionMessageBus::addAlivePeerInfo(ApiPeerData peerData, const QnUuid& gotFromPeer)
{
    AlivePeersMap::iterator itr = m_alivePeers.find(peerData.id);
    if (itr == m_alivePeers.end()) 
        itr = m_alivePeers.insert(peerData.id, peerData);
    AlivePeerInfo& currentValue = itr.value();
    if (gotFromPeer.isNull())
        currentValue.directAccess = true;
    else
        currentValue.proxyVia << gotFromPeer;
}

void QnTransactionMessageBus::removeAlivePeer(const QnUuid& id, bool sendTran, bool isRecursive)
{
    // 1. remove peer from alivePeers map

    m_lastTransportSeq.remove(id);

    runtimeTransactionLog->clearRuntimeData(id);

    auto itr = m_alivePeers.find(id);
    if (itr == m_alivePeers.end())
        return;
    
    handlePeerAliveChanged(itr.value().peer, false, sendTran);
    m_alivePeers.erase(itr);

    // 2. remove peers proxy via current peer
    if (isRecursive)
        return;
    QSet<QnUuid> morePeersToRemove;
    for (auto itr = m_alivePeers.begin(); itr != m_alivePeers.end(); ++itr)
    {
        AlivePeerInfo& otherPeer = itr.value();
        if (otherPeer.proxyVia.contains(id)) {
            otherPeer.proxyVia.remove(id);
            if (otherPeer.proxyVia.isEmpty() && !otherPeer.directAccess) {
                morePeersToRemove << otherPeer.peer.id;
            }
        }
    }
    for(const QnUuid& p: morePeersToRemove)
        removeAlivePeer(p, true, true);
}

void QnTransactionMessageBus::gotAliveData(const ApiPeerAliveData &aliveData, QnTransactionTransport* transport)
{
    QnUuid gotFromPeer;
    if (transport)
        gotFromPeer = transport->remotePeer().id;

#ifdef TRANSACTION_MESSAGE_BUS_DEBUG
    NX_LOG( lit("received peerAlive transaction %1 %2").arg(aliveData.peer.id.toString()).arg(aliveData.peer.peerType), cl_logDEBUG1);
#endif
    if (aliveData.peer.id == m_localPeer.id)
        return; // ignore himself

#if 1
    if (!aliveData.isAlive && !gotFromPeer.isNull()) 
    {
        bool isPeerActuallyAlive = aliveData.peer.id == qnCommon->moduleGUID();
        auto itr = m_connections.find(aliveData.peer.id);
        if (itr != m_connections.end()) 
        {
            QnTransactionTransportPtr transport = itr.value();
            if (transport->getState() == QnTransactionTransport::ReadyForStreaming)
                isPeerActuallyAlive = true;
        }
        if (isPeerActuallyAlive) {
            // ignore incoming offline peer info because we can see that peer online
            QnTransaction<ApiPeerAliveData> tran(ApiCommand::peerAliveInfo);
            tran.params = aliveData;
            tran.params.isAlive = true;
            Q_ASSERT(!aliveData.peer.instanceId.isNull());
            sendTransaction(tran); // resend alive info for that peer
            return; // ignore peer offline transaction
        }
    }
#endif

    // proxy alive info from non-direct connected host
    bool isPeerExist = m_alivePeers.contains(aliveData.peer.id);
    if (aliveData.isAlive) 
    {
        addAlivePeerInfo(ApiPeerData(aliveData.peer.id, aliveData.peer.instanceId, aliveData.peer.peerType), gotFromPeer);
        if (!isPeerExist) 
            emit peerFound(aliveData);
    }
    else 
    {
        if (isPeerExist) {
            removeAlivePeer(aliveData.peer.id, false);
        }
    }
    
    // check sequences
    
    if (aliveData.isAlive) {
        runtimeTransactionLog->clearOldRuntimeData(QnTranStateKey(aliveData.peer.id, aliveData.peer.instanceId));
    }

    if (transport && transport->isSyncDone() && aliveData.isAlive)
    {
        if (!aliveData.persistentState.values.empty() && transactionLog) 
        {
            // check current persistent state
            if (!transactionLog->contains(aliveData.persistentState)) 
            {
#ifdef TRANSACTION_MESSAGE_BUS_DEBUG
                qDebug() << "DETECT transaction GAP via update message. Resync with peer" << transport->remotePeer().id;
                qDebug() << "peer state:";
                printTranState(aliveData.persistentState);
#endif
                if (!transport->remotePeer().isClient() && !m_localPeer.isClient())
                    queueSyncRequest(transport);
                else
                    transport->setState(QnTransactionTransport::Error);
            }
        }

        if (!aliveData.runtimeState.values.empty()) 
        {
            // check current persistent state
            if (!runtimeTransactionLog->contains(aliveData.runtimeState)) {
                qWarning() << "DETECT runtime transaction GAP via update message. Resync with peer" << transport->remotePeer().id;
                if (!transport->remotePeer().isClient() && !m_localPeer.isClient())
                    queueSyncRequest(transport);
                else
                    transport->setState(QnTransactionTransport::Error);
            }
        }

    }
}

void QnTransactionMessageBus::onGotServerAliveInfo(const QnTransaction<ApiPeerAliveData> &tran, QnTransactionTransport* transport, const QnTransactionTransportHeader& ttHeader)
{
    Q_ASSERT(tran.peerID != qnCommon->moduleGUID());

    gotAliveData(tran.params, transport);

    QnTransaction<ApiPeerAliveData> modifiedTran(tran);
    Q_ASSERT(!modifiedTran.params.peer.instanceId.isNull());
    modifiedTran.params.persistentState.values.clear(); // do not proxy persistent state to other peers. this checking required for directly connected peers only
    modifiedTran.params.runtimeState.values.clear();
    proxyTransaction(tran, ttHeader);
}

bool QnTransactionMessageBus::onGotServerRuntimeInfo(const QnTransaction<ApiRuntimeData> &tran, QnTransactionTransport* transport)
{
    if (tran.params.peer.id == qnCommon->moduleGUID())
        return false; // ignore himself

    gotAliveData(ApiPeerAliveData(tran.params.peer, true), transport);
    if (runtimeTransactionLog->contains(tran))
        return false;
    else {
        runtimeTransactionLog->saveTransaction(tran);
        return true;
    }
}

void QnTransactionMessageBus::at_gotTransaction(const QByteArray &serializedTran, const QnTransactionTransportHeader &transportHeader)
{
    QnTransactionTransport* sender = checked_cast<QnTransactionTransport*>(this->sender());
    if( !sender || sender->getState() != QnTransactionTransport::ReadyForStreaming )
    {
        if( sender )
        {
            NX_LOG(lit("Ignoring transaction with seq %1 from peer %2 having state %3").
                arg(transportHeader.sequence).arg(sender->remotePeer().id.toString()).arg(sender->getState()), cl_logDEBUG1);
        }
        else
        {
            NX_LOG(lit("Ignoring transaction with seq %1 from unknown peer").arg(transportHeader.sequence), cl_logDEBUG1);
        }
        return;
    }

    Q_ASSERT(transportHeader.processedPeers.contains(sender->remotePeer().id));

    using namespace std::placeholders;
    if(!handleTransaction(serializedTran, std::bind(GotTransactionFuction(), this, _1, sender, transportHeader), [](Qn::SerializationFormat, const QByteArray& ) { return false; } ))
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
}

void QnTransactionMessageBus::onGotTransactionSyncDone(QnTransactionTransport* sender, const QnTransaction<ApiTranSyncDoneData> &tran) {
    Q_UNUSED(tran)
        sender->setSyncDone(true);
}

template <class T>
void QnTransactionMessageBus::sendTransactionToTransport(const QnTransaction<T> &tran, QnTransactionTransport* transport, const QnTransactionTransportHeader &transportHeader) {
    Q_ASSERT(!tran.isLocal);
    transport->sendTransaction(tran, transportHeader);
}

bool QnTransactionMessageBus::checkSequence(const QnTransactionTransportHeader& transportHeader, const QnAbstractTransaction& tran, QnTransactionTransport* transport)
{
    if (transportHeader.sender.isNull())
        return true; // old version, nothing to check

    // 1. check transport sequence
    int transportSeq = m_lastTransportSeq[transportHeader.sender];
    if (transportSeq >= transportHeader.sequence) {
#ifdef TRANSACTION_MESSAGE_BUS_DEBUG
        NX_LOG( lit("Ignore transaction %1 from peer %2 received via %3 because of transport sequence: %4 <= %5").
            arg(toString(tran.command)).arg(tran.peerID.toString()).arg(transportHeader.sender.toString()).arg(transportHeader.sequence).arg(transportSeq), cl_logDEBUG1 );
#endif
        return false; // already processed
    }
    m_lastTransportSeq[transportHeader.sender] = transportHeader.sequence;

    // 2. check persistent sequence
    if (tran.persistentInfo.isNull() || !transactionLog)
        return true; // nothing to check

    QnTranStateKey persistentKey(tran.peerID, tran.persistentInfo.dbID);
    int persistentSeq = transactionLog->getLatestSequence(persistentKey);

#ifdef TRANSACTION_MESSAGE_BUS_DEBUG
    if (!transport->isSyncDone() && transport->isReadSync(ApiCommand::NotDefined) && transportHeader.sender != transport->remotePeer().id) 
    {
        NX_LOG( lit("Got transcaction from peer %1 while sync with peer %2 in progress").arg(transportHeader.sender.toString()).arg(transport->remotePeer().id.toString()), cl_logWARNING );
    }
#endif

    if (persistentSeq && tran.persistentInfo.sequence > persistentSeq + 1) 
    {
        if (transport->isSyncDone()) 
        {
        // gap in persistent data detect, do resync
#ifdef TRANSACTION_MESSAGE_BUS_DEBUG
            NX_LOG( lit("GAP in persistent data detected! for peer %1 Expected seq=%2, but got seq=%3").
                arg(tran.peerID.toString()).arg(persistentSeq + 1).arg(tran.persistentInfo.sequence), cl_logDEBUG1 );
#endif

            if (!transport->remotePeer().isClient() && !m_localPeer.isClient())
                queueSyncRequest(transport);
            else 
                transport->setState(QnTransactionTransport::Error); // reopen
            return false;
        }
        else {
#ifdef TRANSACTION_MESSAGE_BUS_DEBUG
            NX_LOG( lit("GAP in persistent data, but sync in progress %1. Expected seq=%2, but got seq=%3").
                arg(tran.peerID.toString()).arg(persistentSeq + 1).arg(tran.persistentInfo.sequence), cl_logDEBUG1 );
#endif
        }
    }
    return true;
}

void QnTransactionMessageBus::updatePersistentMarker(const QnTransaction<ApiUpdateSequenceData>& tran, QnTransactionTransport* /*transport*/)
{
    if (transactionLog)
        transactionLog->updateSequence(tran.params);
}

void QnTransactionMessageBus::proxyFillerTransaction(const QnAbstractTransaction& tran, const QnTransactionTransportHeader& transportHeader)
{
    // proxy filler transaction to avoid gaps in the persistent sequence
    QnTransaction<ApiUpdateSequenceData> fillerTran(ApiCommand::updatePersistentSequence);
    ApiSyncMarkerRecord record;
    record.peerID = tran.peerID;
    record.dbID = tran.persistentInfo.dbID;
    record.sequence = tran.persistentInfo.sequence;
    fillerTran.params.markers.push_back(record);
    transactionLog->updateSequence(fillerTran.params);
    proxyTransaction(fillerTran, transportHeader);
}

template <class T>
void QnTransactionMessageBus::gotTransaction(const QnTransaction<T> &tran, QnTransactionTransport* sender, const QnTransactionTransportHeader &transportHeader) 
{
    QMutexLocker lock(&m_mutex);

    AlivePeersMap:: iterator itr = m_alivePeers.find(transportHeader.sender);
    if (itr != m_alivePeers.end())
        itr.value().lastActivity.restart();

    if (!checkSequence(transportHeader, tran, sender))
        return;

    if (!sender->isReadSync(tran.command)) {
#ifdef TRANSACTION_MESSAGE_BUS_DEBUG
        NX_LOG( lit("reject transaction %1 from %2 tt seq=%3 time=%4 db seq=%5 via %6").
            arg(ApiCommand::toString(tran.command)).arg(tran.peerID.toString()).arg(transportHeader.sequence).
            arg(tran.persistentInfo.timestamp).arg(tran.persistentInfo.sequence).arg(sender->remotePeer().id.toString()), cl_logDEBUG1);
#endif
        return;
    }


    if (transportHeader.dstPeers.isEmpty() || transportHeader.dstPeers.contains(m_localPeer.id)) {
#ifdef TRANSACTION_MESSAGE_BUS_DEBUG
        NX_LOG( lit("got transaction %1 from %2 tt seq=%3 time=%4 db seq=%5 via %6").arg(ApiCommand::toString(tran.command)).arg(tran.peerID.toString()).
            arg(transportHeader.sequence).arg(tran.persistentInfo.timestamp).arg(tran.persistentInfo.sequence).arg(sender->remotePeer().id.toString()), cl_logDEBUG1);
#endif
        // process system transactions
        switch(tran.command) {
        case ApiCommand::lockRequest:
        case ApiCommand::lockResponse:
        case ApiCommand::unlockRequest: 
            onGotDistributedMutexTransaction(tran);
            break;
        case ApiCommand::tranSyncRequest:
            onGotTransactionSyncRequest(sender, tran);
            return; // do not proxy
        case ApiCommand::tranSyncResponse:
            onGotTransactionSyncResponse(sender, tran);
            return; // do not proxy
        case ApiCommand::tranSyncDone:
            onGotTransactionSyncDone(sender, tran);
            return; // do not proxy
        case ApiCommand::peerAliveInfo:
            onGotServerAliveInfo(tran, sender, transportHeader);
            return; // do not proxy. this call contains built in proxy
        case ApiCommand::forcePrimaryTimeServer:
            TimeSynchronizationManager::instance()->primaryTimeServerChanged( tran );
            break;
        case ApiCommand::broadcastPeerSystemTime:
            TimeSynchronizationManager::instance()->peerSystemTimeReceived( tran );
            break;
        case ApiCommand::getKnownPeersSystemTime:
            TimeSynchronizationManager::instance()->knownPeersSystemTimeReceived( tran );
            break;
        case ApiCommand::runtimeInfoChanged:
            if (!onGotServerRuntimeInfo(tran, sender))
                return; // already processed. do not proxy and ignore transaction
            if( m_handler )
                m_handler->triggerNotification(tran);
            break;
        case ApiCommand::updatePersistentSequence:
            updatePersistentMarker(tran, sender);
            break;
        default:
            // general transaction
            if (!tran.persistentInfo.isNull() && dbManager)
            {
                QByteArray serializedTran = QnUbjsonTransactionSerializer::instance()->serializedTransaction(tran);
                ErrorCode errorCode = dbManager->executeTransaction( tran, serializedTran );
                switch(errorCode) {
                    case ErrorCode::ok:
                        break;
                    case ErrorCode::containsBecauseTimestamp:
                        proxyFillerTransaction(tran, transportHeader);
                    case ErrorCode::containsBecauseSequence:
                        return; // do not proxy if transaction already exists
                    default:
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
        if( cl_log.logLevel() >= cl_logDEBUG1 )
        {
            QString dstPeersStr;
            for( const QnUuid& peer: transportHeader.dstPeers )
                dstPeersStr += peer.toString();
            NX_LOG( lit("skip transaction %1 for peers %2").arg(ApiCommand::toString(tran.command)).arg(dstPeersStr), cl_logDEBUG1);
        }
#endif
    }

    proxyTransaction(tran, transportHeader);
}

template <class T>
void QnTransactionMessageBus::proxyTransaction(const QnTransaction<T> &tran, const QnTransactionTransportHeader &transportHeader) 
{
    // proxy incoming transaction to other peers.
    if (!transportHeader.dstPeers.isEmpty() && (transportHeader.dstPeers - transportHeader.processedPeers).isEmpty()) {
        emit transactionProcessed(tran);
        return; // all dstPeers already processed
    }

    QnPeerSet processedPeers = transportHeader.processedPeers + connectedPeers(tran.command);
    processedPeers << m_localPeer.id;
    QnTransactionTransportHeader newHeader(processedPeers, transportHeader.dstPeers);
    newHeader.sequence = transportHeader.sequence;
    newHeader.sender = transportHeader.sender;

#ifdef TRANSACTION_MESSAGE_BUS_DEBUG
    QSet<QnUuid> proxyList;
#endif
    for(QnConnectionMap::iterator itr = m_connections.begin(); itr != m_connections.end(); ++itr) 
    {
        QnTransactionTransportPtr transport = *itr;
        if (transportHeader.processedPeers.contains(transport->remotePeer().id) || !transport->isReadyToSend(tran.command)) 
            continue;

        //Q_ASSERT(transport->remotePeer().id != tran.peerID);
        transport->sendTransaction(tran, newHeader);
#ifdef TRANSACTION_MESSAGE_BUS_DEBUG
        proxyList << transport->remotePeer().id;
#endif
    }

#ifdef TRANSACTION_MESSAGE_BUS_DEBUG
    if (!proxyList.isEmpty())
    {
        if( cl_log.logLevel() >= cl_logDEBUG1 )
        {
            QString proxyListStr;
            for( const QnUuid& peer: proxyList )
                proxyListStr += " " + peer.toString();
            NX_LOG( lit("proxy transaction %1 to (%2)").arg(ApiCommand::toString(tran.command)).arg(proxyListStr), cl_logDEBUG1);
        }
    }
#endif

    emit transactionProcessed(tran);
};

void QnTransactionMessageBus::printTranState(const QnTranState& tranState)
{
    for(auto itr = tranState.values.constBegin(); itr != tranState.values.constEnd(); ++itr)
        qDebug() << "key=" << itr.key().peerID << "(dbID=" << itr.key().dbID << ") need after=" << itr.value();
}

void QnTransactionMessageBus::onGotTransactionSyncRequest(QnTransactionTransport* sender, const QnTransaction<ApiSyncRequestData> &tran)
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
    const ErrorCode errorCode = transactionLog->getTransactionsAfter(tran.params.persistentState, serializedTransactions);
    if (errorCode == ErrorCode::ok) 
    {
#ifdef TRANSACTION_MESSAGE_BUS_DEBUG
        NX_LOG( lit("got sync request from peer %1. Need transactions after:").arg(sender->remotePeer().id.toString()), cl_logDEBUG1);
        printTranState(tran.params.persistentState);
        NX_LOG( lit("exist %1 new transactions").arg(serializedTransactions.size()), cl_logDEBUG1);
#endif
        QnTransaction<QnTranStateResponse> tranSyncResponse(ApiCommand::tranSyncResponse);
        tranSyncResponse.params.result = 0;
        sender->sendTransaction(tranSyncResponse, ttUnicast);

        sendRuntimeInfo(sender, ttBroadcast, tran.params.runtimeState); // send as broadcast

        using namespace std::placeholders;
        for(const QByteArray& serializedTran: serializedTransactions)
            if(!handleTransaction(serializedTran, 
                                  std::bind(SendTransactionToTransportFuction(), this, _1, sender, ttBroadcast), 
                                  std::bind(SendTransactionToTransportFastFuction(), this, _1, _2, sender, ttBroadcast)))
                                  sender->setState(QnTransactionTransport::Error);

        QnTransaction<ApiTranSyncDoneData> tranSyncDone(ApiCommand::tranSyncDone);
        tranSyncResponse.params.result = 0;
        sender->sendTransaction(tranSyncDone, ttUnicast);

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
    transport->setSyncDone(false);
    QnTransaction<ApiSyncRequestData> requestTran(ApiCommand::tranSyncRequest);
    requestTran.params.persistentState = transactionLog->getTransactionsState();
    requestTran.params.runtimeState = runtimeTransactionLog->getTransactionsState();
    transport->sendTransaction(requestTran, QnPeerSet() << transport->remotePeer().id << m_localPeer.id);
}

bool QnTransactionMessageBus::sendInitialData(QnTransactionTransport* transport)
{
    /** Send all data to the client peers on the connect. */
    QnPeerSet processedPeers = QnPeerSet() << transport->remotePeer().id << m_localPeer.id;
    if (m_localPeer.isClient()) {
        transport->setWriteSync(true);
        sendRuntimeInfo(transport, processedPeers, QnTranState());
        transport->setReadSync(true);
    }
    else if (transport->remotePeer().peerType == Qn::PT_DesktopClient || 
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

        transport->setWriteSync(true);
        sendRuntimeInfo(transport, processedPeers, QnTranState());
        transport->sendTransaction(tran, processedPeers);
        transport->setReadSync(true);

        //sending local time information on known servers
        TimeSynchronizationManager* timeManager = TimeSynchronizationManager::instance();
        if( timeManager )
        {
            QnTransaction<ApiPeerSystemTimeDataList> tran;
            tran.params = timeManager->getKnownPeersSystemTime();
            tran.command = ApiCommand::getKnownPeersSystemTime;
            tran.peerID = m_localPeer.id;
            transport->sendTransaction(tran, processedPeers);
        }
    } else if (transport->remotePeer().peerType == Qn::PT_MobileClient) {
        /** Request all data to be sent to the client peers on the connect. */
        QnTransaction<ApiMediaServerDataExList> tranServers;
        tranServers.command = ApiCommand::getMediaServersEx;
        tranServers.peerID = m_localPeer.id;
        if (dbManager->doQuery(nullptr, tranServers.params) != ErrorCode::ok) {
            qWarning() << "Can't execute query for sync with client peer!";
            return false;
        }

        ec2::ApiCameraDataExList cameras;
        if (dbManager->doQuery(QnUuid(), cameras) != ErrorCode::ok) {
            qWarning() << "Can't execute query for sync with client peer!";
            return false;
        }
        QnTransaction<ApiCameraDataExList> tranCameras;
        tranCameras.command = ApiCommand::getCamerasEx;
        tranCameras.peerID = m_localPeer.id;

        // filter out desktop cameras
        auto desktopCameraResourceType = qnResTypePool->desktopCameraResourceType();
        QnUuid desktopCameraTypeId = desktopCameraResourceType ? desktopCameraResourceType->getId() : QnUuid();
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
        sendRuntimeInfo(transport, processedPeers, QnTranState());
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

void QnTransactionMessageBus::connectToPeerLost(const QnTransactionTransport* transport)
{
    if (m_alivePeers.contains(transport->remotePeer().id))
        removeAlivePeer(transport->remotePeer().id, true);
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
    ApiPeerAliveData aliveData(peer, isAlive);

    if (sendTran)
    {
        QnTransaction<ApiPeerAliveData> tran(ApiCommand::peerAliveInfo);
        tran.params = aliveData;
        Q_ASSERT(!tran.params.peer.instanceId.isNull());
        if (isAlive && transactionLog && peer.id == m_localPeer.id) {
            tran.params.persistentState = transactionLog->getTransactionsState();
            tran.params.runtimeState = runtimeTransactionLog->getTransactionsState();
        }
        sendTransaction(tran);
#ifdef TRANSACTION_MESSAGE_BUS_DEBUG
        NX_LOG( lit("sending peerAlive info %1 %2").arg(peer.id.toString()).arg(peer.peerType), cl_logDEBUG1);
#endif
    }

    if( peer.id == qnCommon->moduleGUID() )
        return; //sending keep-alive

    if (isAlive)
        emit peerFound(aliveData);
    else
        emit peerLost(aliveData);
}

QnTransaction<ApiModuleDataList> QnTransactionMessageBus::prepareModulesDataTransaction() const {
    QnTransaction<ApiModuleDataList> transaction(ApiCommand::moduleInfoList);

    for(const QnModuleInformation &moduleInformation: QnGlobalModuleFinder::instance()->foundModules()) {
        ApiModuleData data;
        QnGlobalModuleFinder::fillApiModuleData(moduleInformation, &data);
        data.isAlive = true;
        data.discoverers = QnGlobalModuleFinder::instance()->discoverers(data.id).toList();
        transaction.params.push_back(data);
    }
    transaction.peerID = m_localPeer.id;
    return transaction;
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
        m_lastTransportSeq.remove(transport->remotePeer().id);
        transport->setState(QnTransactionTransport::ReadyForStreaming);

        transport->processExtraData();
        transport->startListening();

        runtimeTransactionLog->clearOldRuntimeData(QnTranStateKey(transport->remotePeer().id, transport->remotePeer().instanceId));
        if (sendInitialData(transport))
            connectToPeerEstablished(transport->remotePeer());
        else
            transport->close();
        break;
    }
    case QnTransactionTransport::ReadyForStreaming:
        break;
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
                connectToPeerLost(transport);
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

void QnTransactionMessageBus::at_peerIdDiscovered(const QUrl& url, const QnUuid& id)
{
    QMutexLocker lock(&m_mutex);
    auto itr = m_remoteUrls.find(url);
    if (itr != m_remoteUrls.end()) {
        itr.value().discoveredTimeout.restart();
        itr.value().discoveredPeer = id;
    }
}

void QnTransactionMessageBus::doPeriodicTasks()
{
    QMutexLocker lock(&m_mutex);

    // send HTTP level keep alive (empty chunk) for server <---> server connections
    if (!m_localPeer.isClient()) 
    {
        for(const QnTransactionTransportPtr& transport: m_connections.values()) 
        {
            if (transport->getState() == QnTransactionTransport::ReadyForStreaming && !transport->remotePeer().isClient()) 
            {
                if (transport->isHttpKeepAliveTimeout()) {
                    qWarning() << "Transaction Transport HTTP keep-alive timeout for connection" << transport->remotePeer().id;
                    transport->setState(QnTransactionTransport::Error);
                }
            }
        }
    }

    // add new outgoing connections
    for (QMap<QUrl, RemoteUrlConnectInfo>::iterator itr = m_remoteUrls.begin(); itr != m_remoteUrls.end(); ++itr)
    {
        const QUrl& url = itr.key();
        RemoteUrlConnectInfo& connectInfo = itr.value();
        bool isTimeout = !connectInfo.lastConnectedTime.isValid() || connectInfo.lastConnectedTime.hasExpired(RECONNECT_TIMEOUT);
        if (isTimeout && !isPeerUsing(url)) 
        {
            if (!connectInfo.discoveredPeer.isNull() ) 
            {
                if (connectInfo.discoveredTimeout.elapsed() > DISCOVERED_PEER_TIMEOUT) {
                    connectInfo.discoveredPeer = QnUuid();
                    connectInfo.discoveredTimeout.restart();
                }
                else if (m_connections.contains(connectInfo.discoveredPeer))
                    continue;
            }

            itr.value().lastConnectedTime.restart();
            QnTransactionTransportPtr transport(new QnTransactionTransport(m_localPeer));
            connect(transport.data(), &QnTransactionTransport::gotTransaction, this, &QnTransactionMessageBus::at_gotTransaction,  Qt::QueuedConnection);
            connect(transport.data(), &QnTransactionTransport::stateChanged, this, &QnTransactionMessageBus::at_stateChanged,  Qt::QueuedConnection);
            connect(transport.data(), &QnTransactionTransport::remotePeerUnauthorized, this, &QnTransactionMessageBus::remotePeerUnauthorized,  Qt::DirectConnection);
            connect(transport.data(), &QnTransactionTransport::peerIdDiscovered, this, &QnTransactionMessageBus::at_peerIdDiscovered,  Qt::QueuedConnection);
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
    NX_LOG( "Current transaction state:", cl_logDEBUG1 );
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
            for(const QnTransactionTransportPtr& transport: m_connectingConnections) {
                if (transport->getState() == QnTransactionTransport::Closed)
                    continue; // it's going to close soon
                if (transport->remotePeer().id == itr.key()) {
                    qWarning() << "No alive info during timeout. reconnect to peer" << transport->remotePeer().id;
                    transport->setState(QnTransactionTransport::Error);
                }
            }

            for(const QnTransactionTransportPtr& transport: m_connections.values()) {
                if (transport->getState() == QnTransactionTransport::Closed)
                    continue; // it's going to close soon
                if (transport->remotePeer().id == itr.key() && transport->remotePeer().peerType == Qn::PT_Server) {
                    qWarning() << "No alive info during timeout. reconnect to peer" << transport->remotePeer().id;
                    transport->setState(QnTransactionTransport::Error);
                }
            }
        }
    }
}

void QnTransactionMessageBus::sendRuntimeInfo(QnTransactionTransport* transport, const QnTransactionTransportHeader& transportHeader, const QnTranState& runtimeState)
{
    QList<QnTransaction<ApiRuntimeData>> result;
    runtimeTransactionLog->getTransactionsAfter(runtimeState, result);
    for(const QnTransaction<ApiRuntimeData> &tran: result)
        transport->sendTransaction(tran, transportHeader);
    transport->sendTransaction(prepareModulesDataTransaction(), transportHeader);
}

void QnTransactionMessageBus::gotConnectionFromRemotePeer(const QSharedPointer<AbstractStreamSocket>& socket, const ApiPeerData &remotePeer)
{
    if (!dbManager)
    {
        qWarning() << "This peer connected to remote Server. Ignoring incoming connection";
        return;
    }

    QnTransactionTransportPtr transport(new QnTransactionTransport(m_localPeer, socket));
    transport->setRemotePeer(remotePeer);
    connect(transport.data(), &QnTransactionTransport::gotTransaction, this, &QnTransactionMessageBus::at_gotTransaction,  Qt::QueuedConnection);
    connect(transport.data(), &QnTransactionTransport::stateChanged, this, &QnTransactionMessageBus::at_stateChanged,  Qt::QueuedConnection);
    connect(transport.data(), &QnTransactionTransport::remotePeerUnauthorized, this, &QnTransactionMessageBus::remotePeerUnauthorized,  Qt::DirectConnection);

    QMutexLocker lock(&m_mutex);
    m_connectingConnections << transport;
    transport->setState(QnTransactionTransport::Connected);
    Q_ASSERT(!m_connections.contains(remotePeer.id));
}

void QnTransactionMessageBus::addConnectionToPeer(const QUrl& url)
{
    QMutexLocker lock(&m_mutex);
    if (!m_remoteUrls.contains(url)) {
        m_remoteUrls.insert(url, RemoteUrlConnectInfo());
        QTimer::singleShot(0, this, SLOT(doPeriodicTasks()));
    }
}

void QnTransactionMessageBus::removeConnectionFromPeer(const QUrl& url)
{
    QMutexLocker lock(&m_mutex);
    m_remoteUrls.remove(url);
    QString urlStr = getUrlAddr(url);
    for(const QnTransactionTransportPtr& transport: m_connections.values())
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
    for(const QnTransactionTransportPtr &transport: m_connections) {
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

void QnTransactionMessageBus::at_runtimeDataUpdated(const QnTransaction<ApiRuntimeData>& tran)
{
    // data was changed by local transaction log (old data instance for same peer was removed), emit notification to apply new data version outside
    if( m_handler )
        m_handler->triggerNotification(tran);
}

}

