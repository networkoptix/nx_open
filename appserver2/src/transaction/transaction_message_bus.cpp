#include "transaction_message_bus.h"

#include <boost/bind.hpp>

#include "remote_ec_connection.h"
#include "utils/network/aio/aioservice.h"
#include "utils/common/systemerror.h"
#include "ec2_connection.h"
#include "common/common_module.h"

#include <transaction/transaction_transport.h>

#include "api/app_server_connection.h"
#include "nx_ec/data/api_server_alive_data.h"
#include "utils/common/synctime.h"
#include "ec_connection_notification_manager.h"
#include "nx_ec/data/api_camera_data.h"
#include "nx_ec/data/api_resource_data.h"

#include <utils/common/checked_cast.h>
#include "utils/common/warnings.h"
#include "api/runtime_info_manager.h"

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
bool handleTransactionParams(QnInputBinaryStream<QByteArray> *stream, const QnAbstractTransaction &abstractTransaction, const Function &function) 
{
    QnTransaction<T> transaction(abstractTransaction);
    if (!QnBinary::deserialize(stream, &transaction.params)) 
        return false;
            
    function(transaction);
    return true;
}

template<class Function>
bool handleTransaction(const QByteArray &serializedTransaction, const Function &function)
{
    QnAbstractTransaction transaction;
    QnInputBinaryStream<QByteArray> stream(&serializedTransaction);
    if (!QnBinary::deserialize(&stream, &transaction)) {
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
    case ApiCommand::updateVideowallInstanceStatus:  
                                            return handleTransactionParams<ApiVideowallInstanceStatusData>(&stream, transaction, function);
    case ApiCommand::addStoredFile:
    case ApiCommand::updateStoredFile:      return handleTransactionParams<ApiStoredFileData>       (&stream, transaction, function);
    case ApiCommand::removeStoredFile:      return handleTransactionParams<ApiStoredFilePath>       (&stream, transaction, function);
    case ApiCommand::broadcastBusinessAction:
    case ApiCommand::execBusinessAction:    return handleTransactionParams<ApiBusinessActionData>   (&stream, transaction, function);
    case ApiCommand::saveSettings:          return handleTransactionParams<ApiResourceParamDataList>(&stream, transaction, function);
    case ApiCommand::addLicenses:           return handleTransactionParams<ApiLicenseDataList>      (&stream, transaction, function);
    case ApiCommand::addLicense:            return handleTransactionParams<ApiLicenseData>          (&stream, transaction, function);
    case ApiCommand::testEmailSettings:     transaction.localTransaction = true;
                                            return handleTransactionParams<ApiEmailSettingsData>    (&stream, transaction, function);
    case ApiCommand::resetBusinessRules:    return handleTransactionParams<ApiResetBusinessRuleData>(&stream, transaction, function);
    case ApiCommand::uploadUpdate:          return handleTransactionParams<ApiUpdateUploadData>     (&stream, transaction, function);
    case ApiCommand::uploadUpdateResponce:  return handleTransactionParams<ApiUpdateUploadResponceData>(&stream, transaction, function);
    case ApiCommand::installUpdate:         return handleTransactionParams<QString>                 (&stream, transaction, function);
    case ApiCommand::addCameraBookmarkTags:
    case ApiCommand::removeCameraBookmarkTags:
                                            return handleTransactionParams<ApiCameraBookmarkTagDataList>(&stream, transaction, function);

    case ApiCommand::lockRequest:
    case ApiCommand::lockResponse:
    case ApiCommand::unlockRequest:         return handleTransactionParams<ApiLockData>             (&stream, transaction, function); 
    case ApiCommand::peerAliveInfo:         return handleTransactionParams<ApiPeerAliveData>        (&stream, transaction, function);
    case ApiCommand::tranSyncRequest:       return handleTransactionParams<QnTranState>             (&stream, transaction, function);
    case ApiCommand::tranSyncResponse:      return handleTransactionParams<int>                     (&stream, transaction, function);
    case ApiCommand::runtimeInfoChanged:    return handleTransactionParams<ApiRuntimeData>          (&stream, transaction, function);

    default:
        Q_ASSERT_X(0, Q_FUNC_INFO, "Transaction type is not implemented for delivery! Implement me!");
        return false;
    }
}



// --------------------------------- QnTransactionMessageBus ------------------------------
static QnTransactionMessageBus*m_globalInstance = 0;

QnTransactionMessageBus* QnTransactionMessageBus::instance()
{
    return m_globalInstance;
}

void QnTransactionMessageBus::initStaticInstance(QnTransactionMessageBus* instance)
{
    Q_ASSERT(m_globalInstance == 0 || instance == 0);
    delete m_globalInstance;
    m_globalInstance = instance;
}

QnTransactionMessageBus::QnTransactionMessageBus(): 
    m_localPeer(QnId(), Qn::PT_Server),
    m_binaryTranSerializer(new QnBinaryTransactionSerializer()),
    m_jsonTranSerializer(new QnJsonTransactionSerializer()),
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
}

void QnTransactionMessageBus::start()
{
    assert(!m_localPeer.id.isNull());
    if (!m_thread->isRunning())
        m_thread->start();
}

QnTransactionMessageBus::~QnTransactionMessageBus()
{
    if (m_thread) {
        m_thread->exit();
        m_thread->wait();
    }
    delete m_thread;
    delete m_timer;
}

void QnTransactionMessageBus::onGotServerAliveInfo(const QnTransaction<ApiPeerAliveData> &tran)
{
    if (tran.params.peer.id == m_localPeer.id)
        return; // ignore himself

    // proxy alive info from non-direct connected host
    AlivePeersMap::iterator itr = m_alivePeers.find(tran.params.peer.id);
    if (tran.params.isAlive && itr == m_alivePeers.end()) {
        m_alivePeers.insert(tran.params.peer.id, AlivePeerInfo(ApiPeerData(tran.params.peer.id, tran.params.peer.peerType), true));
        emit peerFound(tran.params, true);
    }
    else if (!tran.params.isAlive && itr != m_alivePeers.end()) {
        emit peerLost(tran.params, true);
        m_alivePeers.remove(tran.params.peer.id);
    }
}

void QnTransactionMessageBus::at_gotTransaction(const QByteArray &serializedTran, const QnTransactionTransportHeader &transportHeader)
{
    QnTransactionTransport* sender = checked_cast<QnTransactionTransport*>(this->sender());
    if (!sender || sender->getState() != QnTransactionTransport::ReadyForStreaming)
        return;

    if(!handleTransaction(serializedTran, boost::bind(GotTransactionFuction(), this, _1, sender, transportHeader)))
        sender->setState(QnTransactionTransport::Error);
}


// ------------------ QnTransactionMessageBus::CustomHandler -------------------


void QnTransactionMessageBus::onGotDistributedMutexTransaction(const QnTransaction<ApiLockData>& tran) {
    if(tran.command == ApiCommand::lockRequest)
        emit gotLockRequest(tran.params);
    else if(tran.command == ApiCommand::lockResponse) 
        emit gotLockResponse(tran.params);
}

void QnTransactionMessageBus::onGotTransactionSyncResponse(QnTransactionTransport* sender)
{
	sender->setReadSync(true);
}

template <class T>
void QnTransactionMessageBus::sendTransactionToTransport(const QnTransaction<T> &tran, QnTransactionTransport* transport, const QnTransactionTransportHeader &transportHeader) {
    transport->sendTransaction(tran, transportHeader);
}
        
template <class T>
void QnTransactionMessageBus::gotTransaction(const QnTransaction<T> &tran, QnTransactionTransport* sender, const QnTransactionTransportHeader &transportHeader) {
    AlivePeersMap:: iterator itr = m_alivePeers.find(tran.id.peerID);
    if (itr != m_alivePeers.end())
        itr.value().lastActivity.restart();

    if (isSystem(tran.command)) {
        if (m_lastTranSeq[tran.id.peerID] >= tran.id.sequence)
            return; // already processed
        m_lastTranSeq[tran.id.peerID] = tran.id.sequence;
    }

    if (transportHeader.dstPeers.isEmpty() || transportHeader.dstPeers.contains(m_localPeer.id)) {
        qDebug() << "got transaction " << ApiCommand::toString(tran.command) << "with time=" << tran.timestamp;
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
            onGotTransactionSyncResponse(sender);
            return;
        case ApiCommand::peerAliveInfo:
            onGotServerAliveInfo(tran);
            break; // do not return. proxy this transaction
        default:
            // general transaction
            if (!sender->isReadSync())
                return;

            if( tran.persistent && transactionLog && transactionLog->contains(tran) )
            {
                // transaction is already processed
            }
            else 
            {
                if (tran.persistent && dbManager)
                {
                    QByteArray serializedTran = QnBinaryTransactionSerializer::instance()->serializedTransaction(tran);
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
            }

            // this is required to allow client place transactions directly into transaction message bus
            if (tran.command == ApiCommand::getFullInfo)
                sender->setWriteSync(true);
            break;
        }
    }
    else {
        qDebug() << "skip transaction " << ApiCommand::toString(tran.command) << "for peers" << transportHeader.dstPeers;
    }

    QMutexLocker lock(&m_mutex);

    // proxy incoming transaction to other peers.
    if (!transportHeader.dstPeers.isEmpty() && (transportHeader.dstPeers - transportHeader.processedPeers).isEmpty()) {
        emit transactionProcessed(tran);
        return; // all dstPeers already processed
    }

    QnPeerSet processedPeers = transportHeader.processedPeers + connectedPeers(tran.command);
    processedPeers << m_localPeer.id;

    for(QnConnectionMap::iterator itr = m_connections.begin(); itr != m_connections.end(); ++itr) {
        QnTransactionTransportPtr transport = *itr;
        if (transportHeader.processedPeers.contains(transport->remotePeer().id) || !transport->isReadyToSend(tran.command)) 
            continue;

        Q_ASSERT(transport->remotePeer().id != tran.id.peerID);
        transport->sendTransaction(tran, QnTransactionTransportHeader(processedPeers));
    }

    emit transactionProcessed(tran);
}

void QnTransactionMessageBus::onGotTransactionSyncRequest(QnTransactionTransport* sender, const QnTransaction<QnTranState> &tran)
{
    sender->setWriteSync(true);

    QList<QByteArray> serializedTransactions;
    const ErrorCode errorCode = transactionLog->getTransactionsAfter(tran.params, serializedTransactions);
    if (errorCode == ErrorCode::ok) 
    {
        QnTransaction<int> tran(ApiCommand::tranSyncResponse, false);
        tran.params = 0;
        tran.fillSequence();
        QByteArray chunkData;
        QnPeerSet processedPeers(QnPeerSet() << sender->remotePeer().id << m_localPeer.id);
        sender->sendTransaction(tran, processedPeers);

        QnTransactionTransportHeader transportHeader(processedPeers);
        foreach(const QByteArray& serializedTran, serializedTransactions)
            if(!handleTransaction(serializedTran, boost::bind(SendTransactionToTransportFuction(), this, _1, sender, transportHeader)))
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
    QnTransaction<QnTranState> requestTran(ApiCommand::tranSyncRequest, false);
    requestTran.params = transactionLog->getTransactionsState();
    requestTran.fillSequence();
    transport->sendTransaction(requestTran, QnPeerSet() << transport->remotePeer().id << m_localPeer.id);
}

void QnTransactionMessageBus::connectToPeerLost(const QnId& id)
{
    if (!m_alivePeers.contains(id)) 
        return;    
    
    ApiPeerData peer = m_alivePeers.value(id).peer;
    m_alivePeers.remove(id);
    handlePeerAliveChanged(peer, false);
}

void QnTransactionMessageBus::connectToPeerEstablished(const ApiPeerData &peer)
{
    if (m_alivePeers.contains(peer.id)) 
        return;

    m_alivePeers.insert(peer.id, AlivePeerInfo(peer, false));
    handlePeerAliveChanged(peer, true);
}

void QnTransactionMessageBus::handlePeerAliveChanged(const ApiPeerData &peer, bool isAlive) {
    QnTransaction<ApiPeerAliveData> tran(ApiCommand::peerAliveInfo, false);
    tran.params.peer.id = peer.id;
    tran.params.peer.peerType = peer.peerType;
    tran.params.isAlive = isAlive;
    tran.fillSequence();
    sendTransaction(tran);

    if (peer.peerType == Qn::PT_VideowallClient)
        sendVideowallInstanceStatus(peer, isAlive);

    if( peer.id == qnCommon->moduleGUID() )
        return; //sending keep-alive

    if (isAlive)
        emit peerFound(tran.params, false);
    else
        emit peerLost(tran.params, false);
}

void QnTransactionMessageBus::sendVideowallInstanceStatus(const ApiPeerData &peer, bool isAlive) {
    QnTransaction<ApiVideowallInstanceStatusData> tran(ApiCommand::updateVideowallInstanceStatus, false);
    tran.params.online = isAlive;
    tran.params.instanceGuid = peer.params["instanceGuid"];
    tran.params.videowallGuid = peer.params["videowallGuid"];
    tran.fillSequence();
    sendTransaction(tran);
}

QString getUrlAddr(const QUrl& url) { return url.host() + QString::number(url.port()); }
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
        for (int i = 0; i < m_connectingConnections.size(); ++i) 
        {
            if (m_connectingConnections[i] == transport) {
                m_connections[transport->remotePeer().id] = m_connectingConnections[i];
                m_connectingConnections.removeAt(i);
                break;
            }
        }
        m_lastTranSeq[transport->remotePeer().id] = 0;
        transport->setState(QnTransactionTransport::ReadyForStreaming);
        // if sync already done or in progress do not send new request
        if (!transport->remotePeer().isClient() && !m_localPeer.isClient())
            queueSyncRequest(transport);
        connectToPeerEstablished(transport->remotePeer());
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

    m_connectionsToRemove.clear();

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
        handlePeerAliveChanged(m_localPeer, true);
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

void QnTransactionMessageBus::sendRuntimeInfo(QnTransactionTransportPtr transport, const QnPeerSet& processedPeers)
{
    foreach (ApiRuntimeData info, QnRuntimeInfoManager::instance()->allData().values())
    {
        QnTransaction<ApiRuntimeData> tran(ApiCommand::runtimeInfoChanged, false);
        tran.params = info;
        transport->sendTransaction(tran, processedPeers);
    }
}

void QnTransactionMessageBus::gotConnectionFromRemotePeer(QSharedPointer<AbstractStreamSocket> socket, const ApiPeerData &remotePeer)
{
    if (!dbManager)
    {
        qWarning() << "This peer connected to remote EC. Ignoring incoming connection";
        return;
    }

    QnTransactionTransportPtr transport(new QnTransactionTransport(m_localPeer, remotePeer, socket));
    connect(transport.data(), &QnTransactionTransport::gotTransaction, this, &QnTransactionMessageBus::at_gotTransaction,  Qt::QueuedConnection);
    connect(transport.data(), &QnTransactionTransport::stateChanged, this, &QnTransactionMessageBus::at_stateChanged,  Qt::QueuedConnection);

    transport->setState(QnTransactionTransport::Connected);


    /** Send all data to the client peers on the connect. */
    QnPeerSet processedPeers = QnPeerSet() << remotePeer.id << m_localPeer.id;
    if (remotePeer.peerType == Qn::PT_DesktopClient || 
        remotePeer.peerType == Qn::PT_VideowallClient)     //TODO: #GDM #VW do not send fullInfo, just required part of it
    {
        /** Request all data to be sent to the client peers on the connect. */
        QnTransaction<ApiFullInfoData> tran;
        tran.command = ApiCommand::getFullInfo;
        tran.id.peerID = m_localPeer.id;
        if (dbManager->doQuery(nullptr, tran.params) != ErrorCode::ok) {
            qWarning() << "Can't execute query for sync with client peer!";
            QnTransactionTransport::connectDone(remotePeer.id); // release connection
            return;
        }

        transport->setWriteSync(true);
        sendRuntimeInfo(transport, processedPeers);
        transport->sendTransaction(tran, processedPeers);
        transport->setReadSync(true);
    } else if (remotePeer.peerType == Qn::PT_AndroidClient) {
        /** Request all data to be sent to the client peers on the connect. */
        QnTransaction<ApiMediaServerDataList> tranServers;
        tranServers.command = ApiCommand::getMediaServers;
        tranServers.id.peerID = m_localPeer.id;
        if (dbManager->doQuery(nullptr, tranServers.params) != ErrorCode::ok) {
            qWarning() << "Can't execute query for sync with client peer!";
            QnTransactionTransport::connectDone(remotePeer.id); // release connection
            return;
        }

        QnTransaction<ApiCameraDataList> tranCameras;
        tranCameras.command = ApiCommand::getCameras;
        tranCameras.id.peerID = m_localPeer.id;
        if (dbManager->doQuery(QnId(), tranCameras.params) != ErrorCode::ok) {
            qWarning() << "Can't execute query for sync with client peer!";
            QnTransactionTransport::connectDone(remotePeer.id); // release connection
            return;
        }

        QnTransaction<ApiUserDataList> tranUsers;
        tranUsers.command = ApiCommand::getUsers;
        tranUsers.id.peerID = m_localPeer.id;
        if (dbManager->doQuery(nullptr, tranUsers.params) != ErrorCode::ok) {
            qWarning() << "Can't execute query for sync with client peer!";
            QnTransactionTransport::connectDone(remotePeer.id); // release connection
            return;
        }

        QnTransaction<ApiLayoutDataList> tranLayouts;
        tranLayouts.command = ApiCommand::getLayouts;
        tranLayouts.id.peerID = m_localPeer.id;
        if (dbManager->doQuery(nullptr, tranLayouts.params) != ErrorCode::ok) {
            qWarning() << "Can't execute query for sync with client peer!";
            QnTransactionTransport::connectDone(remotePeer.id); // release connection
            return;
        }

        QnTransaction<ApiCameraServerItemDataList> tranCameraHistory;
        tranCameraHistory.command = ApiCommand::getCameraHistoryItems;
        tranLayouts.id.peerID = m_localPeer.id;
        if (dbManager->doQuery(nullptr, tranLayouts.params) != ErrorCode::ok) {
            qWarning() << "Can't execute query for sync with client peer!";
            QnTransactionTransport::connectDone(remotePeer.id); // release connection
            return;
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
    
    {
        QMutexLocker lock(&m_mutex);
        if (m_connections[remotePeer.id]) 
            m_connectionsToRemove << m_connections[remotePeer.id];
        m_connections[remotePeer.id] = transport;
    }
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

}
