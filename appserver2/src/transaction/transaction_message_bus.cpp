#include "transaction_message_bus.h"
#include "remote_ec_connection.h"
#include "utils/network/aio/aioservice.h"
#include "utils/common/systemerror.h"
#include "ec2_connection.h"
#include "common/common_module.h"
#include "transaction_transport.h"
#include "transaction_transport_serializer.h"
#include "utils/common/synctime.h"
#include "nx_ec/data/server_alive_data.h"
#include "transaction_log.h"

namespace ec2
{

static const int RECONNECT_TIMEOUT = 1000 * 5;
static const int ALIVE_UPDATE_INTERVAL = 1000 * 60;

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
    m_serializer(*this),
    m_handler(nullptr),
    m_timer(nullptr), 
    m_mutex(QMutex::Recursive),
    m_thread(nullptr)
{
    m_thread = new QThread();
    m_thread->setObjectName("QnTransactionMessageBusThread");
    m_thread->start();
    moveToThread(m_thread);

    qRegisterMetaType<QnTransactionTransport::State>();

    m_timer = new QTimer();
    connect(m_timer, &QTimer::timeout, this, &QnTransactionMessageBus::at_timer);
    m_timer->start(500);
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

void QnTransactionMessageBus::onGotServerAliveInfo(const QnAbstractTransaction& abstractTran, InputBinaryStream<QByteArray>& stream)
{
    QnTransaction<ApiServerAliveData> tran(abstractTran);
    if (!deserialize(tran.params, &stream)) {
        qWarning() << "Got invalid ApiServerAliveData transaction. Ignoring";
        return;
    }

    // proxy alive info from non-direct connected host
    AlivePeersMap::iterator itr = m_alivePeers.find(tran.params.serverId);
    if (tran.params.isAlive && itr == m_alivePeers.end()) {
        m_alivePeers.insert(tran.params.serverId, AlivePeerInfo(tran.params.isClient, true));
        emit peerFound(tran.params.serverId, tran.params.isClient, true);
    }
    else if (!tran.params.isAlive && itr != m_alivePeers.end()) {
        emit peerLost(tran.params.serverId, tran.params.isClient, true);
        m_alivePeers.remove(tran.params.serverId);
    }
}

void QnTransactionMessageBus::at_gotTransaction(QByteArray serializedTran, QSet<QnId> processedPeers)
{
    QnTransactionTransport* sender = (QnTransactionTransport*) this->sender();
    if (!sender || sender->getState() != QnTransactionTransport::ReadyForStreaming)
        return;

    QnAbstractTransaction tran;
    InputBinaryStream<QByteArray> stream(serializedTran);
    if (!deserialize(tran, &stream)) {
        qWarning() << Q_FUNC_INFO << "Ignore bad transaction data. size=" << serializedTran.size();
        sender->setState(QnTransactionTransport::Error);
        return;
    }
    tran.timestamp -= sender->timeDiff();

    qDebug() << "got transaction " << ApiCommand::toString(tran.command);

    AlivePeersMap:: iterator itr = m_alivePeers.find(tran.id.peerGUID);
    if (itr != m_alivePeers.end())
        itr.value().lastActivity.restart();

    // process special transactions
    switch(tran.command)
    {
    case ApiCommand::lockRequest:
    case ApiCommand::lockResponse:
    case ApiCommand::unlockRequest:
        onGotDistributedMutexTransaction(tran, stream);

    case ApiCommand::tranSyncRequest:
        onGotTransactionSyncRequest(sender, stream);
        return;
    case ApiCommand::tranSyncResponse:
        onGotTransactionSyncResponse(sender, stream);
        return;
    case ApiCommand::serverAliveInfo:
        onGotServerAliveInfo(tran, stream);
        break; // do not return. proxy this transaction
    default:
        break;
    }
    
    if (!sender->isReadSync())
        return;

    if (m_handler && !m_handler->processByteArray(sender, serializedTran)) {
        qWarning() << "Can't handle transaction" << ApiCommand::Value(tran.command) << "reopen connection";
        sender->setState(QnTransactionTransport::Error);
        return;
    }

    QMutexLocker lock(&m_mutex);

    // proxy incoming transaction to other peers.
    QByteArray chunkData;
    m_serializer.serializeTran(chunkData, serializedTran, processedPeers);

    for(QnConnectionMap::iterator itr = m_connections.begin(); itr != m_connections.end(); ++itr)
    {
        QnTransactionTransportPtr transport = *itr;
        if (!processedPeers.contains(transport->remoteGuid()) && transport->isWriteSync())
            transport->addData(chunkData);
    }
}

void QnTransactionMessageBus::sendTransactionInternal(const QnAbstractTransaction& /*tran*/, const QByteArray& chunkData, const QnId& dstPeer)
{
    QMutexLocker lock(&m_mutex);
    bool dataQueued = false;
    for (QnConnectionMap::iterator itr = m_connections.begin(); itr != m_connections.end(); ++itr)
    {
        QnTransactionTransportPtr transport = *itr;
        if (dstPeer.isNull() || transport->remoteGuid() == dstPeer) {
            transport->addData(chunkData);
            dataQueued = true;
        }
    }
    
    // proxy via first connection
    if (!m_connections.isEmpty() && !dataQueued) {
        QnTransactionTransportPtr transport = m_connections.first();
        transport->addData(chunkData);
    }
}

// ------------------ QnTransactionMessageBus::CustomHandler -------------------


template <class T>
template <class T2>
bool QnTransactionMessageBus::CustomHandler<T>::deliveryTransaction(const QnAbstractTransaction&  abstractTran, InputBinaryStream<QByteArray>& stream)
{
    QnTransaction<T2> tran(abstractTran);
    if (!deserialize(tran.params, &stream))
        return false;
    
    if (abstractTran.persistent && transactionLog && transactionLog->contains(tran))
        return true; // transaction already processed

    // trigger notification, update local data e.t.c
    if (!m_handler->template processIncomingTransaction<T2>(tran, stream.buffer()))
        return false;

    return true;
}

void QnTransactionMessageBus::onGotDistributedMutexTransaction(const QnAbstractTransaction& tran, InputBinaryStream<QByteArray>& stream)
{
    ApiLockData params;
    if (!deserialize(params, &stream)) {
        qWarning() << "Bad stream! Ignore transaction " << ApiCommand::toString(tran.command);
    }

    if(tran.command == ApiCommand::lockRequest)
        emit gotLockRequest(params);
    else if(tran.command == ApiCommand::unlockRequest)
        emit gotUnlockRequest(params);
    else if(tran.command == ApiCommand::lockResponse)
        emit gotLockResponse(params);
    else
        Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid command type!");
}

void QnTransactionMessageBus::onGotTransactionSyncResponse(QnTransactionTransport* sender, InputBinaryStream<QByteArray>&)
{
	sender->setReadSync(true);
}

bool QnTransactionMessageBus::onGotTransactionSyncRequest(QnTransactionTransport* sender, InputBinaryStream<QByteArray>& stream)
{
    sender->setWriteSync(true);

    QnTranState params;
    if (deserialize(params, &stream))
    {

        QList<QByteArray> transactions;
        const ErrorCode errorCode = transactionLog->getTransactionsAfter(params, transactions);
        if (errorCode == ErrorCode::ok) 
        {
            QnTransaction<int> tran(ApiCommand::tranSyncResponse, false);
            tran.params = 0;
            QByteArray chunkData;
            m_serializer.serializeTran(chunkData, tran);
            sender->addData(chunkData);

            foreach(const QByteArray& serializedTran, transactions) {
                QByteArray chunkData;
                m_serializer.serializeTran(chunkData, serializedTran);
                sender->addData(chunkData);
            }
            return true;
        }
        else {
            qWarning() << "Can't execute query for sync with server peer!";
        }
    }
    return false;
}

template <class T>
bool QnTransactionMessageBus::CustomHandler<T>::processByteArray(QnTransactionTransport* /*sender*/, const QByteArray& data)
{
    Q_ASSERT(data.size() > 4);
    InputBinaryStream<QByteArray> stream(data);
    QnAbstractTransaction  abstractTran;
    if (!deserialize(abstractTran, &stream))
        return false; // bad data

    switch (abstractTran.command)
    {
        case ApiCommand::getAllDataList:
            return deliveryTransaction<ApiFullData>(abstractTran, stream);

        //!ApiSetResourceStatusData
        case ApiCommand::setResourceStatus:
            return deliveryTransaction<ApiSetResourceStatusData>(abstractTran, stream);
        //!ApiSetResourceDisabledData
        case ApiCommand::setResourceDisabled:
            return deliveryTransaction<ApiSetResourceDisabledData>(abstractTran, stream);
        //!ApiResourceParams
        case ApiCommand::setResourceParams:
            return deliveryTransaction<ApiResourceParams>(abstractTran, stream);
        case ApiCommand::saveResource:
            return deliveryTransaction<ApiResource>(abstractTran, stream);
        case ApiCommand::removeResource:
            return deliveryTransaction<ApiIdData>(abstractTran, stream);
        case ApiCommand::setPanicMode:
            return deliveryTransaction<ApiPanicModeData>(abstractTran, stream);
            
        case ApiCommand::saveCamera:
            return deliveryTransaction<ApiCamera>(abstractTran, stream);
        case ApiCommand::saveCameras:
            return deliveryTransaction<ApiCameraList>(abstractTran, stream);
        case ApiCommand::removeCamera:
            return deliveryTransaction<ApiIdData>(abstractTran, stream);
        case ApiCommand::addCameraHistoryItem:
            return deliveryTransaction<ApiCameraServerItemData>(abstractTran, stream);

        case ApiCommand::saveMediaServer:
            return deliveryTransaction<ApiMediaServerData>(abstractTran, stream);
        case ApiCommand::removeMediaServer:
            return deliveryTransaction<ApiIdData>(abstractTran, stream);

        case ApiCommand::saveUser:
            return deliveryTransaction<ApiUserData>(abstractTran, stream);
        case ApiCommand::removeUser:
            return deliveryTransaction<ApiIdData>(abstractTran, stream);

        case ApiCommand::saveBusinessRule:
            return deliveryTransaction<ApiBusinessRuleData>(abstractTran, stream);
        case ApiCommand::removeBusinessRule:
            return deliveryTransaction<ApiIdData>(abstractTran, stream);

        case ApiCommand::saveLayouts:
            return deliveryTransaction<ApiLayoutDataList>(abstractTran, stream);
        case ApiCommand::saveLayout:
            return deliveryTransaction<ApiLayoutData>(abstractTran, stream);
        case ApiCommand::removeLayout:
            return deliveryTransaction<ApiIdData>(abstractTran, stream);
            
        case ApiCommand::addStoredFile:
        case ApiCommand::updateStoredFile:
            return deliveryTransaction<ApiStoredFileData>(abstractTran, stream);
        case ApiCommand::removeStoredFile:
            return deliveryTransaction<ApiStoredFilePath>(abstractTran, stream);

        case ApiCommand::broadcastBusinessAction:
            return deliveryTransaction<ApiBusinessActionData>(abstractTran, stream);

        case ApiCommand::saveSettings:
            return deliveryTransaction<ApiParamList>(abstractTran, stream);
        case ApiCommand::addLicenses:
            return deliveryTransaction<ApiLicenseList>(abstractTran, stream);

        case ApiCommand::testEmailSettings:
            abstractTran.localTransaction = true;
            return deliveryTransaction<ApiEmailSettingsData>(abstractTran, stream);

        case ApiCommand::serverAliveInfo:
            break; // nothing to do

        default:
            Q_ASSERT_X(0, Q_FUNC_INFO, "Transaction type is not implemented for delivery! Implement me!");
            break;
    }

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
    
    QByteArray syncRequest;
    m_serializer.serializeTran(syncRequest, requestTran);
    transport->addData(syncRequest);
}

void QnTransactionMessageBus::connectToPeerLost(const QnId& id)
{
    if (m_alivePeers.contains(id)) {
        bool isClient = m_alivePeers.value(id).isClient;
        m_alivePeers.remove(id);
        sendServerAliveMsg(id, false, isClient);
    }
}

void QnTransactionMessageBus::connectToPeerEstablished(const QnId& id, bool isClient)
{
    if (!m_alivePeers.contains(id)) {
        m_alivePeers.insert(id, AlivePeerInfo(isClient, false));
        sendServerAliveMsg(id, true, isClient);
    }
}

void QnTransactionMessageBus::sendServerAliveMsg(const QnId& serverId, bool isAlive, bool isClient)
{
    m_aliveSendTimer.restart();
    QnTransaction<ApiServerAliveData> tran(ApiCommand::serverAliveInfo, false);
    tran.params.serverId = serverId;
    tran.params.isAlive = isAlive;
    tran.params.isClient = isClient;
    sendTransaction(tran);
    if (isAlive)
        emit peerFound(serverId, isClient, false);
    else
        emit peerLost(serverId, isClient, false);
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
                m_connections[transport->remoteGuid()] = m_connectingConnections[i];
                m_connectingConnections.removeAt(i);
                break;
            }
        }

        transport->setState(QnTransactionTransport::ReadyForStreaming);
        // if sync already done or in progress do not send new request
        if (!transport->isClientPeer())
            queueSyncRequest(transport);
        connectToPeerEstablished(transport->remoteGuid(), transport->isClientPeer());
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
                connectToPeerLost(transport->remoteGuid());
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
    for (QMap<QUrl, RemoveUrlConnectInfo>::iterator itr = m_removeUrls.begin(); itr != m_removeUrls.end(); ++itr)
    {
        const QUrl& url = itr.key();
        bool isClient = itr.value().isClient;
        if (currentTime - itr.value().lastConnectedTime > RECONNECT_TIMEOUT && !isPeerUsing(url)) 
        {
            itr.value().lastConnectedTime = currentTime;
            QnTransactionTransportPtr transport(new QnTransactionTransport(true, isClient));
            connect(transport.data(), &QnTransactionTransport::gotTransaction, this, &QnTransactionMessageBus::at_gotTransaction,  Qt::QueuedConnection);
            connect(transport.data(), &QnTransactionTransport::stateChanged, this, &QnTransactionMessageBus::at_stateChanged,  Qt::QueuedConnection);
            transport->doOutgoingConnect(url);
            m_connectingConnections << transport;
        }
    }

    // send keep-alive if we connected to cloud
    if (m_aliveSendTimer.elapsed() > ALIVE_UPDATE_INTERVAL)
        sendServerAliveMsg(qnCommon->moduleGUID(), true, qnCommon->isCloudMode());

    // check if some server not accessible any more
    for (AlivePeersMap::iterator itr = m_alivePeers.begin(); itr != m_alivePeers.end(); ++itr)
    {
        if (itr.value().lastActivity.elapsed() > ALIVE_UPDATE_INTERVAL * 2)
        {
            itr.value().lastActivity.restart();
            foreach(QSharedPointer<QnTransactionTransport> transport, m_connectingConnections) {
                if (transport->remoteGuid() == itr.key()) {
                    qWarning() << "No alive info during timeout. reconnect to peer" << transport->remoteGuid();
                    transport->setState(QnTransactionTransport::Error);
                }
            }

            foreach(QSharedPointer<QnTransactionTransport> transport, m_connections.values()) {
                if (transport->remoteGuid() == itr.key()) {
                    qWarning() << "No alive info during timeout. reconnect to peer" << transport->remoteGuid();
                    transport->setState(QnTransactionTransport::Error);
                }
            }
        }
    }
}

void QnTransactionMessageBus::gotConnectionFromRemotePeer(QSharedPointer<AbstractStreamSocket> socket, bool isClient, const QnId& remoteGuid, qint64 timediff)
{
    QnTransaction<ApiFullData> tran;
    if (isClient) 
    {
        tran.command = ApiCommand::getAllDataList;
        tran.id.peerGUID = qnCommon->moduleGUID();
        const ErrorCode errorCode = dbManager->doQuery(nullptr, tran.params);
        if (errorCode != ErrorCode::ok) {
            qWarning() << "Can't execute query for sync with client peer!";
            QnTransactionTransport::connectDone(remoteGuid); // release connection
            return;
        }
    }

    QnTransactionTransportPtr transport(new QnTransactionTransport(false, isClient, socket, remoteGuid));
    connect(transport.data(), &QnTransactionTransport::gotTransaction, this, &QnTransactionMessageBus::at_gotTransaction,  Qt::QueuedConnection);
    connect(transport.data(), &QnTransactionTransport::stateChanged, this, &QnTransactionMessageBus::at_stateChanged,  Qt::QueuedConnection);

    transport->setState(QnTransactionTransport::Connected);
    transport->setTimeDiff(timediff);

    // send fullsync to a client immediatly
    if (isClient) 
    {
        transport->setWriteSync(true);
        QByteArray buffer;
        m_serializer.serializeTran(buffer, tran);
        transport->addData(buffer);
    }
    
    QMutexLocker lock(&m_mutex);
    if (m_connections[remoteGuid]) 
        m_connectionsToRemove << m_connections[remoteGuid];
    m_connections[remoteGuid] = transport;
}

void QnTransactionMessageBus::addConnectionToPeer(const QUrl& url, bool isClient)
{
    QMutexLocker lock(&m_mutex);
    m_removeUrls.insert(url, RemoveUrlConnectInfo(isClient));
    QTimer::singleShot(0, this, SLOT(doPeriodicTasks()));
}

void QnTransactionMessageBus::removeConnectionFromPeer(const QUrl& url)
{
    QMutexLocker lock(&m_mutex);
    m_removeUrls.remove(url);
    QString urlStr = getUrlAddr(url);
    foreach(QnTransactionTransportPtr transport, m_connections.values())
    {
        if (getUrlAddr(transport->remoteAddr()) == urlStr) {
            qWarning() << "Disconnected from peer" << url;
            transport->setState(QnTransactionTransport::Error);
        }
    }
}

template class QnTransactionMessageBus::CustomHandler<RemoteEC2Connection>;
template class QnTransactionMessageBus::CustomHandler<Ec2DirectConnection>;

}
