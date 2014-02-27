#include "transaction_message_bus.h"
#include "remote_ec_connection.h"
#include "utils/network/aio/aioservice.h"
#include "utils/common/systemerror.h"
#include "ec2_connection.h"
#include "common/common_module.h"
#include "transaction_transport.h"

namespace ec2
{

static const int RECONNECT_TIMEOUT = 1000 * 5;

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

QnTransactionMessageBus::QnTransactionMessageBus(): m_timer(0), m_thread(0), m_mutex(QMutex::Recursive)
{
    m_thread = new QThread();
    m_thread->setObjectName("QnTransactionMessageBusThread");
    m_thread->start();
    moveToThread(m_thread);

    m_timer = new QTimer();
    connect(m_timer, &QTimer::timeout, this, &QnTransactionMessageBus::at_timer);
    m_timer->start(1);
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

QSharedPointer<QnTransactionTransport> QnTransactionMessageBus::getSibling(QSharedPointer<QnTransactionTransport> transport)
{
    QnConnectionMap::iterator itr = m_connections.find(transport->removeGuid());
    if (itr == m_connections.end())
        return QSharedPointer<QnTransactionTransport>();
    ConnectionsToPeer& data = itr.value();
    return transport->isOriginator() ? data.incomeConn : data.outcomeConn;
}

void QnTransactionMessageBus::at_gotTransaction(QByteArray serializedTran)
{
    QnTransactionTransport* sender = (QnTransactionTransport*) this->sender();
    if (!sender || sender->getState() != QnTransactionTransport::ReadyForStreaming)
        return;

    QnAbstractTransaction tran;
    InputBinaryStream<QByteArray> stream(serializedTran);
    if (!tran.deserialize(&stream)) {
        qWarning() << Q_FUNC_INFO << "Ignore bad transaction data. size=" << serializedTran.size();
        sender->setState(QnTransactionTransport::Error);
        return;
    }

    if (tran.command == ApiCommand::tranSyncRequest) {
        onGotTransactionSyncRequest(sender, stream);
        return;
    }
    else if (tran.command == ApiCommand::tranSyncResponse) {
        onGotTransactionSyncResponse(sender);
        return;
    }
    
    if (!sender->isReadSync())
        return;

    if (m_handler && !m_handler->processByteArray(sender, serializedTran)) {
        sender->setState(QnTransactionTransport::Error);
        return;
    }

    QMutexLocker lock(&m_mutex);
    // proxy incoming transaction to other peers.
    QByteArray chunkData;
    QnTransactionMessageBus::serializeTransaction(chunkData, serializedTran);

    for(QnConnectionMap::iterator itr = m_connections.begin(); itr != m_connections.end(); ++itr)
    {
        if (itr.key() == sender->removeGuid())
            continue;
        ConnectionsToPeer& connection = itr.value();
        connection.proxyIncomingTransaction(tran, chunkData);
    }
}

void QnTransactionMessageBus::sendTransactionInternal(const QnId& originGuid, const QByteArray& chunkData)
{
    QMutexLocker lock(&m_mutex);
    for (QnConnectionMap::iterator itr = m_connections.begin(); itr != m_connections.end(); ++itr)
    {
        if (itr.key() == originGuid)
            continue; // do not send transaction back to originator.
        ConnectionsToPeer& data = itr.value();
        data.sendOutgoingTran(chunkData);
    }
}

// ------------------ QnTransactionMessageBus::ConnectionsToPeer  -------------------

void QnTransactionMessageBus::ConnectionsToPeer::proxyIncomingTransaction(const QnAbstractTransaction& tran, const QByteArray& chunkData)
{
    // proxy data to connected clients only. You can update this function to peer-to-peer sync via proxy in future
    if (incomeConn && incomeConn->getState() == QnTransactionTransport::ReadyForStreaming && incomeConn->isClientPeer()) {
        incomeConn->addData(chunkData);
    }
}

void QnTransactionMessageBus::ConnectionsToPeer::sendOutgoingTran(const QByteArray& chunkData)
{
    if (incomeConn && incomeConn->isWriteSync())
        incomeConn->addData(chunkData);
    else if (outcomeConn && outcomeConn->isWriteSync())
        outcomeConn->addData(chunkData);
}

// ------------------ QnTransactionMessageBus::CustomHandler -------------------

template <class T>
template <class T2>
bool QnTransactionMessageBus::CustomHandler<T>::deliveryTransaction(const QnAbstractTransaction&  abstractTran, InputBinaryStream<QByteArray>& stream)
{
    QnTransaction<T2> tran(abstractTran);
    if (!QnBinary::deserialize(tran.params, &stream))
        return false;
    
    // trigger notification, update local data e.t.c
    if (!m_handler->processIncomingTransaction<T2>(tran, stream.buffer()))
        return false;

    return true;
}

void QnTransactionMessageBus::onGotTransactionSyncResponse(QnTransactionTransport* sender)
{
    sender->setReadSync(true);
}

bool QnTransactionMessageBus::onGotTransactionSyncRequest(QnTransactionTransport* sender, InputBinaryStream<QByteArray>& stream)
{
    sender->setWriteSync(true);

    QnTranState params;
    if (QnBinary::deserialize(params, &stream))
    {
        QList<QByteArray> transactions;
        const ErrorCode errorCode = transactionLog->getTransactionsAfter(params, transactions);
        if (errorCode == ErrorCode::ok) 
        {
            QnTransaction<int> tran(ApiCommand::tranSyncResponse, false);
            tran.params = 0;
            QByteArray chunkData;
            serializeTransaction(chunkData, tran);
            sender->addData(chunkData);

            foreach(const QByteArray& data, transactions) {
                QByteArray chunkData;
                serializeTransaction(chunkData, data);
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
bool QnTransactionMessageBus::CustomHandler<T>::processByteArray(QnTransactionTransport* sender, const QByteArray& data)
{
    Q_ASSERT(data.size() > 4);
    InputBinaryStream<QByteArray> stream(data);
    QnAbstractTransaction  abstractTran;
    if (!QnBinary::deserialize(abstractTran, &stream))
        return false; // bad data
    if (abstractTran.persistent && transactionLog && transactionLog->contains(abstractTran.id))
        return true; // transaction already processed

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
            return deliveryTransaction<ApiResourceData>(abstractTran, stream);
        case ApiCommand::removeResource:
            return deliveryTransaction<ApiIdData>(abstractTran, stream);
        case ApiCommand::setPanicMode:
            return deliveryTransaction<ApiPanicModeData>(abstractTran, stream);
            
        case ApiCommand::saveCamera:
            return deliveryTransaction<ApiCameraData>(abstractTran, stream);
        case ApiCommand::saveCameras:
            return deliveryTransaction<ApiCameraDataList>(abstractTran, stream);
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

        default:
            break;
    }

    //TODO/IMPL deliver businessActionBroadcasted( const QnAbstractBusinessActionPtr& businessAction );
    //TODO/IMPL deliver businessRuleReset( const QnBusinessEventRuleList& rules );
    //TODO/IMPL deliver runtimeInfoChanged( const ec2::QnRuntimeInfo& runtimeInfo );

    return true;
}

void QnTransactionMessageBus::processConnState(QSharedPointer<QnTransactionTransport> &transport)
{
    switch (transport->getState()) 
    {
    case QnTransactionTransport::Error: 
        {
        transport->close();
        QSharedPointer<QnTransactionTransport> sibling = getSibling(transport);
        if (sibling && sibling->getState() == QnTransactionTransport::ReadyForStreaming) 
            sibling->close();
        break;
    }
    case QnTransactionTransport::Connected:
    {
        transport->setState(QnTransactionTransport::ReadyForStreaming);
        QSharedPointer<QnTransactionTransport> subling = getSibling(transport);
        // if sync already done or in progress do not send new request
        if (!transport->isClientPeer() && (!subling || !subling->isReadSync()))
            transport->sendSyncRequest();
        break;
    }
    case QnTransactionTransport::Connect:
        {
            qint64 ct = QDateTime::currentDateTime().toMSecsSinceEpoch();
            if (ct - transport->lastConnectTime() >= RECONNECT_TIMEOUT) {
                transport->setLastConnectTime(ct);
                transport->setState(QnTransactionTransport::Connecting);
                for(RemoveUrlMap::iterator itr = m_remoteUrls.begin(); itr != m_remoteUrls.end(); ++itr) 
                {
                    if (itr.value() == transport)
                        transport->doOutgoingConnect(itr.key());
                }
            }
            break;
        }
    case QnTransactionTransport::Closed:
        transport.clear();
        break;
    }
}

void QnTransactionMessageBus::at_timer()
{
    QMutexLocker lock(&m_mutex);

    m_connectionsToRemove.clear();

    for (int i = m_connectingConnections.size() -1; i >= 0; --i) 
    {
        QSharedPointer<QnTransactionTransport> transport = m_connectingConnections[i];
        if (transport->getState() == QnTransactionTransport::Connected) {
            m_connections[transport->removeGuid()].outcomeConn = transport;
            m_connectingConnections.removeAt(i);

        }
        else {
            processConnState(m_connectingConnections[i]);
            if (!m_connectingConnections[i])
                m_connectingConnections.removeAt(i);
        }
    }


    for(QnConnectionMap::iterator itr = m_connections.begin(); itr != m_connections.end();)
    {
        ConnectionsToPeer& c = itr.value();
        if (c.incomeConn)
            processConnState(itr.value().incomeConn);
        if (c.outcomeConn)
            processConnState(itr.value().outcomeConn);
            
        if (!c.incomeConn && !c.outcomeConn)
            itr = m_connections.erase(itr);
        else {
            ++itr;
        }
    }
}

void QnTransactionMessageBus::toFormattedHex(quint8* dst, quint32 payloadSize)
{
    for (;payloadSize; payloadSize >>= 4) {
        quint8 digit = payloadSize % 16;
        *dst-- = digit < 10 ? digit + '0': digit + 'A'-10;
    }
}

void QnTransactionMessageBus::gotConnectionFromRemotePeer(QSharedPointer<AbstractStreamSocket> socket, const QUrlQuery& query)
{
    bool isClient = query.hasQueryItem("isClient");
    QUuid remoteGuid  = query.queryItemValue(lit("guid"));

    if (remoteGuid.isNull()) {
        qWarning() << "Invalid incoming request. GUID must be filled!";
        return;
    }

    QnTransaction<ApiFullData> data;
    if (isClient) 
    {
        data.command = ApiCommand::getAllDataList;
        const ErrorCode errorCode = dbManager->doQuery(nullptr, data.params);
        if (errorCode != ErrorCode::ok) {
            qWarning() << "Can't execute query for sync with client peer!";
            return;
        }
    }


    QnTransactionTransport* transport = new QnTransactionTransport(false, isClient, socket, remoteGuid);
    connect(transport, &QnTransactionTransport::gotTransaction, this, &QnTransactionMessageBus::at_gotTransaction,  Qt::QueuedConnection);
    transport->setState(QnTransactionTransport::Connected);

    // send fullsync to a client immediatly
    if (isClient) 
    {
        transport->setWriteSync(true);
        QByteArray buffer;
        serializeTransaction(buffer, data);
        transport->addData(buffer);
    }
    
    QMutexLocker lock(&m_mutex);
    if (m_connections[remoteGuid].incomeConn)
        m_connectionsToRemove << m_connections[remoteGuid].incomeConn;
    m_connections[remoteGuid].incomeConn.reset(transport);
}

void QnTransactionMessageBus::addConnectionToPeer(const QUrl& url, bool isClient)
{
    QMutexLocker lock(&m_mutex);
    
    QSharedPointer<QnTransactionTransport> transport(new QnTransactionTransport(true, isClient));
    connect(transport.data(), &QnTransactionTransport::gotTransaction, this, &QnTransactionMessageBus::at_gotTransaction,  Qt::QueuedConnection);

    m_remoteUrls.insert(url, transport);
    transport->setState(QnTransactionTransport::Connect);
    m_connectingConnections <<  QSharedPointer<QnTransactionTransport>(transport);
}

void QnTransactionMessageBus::removeConnectionFromPeer(const QUrl& url)
{
    QMutexLocker lock(&m_mutex);
    QSharedPointer<QnTransactionTransport> transport = m_remoteUrls.value(url);
    if (transport)
        transport->setState(QnTransactionTransport::Closed);
    m_remoteUrls.remove(url);
}

/*
void QnTransactionMessageBus::gotTransaction(const QnId& remoteGuid, bool isConnectionOriginator, const QByteArray& data)
{
    emit sendGotTransaction(remoteGuid, isConnectionOriginator, data);
}
*/

template class QnTransactionMessageBus::CustomHandler<RemoteEC2Connection>;
template class QnTransactionMessageBus::CustomHandler<Ec2DirectConnection>;

}
