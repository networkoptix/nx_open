
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
#include "transaction/runtime_transaction_log.h"


namespace ec2
{

static const int RECONNECT_TIMEOUT = 1000 * 5;
static const int ALIVE_UPDATE_INTERVAL = 1000 * 60;
static const int ALIVE_UPDATE_TIMEOUT = ALIVE_UPDATE_INTERVAL*2 + 10*1000;
static const int DISCOVERED_PEER_TIMEOUT = 1000 * 60 * 3;

QString printTransaction(const char* prefix, const QnAbstractTransaction& tran, const QnTransactionTransportHeader &transportHeader, QnTransactionTransport* sender)
{
    return lit("%1 %2 %3 gotVia=%4").arg(prefix).arg(tran.toString()).arg(toString(transportHeader)).arg(sender->remotePeer().id.toString());
}

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
    if (!QnUbjson::deserialize(stream, &transaction.params)) {
        qWarning() << "Can't deserialize transaction " << toString(abstractTransaction.command);
        return false;
    }
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
    case ApiCommand::markLicenseOverflow:     return handleTransactionParams<ApiLicenseOverflowData>         (serializedTransaction, &stream, transaction, function, fastFunction);
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

QnTransactionMessageBus::QnTransactionMessageBus(Qn::PeerType peerType)
: 
    m_localPeer(qnCommon->moduleGUID(), qnCommon->runningInstanceGUID(), peerType),
    //m_binaryTranSerializer(new QnBinaryTransactionSerializer()),
    m_jsonTranSerializer(new QnJsonTransactionSerializer()),
    m_ubjsonTranSerializer(new QnUbjsonTransactionSerializer()),
	m_handler(nullptr),
    m_timer(nullptr), 
    m_mutex(QMutex::Recursive),
    m_thread(nullptr),
    m_runtimeTransactionLog(new QnRuntimeTransactionLog())
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
    connect(m_runtimeTransactionLog.get(), &QnRuntimeTransactionLog::runtimeDataUpdated, this, &QnTransactionMessageBus::at_runtimeDataUpdated);
}

void QnTransactionMessageBus::start()
{
    if (!m_thread->isRunning())
        m_thread->start();
}

void QnTransactionMessageBus::stop()
{
    dropConnections();

    m_thread->exit();
    m_thread->wait();

    m_aliveSendTimer.invalidate();
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

void QnTransactionMessageBus::removeTTSequenceForPeer(const QnUuid& id)
{
    NX_LOG( QnLog::EC2_TRAN_LOG, lit("Clear transportSequence for peer %1").arg(id.toString()), cl_logDEBUG1);

    QnTranStateKey key(id, QnUuid());
    auto itr = m_lastTransportSeq.lowerBound(key);
    while (itr != m_lastTransportSeq.end() && itr.key().peerID == id)
        itr = m_lastTransportSeq.erase(itr);
}


void QnTransactionMessageBus::removeAlivePeer(const QnUuid& id, bool sendTran, bool isRecursive)
{
    // 1. remove peer from alivePeers map
    removeTTSequenceForPeer(id);

    m_runtimeTransactionLog->clearRuntimeData(id);

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

bool QnTransactionMessageBus::gotAliveData(const ApiPeerAliveData &aliveData, QnTransactionTransport* transport)
{
    QnUuid gotFromPeer;
    if (transport)
        gotFromPeer = transport->remotePeer().id;

    NX_LOG( QnLog::EC2_TRAN_LOG, lit("received peerAlive transaction. id=%1 type=%2 isAlive=%3").
        arg(aliveData.peer.id.toString()).arg(aliveData.peer.peerType).arg(aliveData.isAlive), cl_logDEBUG1);
    if (aliveData.peer.id == m_localPeer.id)
        return false; // ignore himself

#if 1
    if (!aliveData.isAlive && !gotFromPeer.isNull()) 
    {
        bool isPeerActuallyAlive = aliveData.peer.id == qnCommon->moduleGUID();
        auto itr = m_connections.find(aliveData.peer.id);
        if (itr != m_connections.end()) 
        {
            QnTransactionTransport* transport = itr.value();
            if (transport->getState() == QnTransactionTransport::ReadyForStreaming)
                isPeerActuallyAlive = true;
        }
        if (isPeerActuallyAlive) {
            // ignore incoming offline peer info because we can see that peer online
            QnTransaction<ApiPeerAliveData> tran(ApiCommand::peerAliveInfo);
            tran.params = aliveData;
            tran.params.isAlive = true;
            Q_ASSERT(!aliveData.peer.instanceId.isNull());
            sendTransaction(tran); // resend broadcast alive info for that peer
            return false; // ignore peer offline transaction
        }
    }
#endif

    // proxy alive info from non-direct connected host
    bool isPeerExist = m_alivePeers.contains(aliveData.peer.id);
    if (aliveData.isAlive) 
    {
        addAlivePeerInfo(ApiPeerData(aliveData.peer.id, aliveData.peer.instanceId, aliveData.peer.peerType), gotFromPeer);
        if (!isPeerExist) 
        {
            NX_LOG( QnLog::EC2_TRAN_LOG, lit("emit peerFound. id=%1").arg(aliveData.peer.id.toString()), cl_logDEBUG1);
            QMutexLocker lk( &m_signalEmitMutex );
            emit peerFound(aliveData);
        }
    }
    else 
    {
        if (isPeerExist) {
            removeAlivePeer(aliveData.peer.id, false);
        }
    }
    
    // check sequences
    
    if (aliveData.isAlive) {
        m_runtimeTransactionLog->clearOldRuntimeData(QnTranStateKey(aliveData.peer.id, aliveData.peer.instanceId));
    }

    if (transport && transport->isSyncDone() && aliveData.isAlive)
    {
        if (!aliveData.persistentState.values.empty() && transactionLog) 
        {
            // check current persistent state
            if (!transactionLog->contains(aliveData.persistentState)) 
            {
                NX_LOG( QnLog::EC2_TRAN_LOG, lit("DETECT transaction GAP via update message. Resync with peer %1").
                    arg(transport->remotePeer().id.toString()), cl_logDEBUG1 );
                NX_LOG( QnLog::EC2_TRAN_LOG, lit("peer state:"), cl_logDEBUG1 );
                printTranState(aliveData.persistentState);
                if (!transport->remotePeer().isClient() && !m_localPeer.isClient())
                    queueSyncRequest(transport);
                else
                    transport->setState(QnTransactionTransport::Error);
            }
        }

        if (!aliveData.runtimeState.values.empty()) 
        {
            // check current persistent state
            if (!m_runtimeTransactionLog->contains(aliveData.runtimeState)) {
                qWarning() << "DETECT runtime transaction GAP via update message. Resync with peer" << transport->remotePeer().id;
                if (!transport->remotePeer().isClient() && !m_localPeer.isClient())
                    queueSyncRequest(transport);
                else
                    transport->setState(QnTransactionTransport::Error);
            }
        }

    }

    return true;
}

void QnTransactionMessageBus::onGotServerAliveInfo(const QnTransaction<ApiPeerAliveData> &tran, QnTransactionTransport* transport, const QnTransactionTransportHeader& ttHeader)
{
    Q_ASSERT(tran.peerID != qnCommon->moduleGUID());
    if (!gotAliveData(tran.params, transport))
        return; // ignore offline alive tran and resend online tran instead

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
    if (m_runtimeTransactionLog->contains(tran))
        return false;
    else {
        m_runtimeTransactionLog->saveTransaction(tran);
        return true;
    }
}

void QnTransactionMessageBus::at_gotTransaction(const QByteArray &serializedTran, const QnTransactionTransportHeader &transportHeader)
{
    QnTransactionTransport* sender = checked_cast<QnTransactionTransport*>(this->sender());

    //NX_LOG(QnLog::EC2_TRAN_LOG, lit("Got transaction sender = %1").arg((size_t) sender,  0, 16), cl_logDEBUG1);

    if( !sender || sender->getState() != QnTransactionTransport::ReadyForStreaming )
    {
        if( sender )
        {
            NX_LOG(QnLog::EC2_TRAN_LOG, lit("Ignoring incoming transaction because of state %1. ttHeader=%2 received from=%3")
                        .arg(sender->getState())
                        .arg(toString(transportHeader))
                        .arg(sender->remotePeer().id.toString()), cl_logDEBUG1);
            sender->transactionProcessed();
        }
        else
        {
            NX_LOG(QnLog::EC2_TRAN_LOG, lit("Ignoring transaction with seq %1 from unknown peer").arg(transportHeader.sequence), cl_logDEBUG1);
        }
        return;
    }

    Q_ASSERT(transportHeader.processedPeers.contains(sender->remotePeer().id));

    using namespace std::placeholders;
    if(!handleTransaction(serializedTran, std::bind(GotTransactionFuction(), this, _1, sender, transportHeader), [](Qn::SerializationFormat, const QByteArray& ) { return false; } ))
        sender->setState(QnTransactionTransport::Error);

    //TODO #ak is it garanteed that sender is alive?
    sender->transactionProcessed();
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
    // propagate new data to other peers. Aka send current state, other peers should request update if need
    handlePeerAliveChanged(m_localPeer, true, true); 
    m_aliveSendTimer.restart();
}

template <class T>
void QnTransactionMessageBus::sendTransactionToTransport(const QnTransaction<T> &tran, QnTransactionTransport* transport, const QnTransactionTransportHeader &transportHeader) {
    Q_ASSERT(!tran.isLocal);
    transport->sendTransaction(tran, transportHeader);
}

bool QnTransactionMessageBus::checkSequence(const QnTransactionTransportHeader& transportHeader, const QnAbstractTransaction& tran, QnTransactionTransport* transport)
{
    if (m_localPeer.isClient())
        return true;

    if (transportHeader.sender.isNull())
        return true; // old version, nothing to check

    // 1. check transport sequence
    QnTranStateKey ttSenderKey(transportHeader.sender, transportHeader.senderRuntimeID);
    int transportSeq = m_lastTransportSeq[ttSenderKey];
    if (transportSeq >= transportHeader.sequence) {
        NX_LOG( QnLog::EC2_TRAN_LOG, lit("Ignore transaction %1 %2 received via %3 because of transport sequence: %4 <= %5").
            arg(tran.toString()).arg(toString(transportHeader)).arg(transport->remotePeer().id.toString()).arg(transportHeader.sequence).arg(transportSeq), cl_logDEBUG1 );
        return false; // already processed
    }
    m_lastTransportSeq[ttSenderKey] = transportHeader.sequence;

    // 2. check persistent sequence
    if (tran.persistentInfo.isNull() || !transactionLog)
        return true; // nothing to check

    QnTranStateKey persistentKey(tran.peerID, tran.persistentInfo.dbID);
    int persistentSeq = transactionLog->getLatestSequence(persistentKey);

    if( QnLog::instance(QnLog::EC2_TRAN_LOG)->logLevel() >= cl_logWARNING )
        if (!transport->isSyncDone() && transport->isReadSync(ApiCommand::NotDefined) && transportHeader.sender != transport->remotePeer().id) 
        {
            NX_LOG( QnLog::EC2_TRAN_LOG, lit("Got transcaction from peer %1 while sync with peer %2 in progress").
                arg(transportHeader.sender.toString()).arg(transport->remotePeer().id.toString()), cl_logWARNING );
        }

    if (tran.persistentInfo.sequence > persistentSeq + 1) 
    {
        if (transport->isSyncDone()) 
        {
        // gap in persistent data detect, do resync
            NX_LOG( QnLog::EC2_TRAN_LOG, lit("GAP in persistent data detected! for peer %1 Expected seq=%2, but got seq=%3").
                arg(tran.peerID.toString()).arg(persistentSeq + 1).arg(tran.persistentInfo.sequence), cl_logDEBUG1 );

            if (!transport->remotePeer().isClient() && !m_localPeer.isClient())
                queueSyncRequest(transport);
            else 
                transport->setState(QnTransactionTransport::Error); // reopen
            return false;
        }
        else {
            NX_LOG( QnLog::EC2_TRAN_LOG, lit("GAP in persistent data, but sync in progress %1. Expected seq=%2, but got seq=%3").
                arg(tran.peerID.toString()).arg(persistentSeq + 1).arg(tran.persistentInfo.sequence), cl_logDEBUG1 );
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
    QnTransaction<ApiUpdateSequenceData> fillerTran(tran);
    fillerTran.command = ApiCommand::updatePersistentSequence;
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

    // do not perform any logic (aka sequence update) for foreign transaction. Just proxy
    if (!transportHeader.dstPeers.isEmpty() && !transportHeader.dstPeers.contains(m_localPeer.id))
    {
        if( QnLog::instance(QnLog::EC2_TRAN_LOG)->logLevel() >= cl_logDEBUG1 )
        {
            QString dstPeersStr;
            for( const QnUuid& peer: transportHeader.dstPeers )
                dstPeersStr += peer.toString();
            NX_LOG( QnLog::EC2_TRAN_LOG, lit("skip transaction %1 %2 for peers %3").arg(tran.toString()).arg(toString(transportHeader)).arg(dstPeersStr), cl_logDEBUG1);
        }
        proxyTransaction(tran, transportHeader);
        return;
    }

#if 0
    QnTransactionTransport* directConnection = m_connections.value(transportHeader.sender);
    if (directConnection && directConnection->getState() == QnTransactionTransport::ReadyForStreaming && directConnection->isReadSync(ApiCommand::NotDefined)) 
    {
        QnTranStateKey ttSenderKey(transportHeader.sender, transportHeader.senderRuntimeID);
        const int currentTransportSeq = m_lastTransportSeq.value(ttSenderKey);
        bool cond;
        if (sender != directConnection)
            cond = !currentTransportSeq || (currentTransportSeq > transportHeader.sequence);
        else
            cond = currentTransportSeq < transportHeader.sequence;

        if (!cond) {
            NX_LOG( QnLog::EC2_TRAN_LOG, printTransaction("Got unexpected transaction", tran, transportHeader, sender), cl_logDEBUG1);
            Q_ASSERT_X( cond, Q_FUNC_INFO, "Invalid transaction sequence, queued connetion" );
        }
    }
#endif
    AlivePeersMap:: iterator itr = m_alivePeers.find(transportHeader.sender);
    if (itr != m_alivePeers.end())
        itr.value().lastActivity.restart();

    if (!checkSequence(transportHeader, tran, sender))
        return;

    if (!sender->isReadSync(tran.command)) {
        NX_LOG( QnLog::EC2_TRAN_LOG, printTransaction("reject transaction (no readSync)", tran, transportHeader, sender), cl_logDEBUG1);
        return;
    }

    if (tran.isLocal && m_localPeer.isServer())
    {
        NX_LOG( QnLog::EC2_TRAN_LOG, printTransaction("reject local transaction", tran, transportHeader, sender), cl_logDEBUG1);
        return;
    }


    NX_LOG( QnLog::EC2_TRAN_LOG, printTransaction("got transaction", tran, transportHeader, sender), cl_logDEBUG1);
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

    proxyTransaction(tran, transportHeader);
}

template <class T>
void QnTransactionMessageBus::proxyTransaction(const QnTransaction<T> &tran, const QnTransactionTransportHeader &_transportHeader) 
{
    if (m_localPeer.isClient())
        return;

    QnTransactionTransportHeader transportHeader(_transportHeader);
    if (transportHeader.flags & TT_ProxyToClient) {
        QnPeerSet clients = qnTransactionBus->aliveClientPeers().keys().toSet();
        if (clients.isEmpty())
            return;
        transportHeader.dstPeers = clients;
        transportHeader.processedPeers += clients;
        transportHeader.processedPeers << m_localPeer.id;
        for(QnTransactionTransport* transport: m_connections)
        {
            if (transport->remotePeer().isClient() && transport->isReadyToSend(tran.command)) 
                transport->sendTransaction(tran, transportHeader);
        }
        return;
    }

    // proxy incoming transaction to other peers.
    if (!transportHeader.dstPeers.isEmpty() && (transportHeader.dstPeers - transportHeader.processedPeers).isEmpty()) {
        emit transactionProcessed(tran);
        return; // all dstPeers already processed
    }

    QnPeerSet processedPeers = transportHeader.processedPeers + connectedPeers(tran.command);
    processedPeers << m_localPeer.id;
    QnTransactionTransportHeader newHeader(transportHeader);
    newHeader.processedPeers = processedPeers;

    QSet<QnUuid> proxyList;
    for(QnConnectionMap::iterator itr = m_connections.begin(); itr != m_connections.end(); ++itr) 
    {
        QnTransactionTransport* transport = *itr;
        if (transportHeader.processedPeers.contains(transport->remotePeer().id) || !transport->isReadyToSend(tran.command)) 
            continue;

        //Q_ASSERT(transport->remotePeer().id != tran.peerID);
        transport->sendTransaction(tran, newHeader);
        proxyList << transport->remotePeer().id;
    }

    if( QnLog::instance(QnLog::EC2_TRAN_LOG)->logLevel() >= cl_logDEBUG1 )
        if (!proxyList.isEmpty())
        {
            QString proxyListStr;
            for( const QnUuid& peer: proxyList )
                proxyListStr += " " + peer.toString();
            NX_LOG( QnLog::EC2_TRAN_LOG, lit("proxy transaction %1 to (%2)").arg(tran.toString()).arg(proxyListStr), cl_logDEBUG1);
        }

    emit transactionProcessed(tran);
};

void QnTransactionMessageBus::printTranState(const QnTranState& tranState)
{
    if( QnLog::instance(QnLog::EC2_TRAN_LOG)->logLevel() < cl_logDEBUG1 )
        return;

    for(auto itr = tranState.values.constBegin(); itr != tranState.values.constEnd(); ++itr)
    {
        NX_LOG(QnLog::EC2_TRAN_LOG, lit("key=%1 (dbID=%2) need after=%3").arg(itr.key().peerID.toString()).arg(itr.key().dbID.toString()).arg(itr.value()), cl_logDEBUG1 );
    }
}

void QnTransactionMessageBus::onGotTransactionSyncRequest(QnTransactionTransport* sender, const QnTransaction<ApiSyncRequestData> &tran)
{
    sender->setWriteSync(true);

    QnTransactionTransportHeader ttUnicast;
    ttUnicast.processedPeers << sender->remotePeer().id << m_localPeer.id;
    ttUnicast.dstPeers << sender->remotePeer().id;
    QnTransactionTransportHeader ttBroadcast(ttUnicast);
    ttBroadcast.flags |= TT_ProxyToClient;

    QList<QByteArray> serializedTransactions;
    const ErrorCode errorCode = transactionLog->getTransactionsAfter(tran.params.persistentState, serializedTransactions);
    if (errorCode == ErrorCode::ok) 
    {
        NX_LOG( QnLog::EC2_TRAN_LOG, lit("got sync request from peer %1. Need transactions after:").arg(sender->remotePeer().id.toString()), cl_logDEBUG1);
        printTranState(tran.params.persistentState);
        NX_LOG( QnLog::EC2_TRAN_LOG, lit("exist %1 new transactions").arg(serializedTransactions.size()), cl_logDEBUG1);

        assert( m_connections.contains(sender->remotePeer().id) );
        assert( sender->getState() >= QnTransactionTransport::ReadyForStreaming );
        QnTransaction<QnTranStateResponse> tranSyncResponse(ApiCommand::tranSyncResponse);
        tranSyncResponse.params.result = 0;
        sender->sendTransaction(tranSyncResponse, ttUnicast);

        sendRuntimeInfo(sender, ttBroadcast, tran.params.runtimeState);

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
    requestTran.params.runtimeState = m_runtimeTransactionLog->getTransactionsState();

    NX_LOG( QnLog::EC2_TRAN_LOG, lit("send syncRequest to peer %1").arg(transport->remotePeer().id.toString()), cl_logDEBUG1 );
    printTranState(requestTran.params.persistentState);
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

void QnTransactionMessageBus::connectToPeerLost(const QnUuid& id)
{
    if (m_alivePeers.contains(id))
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
    ApiPeerAliveData aliveData(peer, isAlive);

    if (sendTran)
    {
        QnTransaction<ApiPeerAliveData> tran(ApiCommand::peerAliveInfo);
        tran.params = aliveData;
        Q_ASSERT(!tran.params.peer.instanceId.isNull());
        if (isAlive && transactionLog && peer.id == m_localPeer.id) {
            tran.params.persistentState = transactionLog->getTransactionsState();
            tran.params.runtimeState = m_runtimeTransactionLog->getTransactionsState();
        }
        sendTransaction(tran);
        NX_LOG( QnLog::EC2_TRAN_LOG, lit("sending peerAlive info. id=%1 type=%2 isAlive=%3").arg(peer.id.toString()).arg(peer.peerType).arg(isAlive), cl_logDEBUG1);
    }

    if( peer.id == qnCommon->moduleGUID() )
        return; //sending keep-alive

    if (isAlive) {
        NX_LOG( QnLog::EC2_TRAN_LOG, lit("emit peerFound. id=%1").arg(aliveData.peer.id.toString()), cl_logDEBUG1);
    }
    else {
        NX_LOG( QnLog::EC2_TRAN_LOG, lit("emit peerLost. id=%1").arg(aliveData.peer.id.toString()), cl_logDEBUG1);
    }

    QMutexLocker lk( &m_signalEmitMutex );
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
        transaction.params.push_back(data);
    }
    transaction.peerID = m_localPeer.id;
    return transaction;
}

//TODO #ak use SocketAddress instead of this function. It will reduce QString instanciations and make code more clear
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
        QnTransactionTransport* transport =  itr.value();
        if (getUrlAddr(transport->remoteAddr()) == addr1)
            return true;
    }
    return false;
}

void QnTransactionMessageBus::at_stateChanged(QnTransactionTransport::State )
{
    QMutexLocker lock(&m_mutex);
    QnTransactionTransport* transport = (QnTransactionTransport*) sender();

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
        removeTTSequenceForPeer(transport->remotePeer().id);
        transport->setState(QnTransactionTransport::ReadyForStreaming);

        transport->processExtraData();
        transport->startListening();

        m_runtimeTransactionLog->clearOldRuntimeData(QnTranStateKey(transport->remotePeer().id, transport->remotePeer().instanceId));
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
            QnTransactionTransport* transportPtr = m_connectingConnections[i];
            if (transportPtr == transport) {
                m_connectingConnections.removeAt(i);
                return;
            }
        }

        for(QnConnectionMap::iterator itr = m_connections.begin(); itr != m_connections.end(); ++itr)
        {
            QnTransactionTransport* transportPtr = itr.value();
            if (transportPtr == transport) {
                connectToPeerLost(transport->remotePeer().id);
                m_connections.erase(itr);
                break;
            }
        }
        transport->deleteLater();
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
        for(QnTransactionTransport* transport: m_connections.values()) 
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
            QnTransactionTransport* transport = new QnTransactionTransport(m_localPeer);
            connect(transport, &QnTransactionTransport::gotTransaction, this, &QnTransactionMessageBus::at_gotTransaction,  Qt::QueuedConnection);
            connect(transport, &QnTransactionTransport::stateChanged, this, &QnTransactionMessageBus::at_stateChanged,  Qt::QueuedConnection);
            connect(transport, &QnTransactionTransport::remotePeerUnauthorized, this, &QnTransactionMessageBus::emitRemotePeerUnauthorized,  Qt::DirectConnection);
            connect(transport, &QnTransactionTransport::peerIdDiscovered, this, &QnTransactionMessageBus::at_peerIdDiscovered,  Qt::QueuedConnection);
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
    NX_LOG( QnLog::EC2_TRAN_LOG, "Current transaction state:", cl_logDEBUG1 );
    if (transactionLog)
        printTranState(transactionLog->getTransactionsState());
    }

    // check if some server not accessible any more
    QSet<QnUuid> lostPeers;
    for (AlivePeersMap::iterator itr = m_alivePeers.begin(); itr != m_alivePeers.end(); ++itr)
    {
        if (itr.value().lastActivity.elapsed() > ALIVE_UPDATE_TIMEOUT)
        {
            itr.value().lastActivity.restart();
            for(QnTransactionTransport* transport: m_connectingConnections) {
                if (transport->getState() == QnTransactionTransport::Closed)
                    continue; // it's going to close soon
                if (transport->remotePeer().id == itr.key()) {
                    qWarning() << "No alive info during timeout. reconnect to peer" << transport->remotePeer().id;
                    transport->setState(QnTransactionTransport::Error);
                }
            }

            for(QnTransactionTransport* transport: m_connections.values()) {
                if (transport->getState() == QnTransactionTransport::Closed)
                    continue; // it's going to close soon
                if (transport->remotePeer().id == itr.key() && transport->remotePeer().peerType == Qn::PT_Server) {
                    qWarning() << "No alive info during timeout. reconnect to peer" << transport->remotePeer().id;
                    transport->setState(QnTransactionTransport::Error);
                }
            }
            lostPeers << itr.key();
        }
    }
    for (const QnUuid& id: lostPeers)
        connectToPeerLost(id);
}

void QnTransactionMessageBus::sendRuntimeInfo(QnTransactionTransport* transport, const QnTransactionTransportHeader& transportHeader, const QnTranState& runtimeState)
{
    QList<QnTransaction<ApiRuntimeData>> result;
    m_runtimeTransactionLog->getTransactionsAfter(runtimeState, result);
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

    QnTransactionTransport* transport = new QnTransactionTransport(m_localPeer, socket);
    transport->setRemotePeer(remotePeer);
    connect(transport, &QnTransactionTransport::gotTransaction, this, &QnTransactionMessageBus::at_gotTransaction,  Qt::QueuedConnection);
    connect(transport, &QnTransactionTransport::stateChanged, this, &QnTransactionMessageBus::at_stateChanged,  Qt::QueuedConnection);
    connect(transport, &QnTransactionTransport::remotePeerUnauthorized, this, &QnTransactionMessageBus::emitRemotePeerUnauthorized, Qt::DirectConnection );

    QMutexLocker lock(&m_mutex);
    transport->moveToThread(thread());
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
    for(QnTransactionTransport* transport: m_connections.values())
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
    for(QnTransactionTransport* transport: m_connections) {
        qWarning() << "Disconnected from peer" << transport->remoteAddr();
        transport->setState(QnTransactionTransport::Error);
    }
    for (auto transport: m_connectingConnections) 
        transport->setState(ec2::QnTransactionTransport::Error);
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
        QnTransactionTransport* transport = *itr;
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

QnTransactionMessageBus::AlivePeersMap QnTransactionMessageBus::aliveClientPeers() const
{
    QMutexLocker lock(&m_mutex);
    AlivePeersMap result;
    for(AlivePeersMap::const_iterator itr = m_alivePeers.begin(); itr != m_alivePeers.end(); ++itr)
    {
        if (itr->peer.isClient())
            result.insert(itr.key(), itr.value());
    }

    return result;
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

void QnTransactionMessageBus::emitRemotePeerUnauthorized(const QnUuid& id)
{
    QMutexLocker lk( &m_signalEmitMutex );
    emit remotePeerUnauthorized( id );
}

}
