#include "transaction_message_bus.h"

#include "remote_ec_connection.h"
#include "utils/network/aio/aioservice.h"
#include "utils/common/systemerror.h"
#include "ec2_connection.h"
#include "common/common_module.h"

#include <transaction/transaction_transport.h>

#include "utils/common/synctime.h"
#include "nx_ec/data/api_server_alive_data.h"

#include "api/app_server_connection.h"

namespace ec2
{

static const int RECONNECT_TIMEOUT = 1000 * 5;
static const int ALIVE_UPDATE_INTERVAL = 1000 * 60;

#define QN_HANDLE_TRANSACTION_PARAMS(stream, abstractTran, tranType, method, sender, transportHeader) \
    { \
        QnTransaction<tranType> tran(abstractTran); \
        if (!QnBinary::deserialize(&stream, &tran.params)) return; \
        method<tranType>(tran, sender, transportHeader); \
        break; \
    } 

#define QN_HANDLE_TRANSACTION(serialized, method, sender, transportHeader) \
    { \
        QnAbstractTransaction abstractTran;                                                             \
        QnInputBinaryStream<QByteArray> stream(serialized);                                             \
        if (!QnBinary::deserialize(&stream, &abstractTran)) {                                           \
            qWarning() << Q_FUNC_INFO << "Ignore bad transaction data. size=" << serialized.size();     \
            sender->setState(QnTransactionTransport::Error);                                            \
            return;                                                                                     \
        }                                                                                               \
        switch (abstractTran.command) {                                                                 \
        case ApiCommand::getAllDataList:        QN_HANDLE_TRANSACTION_PARAMS(stream, abstractTran, ApiFullInfoData, method, sender, transportHeader)    \
        case ApiCommand::setResourceStatus:     QN_HANDLE_TRANSACTION_PARAMS(stream, abstractTran, ApiSetResourceStatusData, method, sender, transportHeader)    \
        case ApiCommand::setResourceParams:     QN_HANDLE_TRANSACTION_PARAMS(stream, abstractTran, ApiResourceParamsData, method, sender, transportHeader)    \
        case ApiCommand::saveResource:          QN_HANDLE_TRANSACTION_PARAMS(stream, abstractTran, ApiResourceData, method, sender, transportHeader)    \
        case ApiCommand::removeResource:        QN_HANDLE_TRANSACTION_PARAMS(stream, abstractTran, ApiIdData, method, sender, transportHeader)    \
        case ApiCommand::setPanicMode:          QN_HANDLE_TRANSACTION_PARAMS(stream, abstractTran, ApiPanicModeData, method, sender, transportHeader)    \
        case ApiCommand::saveCamera:            QN_HANDLE_TRANSACTION_PARAMS(stream, abstractTran, ApiCameraData, method, sender, transportHeader)    \
        case ApiCommand::saveCameras:           QN_HANDLE_TRANSACTION_PARAMS(stream, abstractTran, ApiCameraDataList, method, sender, transportHeader)    \
        case ApiCommand::removeCamera:          QN_HANDLE_TRANSACTION_PARAMS(stream, abstractTran, ApiIdData, method, sender, transportHeader)    \
        case ApiCommand::addCameraHistoryItem:  QN_HANDLE_TRANSACTION_PARAMS(stream, abstractTran, ApiCameraServerItemData, method, sender, transportHeader)    \
        case ApiCommand::saveMediaServer:       QN_HANDLE_TRANSACTION_PARAMS(stream, abstractTran, ApiMediaServerData, method, sender, transportHeader)    \
        case ApiCommand::removeMediaServer:     QN_HANDLE_TRANSACTION_PARAMS(stream, abstractTran, ApiIdData, method, sender, transportHeader)    \
        case ApiCommand::saveUser:              QN_HANDLE_TRANSACTION_PARAMS(stream, abstractTran, ApiUserData, method, sender, transportHeader)    \
        case ApiCommand::removeUser:            QN_HANDLE_TRANSACTION_PARAMS(stream, abstractTran, ApiIdData, method, sender, transportHeader)    \
        case ApiCommand::saveBusinessRule:      QN_HANDLE_TRANSACTION_PARAMS(stream, abstractTran, ApiBusinessRuleData, method, sender, transportHeader)    \
        case ApiCommand::removeBusinessRule:    QN_HANDLE_TRANSACTION_PARAMS(stream, abstractTran, ApiIdData, method, sender, transportHeader)    \
        case ApiCommand::saveLayouts:           QN_HANDLE_TRANSACTION_PARAMS(stream, abstractTran, ApiLayoutDataList, method, sender, transportHeader)    \
        case ApiCommand::saveLayout:            QN_HANDLE_TRANSACTION_PARAMS(stream, abstractTran, ApiLayoutData, method, sender, transportHeader)    \
        case ApiCommand::removeLayout:          QN_HANDLE_TRANSACTION_PARAMS(stream, abstractTran, ApiIdData, method, sender, transportHeader)    \
        case ApiCommand::saveVideowall:         QN_HANDLE_TRANSACTION_PARAMS(stream, abstractTran, ApiVideowallData, method, sender, transportHeader)    \
        case ApiCommand::removeVideowall:       QN_HANDLE_TRANSACTION_PARAMS(stream, abstractTran, ApiIdData, method, sender, transportHeader)    \
        case ApiCommand::videowallControl:      QN_HANDLE_TRANSACTION_PARAMS(stream, abstractTran, ApiVideowallControlMessageData, method, sender, transportHeader)    \
        case ApiCommand::addStoredFile: \
        case ApiCommand::updateStoredFile:      QN_HANDLE_TRANSACTION_PARAMS(stream, abstractTran, ApiStoredFileData, method, sender, transportHeader)    \
        case ApiCommand::removeStoredFile:      QN_HANDLE_TRANSACTION_PARAMS(stream, abstractTran, ApiStoredFilePath, method, sender, transportHeader)    \
        case ApiCommand::broadcastBusinessAction: \
        case ApiCommand::execBusinessAction:    QN_HANDLE_TRANSACTION_PARAMS(stream, abstractTran, ApiBusinessActionData, method, sender, transportHeader)    \
        case ApiCommand::saveSettings:          QN_HANDLE_TRANSACTION_PARAMS(stream, abstractTran, ApiResourceParamDataList, method, sender, transportHeader)    \
        case ApiCommand::addLicenses:           QN_HANDLE_TRANSACTION_PARAMS(stream, abstractTran, ApiLicenseDataList, method, sender, transportHeader)    \
        case ApiCommand::addLicense:            QN_HANDLE_TRANSACTION_PARAMS(stream, abstractTran, ApiLicenseData, method, sender, transportHeader)    \
        case ApiCommand::testEmailSettings: { \
                                                abstractTran.localTransaction = true; \
                                                QN_HANDLE_TRANSACTION_PARAMS(stream, abstractTran, ApiEmailSettingsData, method, sender, transportHeader)    \
                                            } \
        case ApiCommand::resetBusinessRules:    QN_HANDLE_TRANSACTION_PARAMS(stream, abstractTran, ApiResetBusinessRuleData, method, sender, transportHeader)    \
        case ApiCommand::uploadUpdate:          QN_HANDLE_TRANSACTION_PARAMS(stream, abstractTran, ApiUpdateUploadData, method, sender, transportHeader)    \
        case ApiCommand::uploadUpdateResponce:  QN_HANDLE_TRANSACTION_PARAMS(stream, abstractTran, ApiUpdateUploadResponceData, method, sender, transportHeader)    \
        case ApiCommand::installUpdate:         QN_HANDLE_TRANSACTION_PARAMS(stream, abstractTran, QString, method, sender, transportHeader)    \
        case ApiCommand::serverAliveInfo:\
            break; \
        case ApiCommand::addCameraBookmarkTags: \
        case ApiCommand::removeCameraBookmarkTags: QN_HANDLE_TRANSACTION_PARAMS(stream, abstractTran, ApiCameraBookmarkTagDataList, method, sender, transportHeader)    \
        default: \
            Q_ASSERT_X(0, Q_FUNC_INFO, "Transaction type is not implemented for delivery! Implement me!"); \
            break;\
        }\
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
    m_localPeer(QnId(), QnPeerInfo::Server),
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
    if (tran.params.peerId == m_localPeer.id)
        return; // ignore himself

    // proxy alive info from non-direct connected host
    AlivePeersMap::iterator itr = m_alivePeers.find(tran.params.peerId);
    if (tran.params.isAlive && itr == m_alivePeers.end()) {
        m_alivePeers.insert(tran.params.peerId, AlivePeerInfo(QnPeerInfo(tran.params.peerId, static_cast<QnPeerInfo::Type>(tran.params.peerType)), true, tran.params.hardwareIds));
        emit peerFound(tran.params, true);
    }
    else if (!tran.params.isAlive && itr != m_alivePeers.end()) {
        emit peerLost(tran.params, true);
        m_alivePeers.remove(tran.params.peerId);
    }
}

void QnTransactionMessageBus::at_gotTransaction(const QByteArray &serializedTran, const QnTransactionTransportHeader &transportHeader)
{
    QnTransactionTransport* sender = (QnTransactionTransport*) this->sender();
    if (!sender || sender->getState() != QnTransactionTransport::ReadyForStreaming)
        return;

    QN_HANDLE_TRANSACTION(serializedTran, gotTransaction, sender, transportHeader);
}


// ------------------ QnTransactionMessageBus::CustomHandler -------------------


template <class T>
template <class T2>
bool QnTransactionMessageBus::CustomHandler<T>::deliveryTransaction(const QnTransaction<T2> &tran)
{  
    if (abstractTran.persistent && transactionLog && transactionLog->contains(tran))
        return true; // transaction already processed

    // trigger notification, update local data e.t.c
    if (!m_handler->template processIncomingTransaction<T2>(tran, stream.buffer()))
        return false;

    return true;
}

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
        sender->sendTransaction(tran,  QnPeerSet() << sender->remotePeer().id);

        QnTransactionTransportHeader transportHeader(QnPeerSet() << sender->remotePeer().id << m_localPeer.id);
        foreach(const QByteArray& serializedTran, serializedTransactions) {

            QN_HANDLE_TRANSACTION(serializedTran, sendTransactionToTransport, sender, transportHeader);
        }
        return;
    }
    else {
        qWarning() << "Can't execute query for sync with server peer!";
    }
}

template <class T>
bool QnTransactionMessageBus::CustomHandler<T>::processTransaction(QnTransactionTransport* /*sender*/, QnAbstractTransaction& abstractTran, QnInputBinaryStream<QByteArray>& stream)
{
    //TODO/IMPL deliver businessActionBroadcasted( const QnAbstractBusinessActionPtr& businessAction );
    //TODO/IMPL deliver businessRuleReset( const QnBusinessEventRuleList& rules );
    //TODO/IMPL deliver runtimeInfoChanged( const ec2::QnRuntimeInfo& runtimeInfo );
    return true;
}

void QnTransactionMessageBus::queueSyncRequest(QnTransactionTransport* transport)
{
    // send sync request
    transport->setReadSync(false);
    QnTransaction<QnTranState> requestTran(ApiCommand::tranSyncRequest, false);
    requestTran.params = transactionLog->getTransactionsState();
    requestTran.fillSequence();
    transport->sendTransaction(requestTran, QnPeerSet() << transport->remotePeer().id);
}

void QnTransactionMessageBus::connectToPeerLost(const QnId& id)
{
    if (m_alivePeers.contains(id)) {
        QnPeerInfo peer = m_alivePeers.value(id).peer;
        QList<QByteArray> hwList = m_alivePeers.value(id).hwList;
        m_alivePeers.remove(id);
        sendServerAliveMsg(peer, false, hwList);
    }
}

void QnTransactionMessageBus::connectToPeerEstablished(const QnPeerInfo &peer, const QList<QByteArray>& hwList)
{
    if (!m_alivePeers.contains(peer.id)) {
        m_alivePeers.insert(peer.id, AlivePeerInfo(peer, false, hwList));
        sendServerAliveMsg(peer, true, hwList);
    }
}

void QnTransactionMessageBus::sendServerAliveMsg(const QnPeerInfo &peer, bool isAlive, const QList<QByteArray>& hwList)
{
    m_aliveSendTimer.restart();
    QnTransaction<ApiPeerAliveData> tran(ApiCommand::serverAliveInfo, false);
    tran.params.peerId = peer.id;
    tran.params.peerType = static_cast<int>(peer.peerType);
    tran.params.isAlive = isAlive;
    tran.params.hardwareIds = hwList;
    tran.fillSequence();
    sendTransaction(tran);
    if (isAlive)
        emit peerFound(tran.params, false);
    else
        emit peerLost(tran.params, false);
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
        connectToPeerEstablished(transport->remotePeer(), transport->hwList());
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
            transport->setHwList(qnLicensePool->allLocalHardwareIds());
            connect(transport.data(), &QnTransactionTransport::gotTransaction, this, &QnTransactionMessageBus::at_gotTransaction,  Qt::QueuedConnection);
            connect(transport.data(), &QnTransactionTransport::stateChanged, this, &QnTransactionMessageBus::at_stateChanged,  Qt::QueuedConnection);
            transport->doOutgoingConnect(url);
            m_connectingConnections << transport;
        }
    }

    // send keep-alive if we connected to cloud
    if( !m_aliveSendTimer.isValid() )
        m_aliveSendTimer.restart();
    if (m_aliveSendTimer.elapsed() > ALIVE_UPDATE_INTERVAL)
        sendServerAliveMsg(m_localPeer, true, qnLicensePool->allLocalHardwareIds());

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
                if (transport->remotePeer().id == itr.key() && transport->remotePeer().peerType == QnPeerInfo::Server) {
                    qWarning() << "No alive info during timeout. reconnect to peer" << transport->remotePeer().id;
                    transport->setState(QnTransactionTransport::Error);
                }
            }
        }
    }
}

void QnTransactionMessageBus::gotConnectionFromRemotePeer(QSharedPointer<AbstractStreamSocket> socket, const QnPeerInfo &remotePeer, QList<QByteArray> hwList)
{
    if (!dbManager)
    {
        qWarning() << "This peer connected to remote EC. Ignoring incoming connection";
        return;
    }

    /** Request all data to be sent to the client peers on the connect. */
    QnTransaction<ApiFullInfoData> tran;
    if (remotePeer.isClient()) {
        tran.command = ApiCommand::getAllDataList;
        tran.id.peerID = m_localPeer.id;
        const ErrorCode errorCode = dbManager->doQuery(nullptr, tran.params);
        if (errorCode != ErrorCode::ok) {
            qWarning() << "Can't execute query for sync with client peer!";
            QnTransactionTransport::connectDone(remotePeer.id); // release connection
            return;
        }
    }

    QnTransactionTransportPtr transport(new QnTransactionTransport(m_localPeer, remotePeer, socket));
    transport->setHwList(hwList);
    connect(transport.data(), &QnTransactionTransport::gotTransaction, this, &QnTransactionMessageBus::at_gotTransaction,  Qt::QueuedConnection);
    connect(transport.data(), &QnTransactionTransport::stateChanged, this, &QnTransactionMessageBus::at_stateChanged,  Qt::QueuedConnection);

    transport->setState(QnTransactionTransport::Connected);

    /** Send all data to the client peers on the connect. */
    if (remotePeer.isClient()) 
    {
        transport->setWriteSync(true);
        transport->sendTransaction(tran, QnPeerSet() << remotePeer.id << m_localPeer.id);
        transport->setReadSync(true);
    }
    
    QMutexLocker lock(&m_mutex);
    if (m_connections[remotePeer.id]) 
        m_connectionsToRemove << m_connections[remotePeer.id];
    m_connections[remotePeer.id] = transport;
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

void QnTransactionMessageBus::setLocalPeer(const QnPeerInfo localPeer) {
    m_localPeer = localPeer;
}

ec2::QnPeerInfo QnTransactionMessageBus::localPeer() const {
    return m_localPeer;
}

template class QnTransactionMessageBus::CustomHandler<RemoteEC2Connection>;
template class QnTransactionMessageBus::CustomHandler<Ec2DirectConnection>;

}
