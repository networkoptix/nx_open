#include "transaction_message_bus.h"

#include <chrono>

#include <QtCore/QTimer>
#include <QtCore/QTextStream>

#include <api/global_settings.h>
#include "api/app_server_connection.h"
#include "api/runtime_info_manager.h"

#include "common/common_module.h"
#include "ec_connection_notification_manager.h"
#include "managers/time_manager.h"
#include "nx/vms/discovery/manager.h"
#include "settings.h"

#include "nx_ec/data/api_camera_data_ex.h"
#include "nx_ec/data/api_camera_data.h"
#include "nx_ec/data/api_resource_data.h"
#include "nx_ec/data/api_resource_type_data.h"
#include "nx_ec/data/api_reverse_connection_data.h"
#include "nx_ec/data/api_peer_alive_data.h"
#include "nx_ec/data/api_discovery_data.h"
#include <nx_ec/data/api_access_rights_data.h>

#include "transaction/runtime_transaction_log.h"
#include <transaction/transaction_transport.h>

#include <utils/common/checked_cast.h>
#include <nx/utils/log/log.h>
#include "utils/common/synctime.h"
#include <nx/utils/system_error.h>
#include "utils/common/warnings.h"
#include <core/resource/media_server_resource.h>
#include <nx/utils/random.h>
#include <core/resource_access/resource_access_manager.h>
#include "transaction_message_bus_priv.h"

namespace ec2 {

static const int RECONNECT_TIMEOUT = 1000 * 5;
static const std::chrono::seconds ALIVE_UPDATE_INTERVAL_OVERHEAD(10);
static const int ALIVE_UPDATE_PROBE_COUNT = 2;
static const int ALIVE_RESEND_TIMEOUT_MAX = 1000 * 5; // resend alive data after a random delay in range min..max
static const int ALIVE_RESEND_TIMEOUT_MIN = 100;
//!introduced to make discovery interval dependent of peer alive update interval
static const int PEER_DISCOVERY_BY_ALIVE_UPDATE_INTERVAL_FACTOR = 3;

QString printTransaction(const char* prefix, const QnAbstractTransaction& tran, const QnUuid& hash, const QnTransactionTransportHeader &transportHeader, QnTransactionTransport* sender)
{
    return lit("%1, hash=%2  %3 %4 gotVia=%5").arg(hash.toString()).arg(prefix).arg(tran.toString()).arg(toString(transportHeader)).arg(sender->remotePeer().id.toString());
}

struct GotTransactionFuction
{
    typedef void result_type;

    template<class T>
    void operator()(QnTransactionMessageBus *bus, const QnTransaction<T> &transaction, QnTransactionTransport *sender, const QnTransactionTransportHeader &transportHeader) const
    {
        bus->gotTransaction(transaction, sender, transportHeader);
    }
};

// --------------------------------- QnTransactionMessageBus ------------------------------

QnTransactionMessageBus::QnTransactionMessageBus(
    Qn::PeerType peerType,
    QnCommonModule* commonModule,
    QnJsonTransactionSerializer* jsonTranSerializer,
    QnUbjsonTransactionSerializer* ubjsonTranSerializer)
:
    TransactionMessageBusBase(peerType, commonModule, jsonTranSerializer, ubjsonTranSerializer),
    m_runtimeTransactionLog(new QnRuntimeTransactionLog(commonModule))
{
    m_thread->setObjectName("QnTransactionMessageBusThread");
    qRegisterMetaType<QnTransactionTransport::State>(); // TODO: #Elric #EC2 registration
    qRegisterMetaType<QnAbstractTransaction>("QnAbstractTransaction");
    qRegisterMetaType<QnTransaction<ApiRuntimeData> >("QnTransaction<ApiRuntimeData>");

    connect(m_thread, &QThread::started,
        [this]()
        {
            if (!m_timer)
            {
                m_timer = new QTimer();
                connect(m_timer, &QTimer::timeout, this, &QnTransactionMessageBus::doPeriodicTasks);
            }
            m_timer->start(500);
        });
    connect(m_thread, &QThread::finished, [this]() { m_timer->stop(); });

    m_aliveSendTimer.invalidate();
    m_currentTimeTimer.restart();

    connect(m_runtimeTransactionLog.get(), &QnRuntimeTransactionLog::runtimeDataUpdated, this, &QnTransactionMessageBus::at_runtimeDataUpdated);
    m_relativeTimer.restart();

    connect(
        commonModule->globalSettings(), &QnGlobalSettings::ec2ConnectionSettingsChanged,
        this, &QnTransactionMessageBus::onEc2ConnectionSettingsChanged);
}

void QnTransactionMessageBus::stop()
{
    dropConnections();
    base_type::stop();
    m_aliveSendTimer.invalidate();
}

QnTransactionMessageBus::~QnTransactionMessageBus()
{
    if (m_thread)
    {
        m_thread->exit();
        m_thread->wait();
    }

    for (QnTransactionTransport* transport : m_connections)
        delete transport;
    for (QnTransactionTransport* transport : m_connectingConnections)
        delete transport;

    delete m_timer;
}

void QnTransactionMessageBus::addAlivePeerInfo(const ApiPeerData& peerData, const QnUuid& gotFromPeer, int distance)
{
    AlivePeersMap::iterator itr = m_alivePeers.find(peerData.id);
    if (itr == m_alivePeers.end())
        itr = m_alivePeers.insert(peerData.id, peerData);
    AlivePeerInfo& currentValue = itr.value();

    currentValue.routingInfo.insert(gotFromPeer, RoutingRecord(distance, m_currentTimeTimer.elapsed()));
}

void QnTransactionMessageBus::removeTTSequenceForPeer(const QnUuid& id)
{
    NX_LOG(QnLog::EC2_TRAN_LOG, lit("Clear transportSequence for peer %1").arg(id.toString()), cl_logDEBUG1);

    ApiPersistentIdData key(id, QnUuid());
    auto itr = m_lastTransportSeq.lowerBound(key);
    while (itr != m_lastTransportSeq.end() && itr.key().id == id)
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

#ifdef _DEBUG
    if (m_alivePeers.isEmpty())
    {
        QnTranState runtimeState;
        QList<QnTransaction<ApiRuntimeData>> result;
        m_runtimeTransactionLog->getTransactionsAfter(runtimeState, result);
        const bool validPeerId = result.size() == 1 && result[0].peerID == commonModule()->moduleGUID();
        if (!validPeerId)
            NX_LOG("ASSERT(result.size() == 1 && result[0].peerID == commonModule()->moduleGUID())", cl_logERROR);
    }
#endif

    // 2. remove peers proxy via current peer
    if (isRecursive)
        return;
    QSet<QnUuid> morePeersToRemove;
    for (auto itr = m_alivePeers.begin(); itr != m_alivePeers.end(); ++itr)
    {
        AlivePeerInfo& otherPeer = itr.value();
        if (otherPeer.routingInfo.contains(id))
        {
            otherPeer.routingInfo.remove(id);
            if (otherPeer.routingInfo.isEmpty())
                morePeersToRemove << otherPeer.peer.id;
        }
    }
    for (const QnUuid& p : morePeersToRemove)
        removeAlivePeer(p, true, true);
}

void QnTransactionMessageBus::addDelayedAliveTran(QnTransaction<ApiPeerAliveData>&& tran, int timeout)
{
    DelayedAliveData data;
    data.tran = std::move(tran);
    data.timeToSend = m_relativeTimer.elapsed() + timeout;
    m_delayedAliveTran.insert(tran.params.peer.id, std::move(data));
}

void QnTransactionMessageBus::sendDelayedAliveTran()
{
    for (auto itr = m_delayedAliveTran.begin(); itr != m_delayedAliveTran.end();)
    {
        DelayedAliveData& data = itr.value();
        if (m_relativeTimer.elapsed() >= data.timeToSend)
        {
            bool isAliveNow = m_alivePeers.contains(data.tran.params.peer.id);
            bool isAliveDelayed = data.tran.params.isAlive;
            if (isAliveNow == isAliveDelayed)
            {
                QnTransactionTransportHeader ttHeader;
                if (data.tran.params.peer.id != commonModule()->moduleGUID())
                    ttHeader.distance = 1;
                ttHeader.processedPeers = connectedServerPeers() << commonModule()->moduleGUID();
                ttHeader.fillSequence(commonModule()->moduleGUID(), commonModule()->runningInstanceGUID());
                sendTransactionInternal(std::move(data.tran), ttHeader); // resend broadcast alive info for that peer
                itr = m_delayedAliveTran.erase(itr);
            }
            else
                ++itr;
        }
        else
        {
            ++itr;
        }
    }
}

void QnTransactionMessageBus::resyncWithPeer(QnTransactionTransport* transport)
{
	if (!transport->remotePeer().isClient() && !ApiPeerData::isClient(m_localPeerType))
		queueSyncRequest(transport);
	else
		transport->setState(QnTransactionTransport::Error);
}

bool QnTransactionMessageBus::gotAliveData(const ApiPeerAliveData &aliveData, QnTransactionTransport* transport, const QnTransactionTransportHeader* ttHeader)
{
    if (ttHeader->dstPeers.isEmpty())
        m_delayedAliveTran.remove(aliveData.peer.id); // cancel delayed status tran if we got new broadcast alive data for that peer

    QnUuid gotFromPeer;
    if (transport)
        gotFromPeer = transport->remotePeer().id;

    NX_LOG(QnLog::EC2_TRAN_LOG, lit("received peerAlive transaction. id=%1 type=%2 isAlive=%3").
        arg(aliveData.peer.id.toString()).arg(aliveData.peer.peerType).arg(aliveData.isAlive), cl_logDEBUG1);
    if (aliveData.peer.id == commonModule()->moduleGUID())
        return false; // ignore himself

#if 1
    if (!aliveData.isAlive && !gotFromPeer.isNull())
    {
        bool isPeerActuallyAlive = aliveData.peer.id == commonModule()->moduleGUID();
        auto itr = m_connections.find(aliveData.peer.id);
        if (itr != m_connections.end())
        {
            QnTransactionTransport* transport = itr.value();
            if (transport->getState() == QnTransactionTransport::ReadyForStreaming)
            {
                isPeerActuallyAlive = true;
            }
        }
        if (isPeerActuallyAlive)
        {
            // ignore incoming offline peer info because we can see that peer online
            QnTransaction<ApiPeerAliveData> tran(
                ApiCommand::peerAliveInfo,
                commonModule()->moduleGUID());
            tran.params = aliveData;
            tran.params.isAlive = true;
            NX_ASSERT(!aliveData.peer.instanceId.isNull());

            int delay = (aliveData.peer.id == commonModule()->moduleGUID())
                ? 0 : nx::utils::random::number<int>(
                ALIVE_RESEND_TIMEOUT_MIN, ALIVE_RESEND_TIMEOUT_MAX);

            addDelayedAliveTran(std::move(tran), delay);
            return false; // ignore peer offline transaction
        }
    }
#endif

    // proxy alive info from non-direct connected host
    bool isPeerExist = m_alivePeers.contains(aliveData.peer.id);
    if (aliveData.isAlive)
    {
        addAlivePeerInfo(ApiPeerData(aliveData.peer.id, aliveData.peer.instanceId, aliveData.peer.peerType), gotFromPeer, ttHeader->distance);
        if (!isPeerExist)
        {
            NX_LOG(QnLog::EC2_TRAN_LOG, lit("emit peerFound. id=%1").arg(aliveData.peer.id.toString()), cl_logDEBUG1);
            emit peerFound(aliveData.peer.id, aliveData.peer.peerType);
        }
    }
    else
    {
        if (isPeerExist)
        {
            removeAlivePeer(aliveData.peer.id, false);
        }
    }

    // check sequences

    if (aliveData.isAlive)
    {
        m_runtimeTransactionLog->clearOldRuntimeData(ApiPersistentIdData(aliveData.peer.id, aliveData.peer.instanceId));
    }

    if (transport && transport->isSyncDone() && aliveData.isAlive)
    {
        if (!aliveData.runtimeState.values.empty())
        {
            // check current persistent state
            if (!m_runtimeTransactionLog->contains(aliveData.runtimeState))
            {
                NX_LOG(QnLog::EC2_TRAN_LOG, lit("DETECT runtime transaction GAP via update message. Resync with peer %1").
                    arg(transport->remotePeer().id.toString()), cl_logDEBUG1);

				resyncWithPeer(transport);
            }
        }
    }

    return true;
}

void QnTransactionMessageBus::onGotServerAliveInfo(const QnTransaction<ApiPeerAliveData> &tran, QnTransactionTransport* transport, const QnTransactionTransportHeader& ttHeader)
{
    NX_ASSERT(tran.peerID != commonModule()->moduleGUID());
    if (!gotAliveData(tran.params, transport, &ttHeader))
        return; // ignore offline alive tran and resend online tran instead

    if (transport->remotePeer().peerType == Qn::PT_CloudServer)
        return; //< do not propagate cloud peer alive to other peers. It isn't used yet

    QnTransaction<ApiPeerAliveData> modifiedTran(tran);
    NX_ASSERT(!modifiedTran.params.peer.instanceId.isNull());
    modifiedTran.params.persistentState.values.clear(); // do not proxy persistent state to other peers. this checking required for directly connected peers only
    modifiedTran.params.runtimeState.values.clear();
    proxyTransaction(modifiedTran, ttHeader);
}

bool QnTransactionMessageBus::onGotServerRuntimeInfo(const QnTransaction<ApiRuntimeData> &tran, QnTransactionTransport* transport, const QnTransactionTransportHeader& ttHeader)
{
    if (tran.params.peer.id == commonModule()->moduleGUID())
        return false; // ignore himself

    gotAliveData(ApiPeerAliveData(tran.params.peer, true), transport, &ttHeader);
    if (m_runtimeTransactionLog->contains(tran))
        return false;
    else
    {
        m_runtimeTransactionLog->saveTransaction(tran);
        return true;
    }
}

void QnTransactionMessageBus::at_gotTransaction(
    Qn::SerializationFormat tranFormat,
    QByteArray serializedTran,
    const QnTransactionTransportHeader &transportHeader)
{
    QnTransactionTransport* sender = checked_cast<QnTransactionTransport*>(this->sender());

    //NX_LOG(QnLog::EC2_TRAN_LOG, lit("Got transaction sender = %1").arg((size_t) sender,  0, 16), cl_logDEBUG1);

    if (!sender || sender->getState() != QnTransactionTransport::ReadyForStreaming)
    {
        if (sender)
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

    if (!transportHeader.isNull())
        NX_ASSERT(transportHeader.processedPeers.contains(sender->remotePeer().id));

	handleIncomingTransaction(sender, tranFormat, serializedTran, transportHeader);

    //TODO #ak is it garanteed that sender is alive?
    sender->transactionProcessed();
}

void QnTransactionMessageBus::handleIncomingTransaction(
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

// ------------------ QnTransactionMessageBus::CustomHandler -------------------

void QnTransactionMessageBus::onGotDistributedMutexTransaction(const QnTransaction<ApiLockData>& tran)
{
    if (tran.command == ApiCommand::lockRequest)
        emit gotLockRequest(tran.params);
    else if (tran.command == ApiCommand::lockResponse)
        emit gotLockResponse(tran.params);
}

void QnTransactionMessageBus::onGotTransactionSyncResponse(QnTransactionTransport* sender, const QnTransaction<QnTranStateResponse>& /*tran*/)
{
    sender->setReadSync(true);
}

void QnTransactionMessageBus::onGotTransactionSyncDone(QnTransactionTransport* sender, const QnTransaction<ApiTranSyncDoneData>& /*tran*/)
{
    sender->setSyncDone(true);
    sender->setSyncInProgress(false);
    // propagate new data to other peers. Aka send current state, other peers should request update if need
    handlePeerAliveChanged(localPeer(), true, true);
    m_aliveSendTimer.restart();
}

bool QnTransactionMessageBus::checkSequence(const QnTransactionTransportHeader& transportHeader, const QnAbstractTransaction& tran, QnTransactionTransport* transport)
{
    if (ApiPeerData::isClient(m_localPeerType))
        return true;

    if (transportHeader.sender.isNull())
        return true; // old version, nothing to check

    // 1. check transport sequence
    ApiPersistentIdData ttSenderKey(transportHeader.sender, transportHeader.senderRuntimeID);
    int transportSeq = m_lastTransportSeq[ttSenderKey];
    if (transportSeq >= transportHeader.sequence)
    {
        NX_LOG(QnLog::EC2_TRAN_LOG, lit("Ignore transaction %1 %2 received via %3 because of transport sequence: %4 <= %5").
            arg(tran.toString()).arg(toString(transportHeader)).arg(transport->remotePeer().id.toString()).arg(transportHeader.sequence).arg(transportSeq), cl_logDEBUG1);
        return false; // already processed
    }
    m_lastTransportSeq[ttSenderKey] = transportHeader.sequence;

	return true;
}

void QnTransactionMessageBus::updateLastActivity(QnTransactionTransport* sender, const QnTransactionTransportHeader& transportHeader)
{
    auto itr = m_alivePeers.find(transportHeader.sender);
    if (itr == m_alivePeers.end())
        return;
    AlivePeerInfo& peerInfo = itr.value();
    const QnUuid& gotFromPeer = sender->remotePeer().id;
    peerInfo.routingInfo[gotFromPeer] = RoutingRecord(transportHeader.distance, m_currentTimeTimer.elapsed());
}

ErrorCode QnTransactionMessageBus::updatePersistentMarker(
    const QnTransaction<ApiUpdateSequenceData>& tran)
{
    return ErrorCode::notImplemented;
}

template <class T>
bool QnTransactionMessageBus::processSpecialTransaction(const QnTransaction<T> &tran, QnTransactionTransport* sender, const QnTransactionTransportHeader &transportHeader)
{
    QnMutexLocker lock(&m_mutex);

    // do not perform any logic (aka sequence update) for foreign transaction. Just proxy
    if (!transportHeader.dstPeers.isEmpty() && !transportHeader.dstPeers.contains(commonModule()->moduleGUID()))
    {
        if (nx::utils::log::isToBeLogged(nx::utils::log::Level::debug, QnLog::EC2_TRAN_LOG))
        {
            QString dstPeersStr;
            for (const QnUuid& peer : transportHeader.dstPeers)
                dstPeersStr += peer.toString();
            NX_LOG(QnLog::EC2_TRAN_LOG, lit("skip transaction %1 %2 for peers %3").arg(tran.toString()).arg(toString(transportHeader)).arg(dstPeersStr), cl_logDEBUG1);
        }
        proxyTransaction(tran, transportHeader);
        return true;
    }

    updateLastActivity(sender, transportHeader);

    auto transactionDescriptor = getTransactionDescriptorByTransaction(tran);
    auto transactionHash = transactionDescriptor ? transactionDescriptor->getHashFunc(tran.params) : QnUuid();

    if (!checkSequence(transportHeader, tran, sender))
        return true;

    if (!sender->isReadSync(tran.command))
    {
        NX_LOG(QnLog::EC2_TRAN_LOG, printTransaction("reject transaction (no readSync)", tran, transactionHash, transportHeader, sender), cl_logDEBUG1);
        return true;
    }

    if (tran.isLocal() && ApiPeerData::isServer(m_localPeerType))
    {
        NX_LOG(QnLog::EC2_TRAN_LOG, printTransaction("reject local transaction", tran, transactionHash, transportHeader, sender), cl_logDEBUG1);
        return true;
    }

    NX_LOG(QnLog::EC2_TRAN_LOG, printTransaction("got transaction", tran, transactionHash, transportHeader, sender), cl_logDEBUG1);
    // process system transactions
    switch (tran.command)
    {
        case ApiCommand::lockRequest:
        case ApiCommand::lockResponse:
        case ApiCommand::unlockRequest:
            onGotDistributedMutexTransaction(tran);
            break;
        case ApiCommand::tranSyncRequest:
            onGotTransactionSyncRequest(sender, tran);
            return true; // do not proxy
        case ApiCommand::tranSyncResponse:
            onGotTransactionSyncResponse(sender, tran);
            return true; // do not proxy
        case ApiCommand::tranSyncDone:
            onGotTransactionSyncDone(sender, tran);
            return true; // do not proxy
        case ApiCommand::peerAliveInfo:
            onGotServerAliveInfo(tran, sender, transportHeader);
            return true; // do not proxy. this call contains built in proxy
        case ApiCommand::forcePrimaryTimeServer:
            m_timeSyncManager->onGotPrimariTimeServerTran(tran);
            break;
        case ApiCommand::broadcastPeerSyncTime:
            m_timeSyncManager->resyncTimeWithPeer(tran.peerID);
            return true; // do not proxy.
        case ApiCommand::broadcastPeerSystemTime:
        case ApiCommand::getKnownPeersSystemTime:
            return true; // Ignore deprecated transactions
        case ApiCommand::runtimeInfoChanged:
            if (!onGotServerRuntimeInfo(tran, sender, transportHeader))
                return true; // already processed. do not proxy and ignore transaction
            if (m_handler)
                m_handler->triggerNotification(tran, NotificationSource::Remote);
            break;
        case ApiCommand::updatePersistentSequence:
            updatePersistentMarker(tran);
            break;
        case ApiCommand::installUpdate:
        case ApiCommand::uploadUpdate:
        case ApiCommand::changeSystemId:
        {	// Transactions listed here should not go to the DbManager.
            // We are only interested in relevant notifications triggered.
            // Also they are allowed only if sender is Admin.
            if (!commonModule()->resourceAccessManager()->hasGlobalPermission(
                sender->getUserAccessData(),
                Qn::GlobalAdminPermission))
            {
                NX_LOG(
                    QnLog::EC2_TRAN_LOG,
                    lit("Can't handle transaction %1 because of no administrator rights. Reopening connection...")
                    .arg(ApiCommand::toString(tran.command)),
                    cl_logWARNING
                );
                sender->setState(QnTransactionTransport::Error);
                return true;
            }

            if (m_handler)
                m_handler->triggerNotification(tran, NotificationSource::Remote);
            break;
        }
        case ApiCommand::getFullInfo:
            sender->setWriteSync(true);
            if (m_handler)
                m_handler->triggerNotification(tran, NotificationSource::Remote);
            break;
        default:
            return false; // Not a special case transaction.
    }

    proxyTransaction(tran, transportHeader);
    return true;
}

template <class T>
void QnTransactionMessageBus::gotTransaction(
    const QnTransaction<T> &tran, 
    QnTransactionTransport* sender, 
    const QnTransactionTransportHeader &transportHeader)
{
    QnMutexLocker lock(&m_mutex);
    if (processSpecialTransaction(tran, sender, transportHeader))
        return;

    if (m_handler)
        m_handler->triggerNotification(tran, NotificationSource::Remote);
    proxyTransaction(tran, transportHeader);
}

template <class T>
void QnTransactionMessageBus::proxyTransaction(const QnTransaction<T> &tran, const QnTransactionTransportHeader &_transportHeader)
{
    if (ApiPeerData::isClient(m_localPeerType))
        return;

    QnTransactionTransportHeader transportHeader(_transportHeader);
    transportHeader.distance++;
    if (transportHeader.flags & Qn::TT_ProxyToClient)
    {
        QnPeerSet clients = aliveClientPeers().keys().toSet();
        if (clients.isEmpty())
            return;
        transportHeader.dstPeers = clients;
        transportHeader.processedPeers += clients;
        transportHeader.processedPeers << commonModule()->moduleGUID();
        for (QnTransactionTransport* transport : m_connections)
        {
            if (transport->remotePeer().isClient() && transport->isReadyToSend(tran.command))
                transport->sendTransaction(tran, transportHeader);
        }
        return;
    }

    // proxy incoming transaction to other peers.
    if (!transportHeader.dstPeers.isEmpty() && (transportHeader.dstPeers - transportHeader.processedPeers).isEmpty())
    {
        return; // all dstPeers already processed
    }

    // do not put clients peers to processed list in case if client just reconnected to other server and previous server hasn't got update yet.
    QnPeerSet processedPeers = transportHeader.processedPeers + connectedServerPeers();
    processedPeers << commonModule()->moduleGUID();
    QnTransactionTransportHeader newHeader(transportHeader);
    newHeader.processedPeers = processedPeers;

    QSet<QnUuid> proxyList;
    for (QnConnectionMap::iterator itr = m_connections.begin(); itr != m_connections.end(); ++itr)
    {
        QnTransactionTransport* transport = *itr;
        if (transportHeader.processedPeers.contains(transport->remotePeer().id) || !transport->isReadyToSend(tran.command))
            continue;

        //NX_ASSERT(transport->remotePeer().id != tran.peerID);
        transport->sendTransaction(tran, newHeader);
        proxyList << transport->remotePeer().id;
    }

    if (nx::utils::log::isToBeLogged(nx::utils::log::Level::debug, QnLog::EC2_TRAN_LOG))
    {
        if (!proxyList.isEmpty())
        {
            QString proxyListStr;
            for (const QnUuid& peer : proxyList)
                proxyListStr += " " + peer.toString();
            NX_LOG(QnLog::EC2_TRAN_LOG, lit("proxy transaction %1 to (%2)").arg(tran.toString()).arg(proxyListStr), cl_logDEBUG1);
        }
    }
};

void QnTransactionMessageBus::onGotTransactionSyncRequest(
    QnTransactionTransport* sender,
    const QnTransaction<ApiSyncRequestData> &tran)
{
	NX_ASSERT("This function should not be called for non-server peers");
}

void QnTransactionMessageBus::queueSyncRequest(QnTransactionTransport* transport)
{
	NX_ASSERT("This function should not be called for non-server peers");
}

bool QnTransactionMessageBus::sendInitialData(QnTransactionTransport* transport)
{
    /** Send all data to the client peers on the connect. */
    QnPeerSet processedPeers = QnPeerSet() << transport->remotePeer().id << commonModule()->moduleGUID();
    if (ApiPeerData::isClient(m_localPeerType))
    {
        transport->setWriteSync(true);
        sendRuntimeInfo(transport, processedPeers, QnTranState());
        transport->setReadSync(true);
    }
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
    m_delayedAliveTran.remove(peer.id); // it's expected new tran about peer state if we connected / reconnected. drop previous (probably offline) tran
    addAlivePeerInfo(peer, peer.id, 0);
    handlePeerAliveChanged(peer, true, false);
}

void QnTransactionMessageBus::fillExtraAliveTransactionParams(ApiPeerAliveData* outAliveData)
{
}

void QnTransactionMessageBus::handlePeerAliveChanged(const ApiPeerData &peer, bool isAlive, bool sendTran)
{
    ApiPeerAliveData aliveData(peer, isAlive);

    if (sendTran)
    {
        QnTransaction<ApiPeerAliveData> tran(
            ApiCommand::peerAliveInfo,
            commonModule()->moduleGUID());
        tran.params = aliveData;
        NX_ASSERT(!tran.params.peer.instanceId.isNull());
		fillExtraAliveTransactionParams(&tran.params);
        if (peer.id == commonModule()->moduleGUID())
            sendTransaction(tran);
        else
        {
            int delay = nx::utils::random::number<int>(
                ALIVE_RESEND_TIMEOUT_MIN, ALIVE_RESEND_TIMEOUT_MAX);
            addDelayedAliveTran(std::move(tran), delay);
        }
        NX_LOG(QnLog::EC2_TRAN_LOG, lit("sending peerAlive info. id=%1 type=%2 isAlive=%3").arg(peer.id.toString()).arg(peer.peerType).arg(isAlive), cl_logDEBUG1);
    }

    if (peer.id == commonModule()->moduleGUID())
        return; //sending keep-alive

    if (isAlive)
    {
        NX_LOG(QnLog::EC2_TRAN_LOG, lit("emit peerFound. id=%1").arg(aliveData.peer.id.toString()), cl_logDEBUG1);
    }
    else
    {
        NX_LOG(QnLog::EC2_TRAN_LOG, lit("emit peerLost. id=%1").arg(aliveData.peer.id.toString()), cl_logDEBUG1);
    }

    if (isAlive)
        emit peerFound(aliveData.peer.id, aliveData.peer.peerType);
    else
        emit peerLost(aliveData.peer.id, aliveData.peer.peerType);
}

static nx::network::SocketAddress getUrlAddr(const nx::utils::Url& url) { return nx::network::SocketAddress(url.host(), url.port()); }

bool QnTransactionMessageBus::isPeerUsing(const nx::utils::Url& url)
{
    const nx::network::SocketAddress& addr1 = getUrlAddr(url);
    for (int i = 0; i < m_connectingConnections.size(); ++i)
    {
        const nx::network::SocketAddress& addr2 = m_connectingConnections[i]->remoteSocketAddr();
        if (addr2 == addr1)
            return true;
    }
    for (QnConnectionMap::iterator itr = m_connections.begin(); itr != m_connections.end(); ++itr)
    {
        QnTransactionTransport* transport = itr.value();
        if (getUrlAddr(transport->remoteAddr()) == addr1)
            return true;
    }
    return false;
}

void QnTransactionMessageBus::at_stateChanged(QnTransactionTransport::State)
{
    QnMutexLocker lock(&m_mutex);
    QnTransactionTransport* transport = (QnTransactionTransport*)sender();
    if (!transport)
        return;

    switch (transport->getState())
    {
        case QnTransactionTransport::Error:
            lock.unlock();
            transport->close();
            break;
        case QnTransactionTransport::Connected:
        {
            bool found = false;
            for (int i = 0; i < m_connectingConnections.size(); ++i)
            {
                if (m_connectingConnections[i] == transport)
                {
                    NX_ASSERT(!m_connections.contains(transport->remotePeer().id));
                    m_connections[transport->remotePeer().id] = m_connectingConnections[i];
                    emit newDirectConnectionEstablished(m_connectingConnections[i]);
                    m_connectingConnections.removeAt(i);
                    found = true;
                    break;
                }
            }
            NX_ASSERT(found);
            removeTTSequenceForPeer(transport->remotePeer().id);

            if (ApiPeerData::isServer(m_localPeerType) && transport->remoteIdentityTime() > commonModule()->systemIdentityTime())
            {
                // swith to new time
                NX_LOG(lit("Remote peer %1 has database restore time greater then current peer. Restarting and resync database with remote peer").arg(transport->remotePeer().id.toString()), cl_logINFO);
                for (QnTransactionTransport* t : m_connections)
                    t->setState(QnTransactionTransport::Error);
                for (QnTransactionTransport* t : m_connectingConnections)
                    t->setState(QnTransactionTransport::Error);
                commonModule()->setSystemIdentityTime(transport->remoteIdentityTime(), transport->remotePeer().id);
                m_restartPending = true;
                return;
            }

            transport->setState(QnTransactionTransport::ReadyForStreaming);

            transport->processExtraData();
            transport->startListening();

            m_runtimeTransactionLog->clearOldRuntimeData(ApiPersistentIdData(transport->remotePeer().id, transport->remotePeer().instanceId));
            if (sendInitialData(transport))
            {
                connectToPeerEstablished(transport->remotePeer());
            }
            else
            {
                lock.unlock();
                transport->close();
            }
            break;
        }
        case QnTransactionTransport::ReadyForStreaming:
            break;
        case QnTransactionTransport::Closed:
            for (int i = m_connectingConnections.size() - 1; i >= 0; --i)
            {
                QnTransactionTransport* transportPtr = m_connectingConnections[i];
                if (transportPtr == transport)
                {
                    m_connectingConnections.removeAt(i);
                    break;
                }
            }

            for (QnConnectionMap::iterator itr = m_connections.begin(); itr != m_connections.end(); ++itr)
            {
                QnTransactionTransport* transportPtr = itr.value();
                if (transportPtr == transport)
                {
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

void QnTransactionMessageBus::at_peerIdDiscovered(const nx::utils::Url& url, const QnUuid& id)
{
    QnMutexLocker lock(&m_mutex);
    auto itr = m_remoteUrls.find(url);
    if (itr != m_remoteUrls.end())
    {
        itr.value().discoveredTimeout.restart();
        itr.value().discoveredPeer = id;
    }
}

void QnTransactionMessageBus::doPeriodicTasks()
{
    QnMutexLocker lock(&m_mutex);
    for (QnConnectionMap::iterator
        itr = m_connections.begin();
        itr != m_connections.end();
        ++itr)
    {
        QnTransactionTransport* transport = itr.value();

        if (transport->remotePeerSupportsKeepAlive() &&
            transport->getState() >= QnTransactionTransport::Connected &&
            transport->getState() < QnTransactionTransport::Closed &&
            transport->isHttpKeepAliveTimeout())
        {
            NX_LOGX(
                QnLog::EC2_TRAN_LOG,
                lm("Transaction Transport HTTP keep-alive timeout for connection %1 to %2")
                .arg(transport->remotePeer().id).arg(transport->remoteAddr().toString()),
                cl_logWARNING);
            transport->setState(QnTransactionTransport::Error);
            continue;
        }

        if (!ApiPeerData::isClient(m_localPeerType))
        {
            if (transport->getState() == QnTransactionTransport::ReadyForStreaming &&
                !transport->remotePeer().isClient() &&
                transport->isNeedResync())
            {
                queueSyncRequest(transport);
            }
        }
    }

    // add new outgoing connections
    for (QMap<nx::utils::Url, RemoteUrlConnectInfo>::iterator itr = m_remoteUrls.begin(); itr != m_remoteUrls.end(); ++itr)
    {
        const nx::utils::Url& url = itr.key();
        RemoteUrlConnectInfo& connectInfo = itr.value();
        bool isTimeout = !connectInfo.lastConnectedTime.isValid() || connectInfo.lastConnectedTime.hasExpired(RECONNECT_TIMEOUT);

        if (isTimeout && !isPeerUsing(url) && !m_restartPending &&
            (ApiPeerData::isClient(m_localPeerType) ||
                !commonModule()->isReadOnly()))
        {
            if (!connectInfo.discoveredPeer.isNull())
            {
                if (connectInfo.discoveredTimeout.elapsed() >
                    std::chrono::milliseconds(
                        PEER_DISCOVERY_BY_ALIVE_UPDATE_INTERVAL_FACTOR *
                        commonModule()->globalSettings()->aliveUpdateInterval()).count())
                {
                    connectInfo.discoveredPeer = QnUuid();
                    connectInfo.discoveredTimeout.restart();
                }
                else if (m_connections.contains(connectInfo.discoveredPeer))
                {
                    continue;
                }
            }

            itr.value().lastConnectedTime.restart();
            QnTransactionTransport* transport = new QnTransactionTransport(
                this,
                &m_connectionGuardSharedState,
                localPeer());
            connect(transport, &QnTransactionTransport::gotTransaction, this, &QnTransactionMessageBus::at_gotTransaction, Qt::QueuedConnection);
            connect(transport, &QnTransactionTransport::stateChanged, this, &QnTransactionMessageBus::at_stateChanged, Qt::QueuedConnection);
            connect(transport, &QnTransactionTransport::remotePeerUnauthorized, this, &QnTransactionMessageBus::emitRemotePeerUnauthorized, Qt::QueuedConnection);
            connect(transport, &QnTransactionTransport::peerIdDiscovered, this, &QnTransactionMessageBus::at_peerIdDiscovered, Qt::QueuedConnection);
            transport->doOutgoingConnect(url);
            m_connectingConnections << transport;
        }
    }

    // send keep-alive if we connected to cloud
    if (!m_aliveSendTimer.isValid())
        m_aliveSendTimer.restart();
    const auto& settings = commonModule()->globalSettings();
    if (m_aliveSendTimer.elapsed() > std::chrono::milliseconds(settings->aliveUpdateInterval()).count())
    {
        m_aliveSendTimer.restart();
        handlePeerAliveChanged(localPeer(), true, true);
		logTransactionState();
    }

    QSet<QnUuid> lostPeers = checkAlivePeerRouteTimeout(); // check if some routs to a server not accessible any more
    removePeersWithTimeout(lostPeers); // removeLostPeers
    sendDelayedAliveTran();
}

void QnTransactionMessageBus::logTransactionState()
{
}

QSet<QnUuid> QnTransactionMessageBus::checkAlivePeerRouteTimeout()
{
    const auto& settings = commonModule()->globalSettings();
    QSet<QnUuid> lostPeers;
    for (AlivePeersMap::iterator itr = m_alivePeers.begin(); itr != m_alivePeers.end(); ++itr)
    {
        AlivePeerInfo& peerInfo = itr.value();
        for (auto itr = peerInfo.routingInfo.begin(); itr != peerInfo.routingInfo.end();)
        {
            const RoutingRecord& routingRecord = itr.value();
            if ((routingRecord.distance > 0) &&
                (m_currentTimeTimer.elapsed() - routingRecord.lastRecvTime >
                    std::chrono::milliseconds(settings->aliveUpdateInterval()*ALIVE_UPDATE_PROBE_COUNT + ALIVE_UPDATE_INTERVAL_OVERHEAD).count()))
            {
                itr = peerInfo.routingInfo.erase(itr);
            }
            else
            {
                ++itr;
            }
        }
        if (peerInfo.routingInfo.isEmpty())
            lostPeers << peerInfo.peer.id;
    }
    return lostPeers;
}

void QnTransactionMessageBus::removePeersWithTimeout(const QSet<QnUuid>& lostPeers)
{
    for (AlivePeersMap::iterator itr = m_alivePeers.begin(); itr != m_alivePeers.end(); ++itr)
    {
        if (lostPeers.contains(itr.key()))
        {
            for (QnTransactionTransport* transport : m_connectingConnections)
            {
                if (transport->getState() == QnTransactionTransport::Closed)
                    continue; // it's going to close soon
                if (transport->remotePeer().id == itr.key())
                {
                    qWarning() << "No alive info during timeout. reconnect to peer" << transport->remotePeer().id;
                    transport->setState(QnTransactionTransport::Error);
                }
            }

            for (QnTransactionTransport* transport : m_connections.values())
            {
                if (transport->getState() == QnTransactionTransport::Closed)
                    continue; // it's going to close soon
                if (transport->remotePeer().id == itr.key() &&
                    ec2::ApiPeerData::isServer(transport->remotePeer().peerType))
                {
                    qWarning() << "No alive info during timeout. reconnect to peer" << transport->remotePeer().id;
                    transport->setState(QnTransactionTransport::Error);
                }
            }
        }
    }
    for (const QnUuid& id : lostPeers)
        connectToPeerLost(id);
}

void QnTransactionMessageBus::sendRuntimeInfo(QnTransactionTransport* transport, const QnTransactionTransportHeader& transportHeader, const QnTranState& runtimeState)
{
    QList<QnTransaction<ApiRuntimeData>> result;
    m_runtimeTransactionLog->getTransactionsAfter(runtimeState, result);
    for (const QnTransaction<ApiRuntimeData> &tran : result)
    {
        QnTransactionTransportHeader ttHeader = transportHeader;
        ttHeader.distance = distanceToPeer(tran.params.peer.id);
        transport->sendTransaction(tran, ttHeader);
    }
}

bool QnTransactionMessageBus::moveConnectionToReadyForStreaming(const QnUuid& connectionGuid)
{
    QnMutexLocker lock(&m_mutex);

    for (auto connection : m_connectingConnections)
    {
        if (connection->connectionGuid() == connectionGuid)
        {
            connection->monitorConnectionForClosure();
            connection->setState(QnTransactionTransport::Connected);
            return true;
        }
    }

    return false;
}

nx::utils::Url QnTransactionMessageBus::updateOutgoingUrl(const nx::utils::Url& srcUrl) const
{
    nx::utils::Url url(srcUrl);
    url.setPath("/ec2/events");
    QUrlQuery q(url.query());

    q.addQueryItem("guid", commonModule()->moduleGUID().toString());
    q.addQueryItem("runtime-guid", commonModule()->runningInstanceGUID().toString());
    q.addQueryItem("system-identity-time", QString::number(commonModule()->systemIdentityTime()));
    url.setQuery(q);
    return url;
}

void QnTransactionMessageBus::addOutgoingConnectionToPeer(const QnUuid& id, const nx::utils::Url &_url)
{
    removeOutgoingConnectionFromPeer(id);
    nx::utils::Url url = updateOutgoingUrl(_url);
    QnMutexLocker lock(&m_mutex);
    if (!m_remoteUrls.contains(url))
    {
        m_remoteUrls.insert(url, RemoteUrlConnectInfo(id));
        QTimer::singleShot(0, this, SLOT(doPeriodicTasks()));
    }
}

void QnTransactionMessageBus::removeOutgoingConnectionFromPeer(const QnUuid& id)
{
    QnMutexLocker lock(&m_mutex);

    for (auto itr = m_remoteUrls.begin(); itr != m_remoteUrls.end(); ++itr)
    {
        RemoteUrlConnectInfo& info = itr.value();
        if (info.id == id)
        {
            nx::utils::Url url = itr.key();
            const nx::network::SocketAddress& urlStr = getUrlAddr(url);
            for (QnTransactionTransport* transport : m_connections.values())
            {
                if (transport->remoteSocketAddr() == urlStr)
                {
                    NX_LOGX(lm("Disconnected from peer %1").arg(url), cl_logWARNING);
                    transport->setState(QnTransactionTransport::Error);
                }
            }
            m_remoteUrls.erase(itr);
            break;
        }
    }
}

QVector<QnTransportConnectionInfo> QnTransactionMessageBus::connectionsInfo() const
{
    QVector<QnTransportConnectionInfo> connections;

    auto storeTransport = [&connections](const QnTransactionTransport *transport)
    {
        QnTransportConnectionInfo info;
        info.url = transport->remoteAddr();
        info.state = QnTransactionTransport::toString(transport->getState());
        info.isIncoming = transport->isIncoming();
        info.remotePeerId = transport->remotePeer().id;
        connections.append(info);
    };

    QnMutexLocker lock(&m_mutex);

    for (const QnTransactionTransport *transport : m_connections.values())
        storeTransport(transport);
    for (const QnTransactionTransport *transport : m_connectingConnections)
        storeTransport(transport);

    return connections;
}

void QnTransactionMessageBus::waitForNewTransactionsReady(const QnUuid& connectionGuid)
{
    QnMutexLocker lock(&m_mutex);

    auto waitForNewTransactionsReadyFunc =
        [&lock](QnTransactionTransport* transport)
    {
        QnTransactionTransport::Locker transactionTransportLocker(transport);
        //QnTransactionTransport destructor will block until transactionTransportLocker is alive
        lock.unlock();
        transactionTransportLocker.waitForNewTransactionsReady();
        lock.relock();
    };

    for (QnTransactionTransport* transport : m_connections)
    {
        if (transport->connectionGuid() != connectionGuid)
            continue;

        waitForNewTransactionsReadyFunc(transport);
        return;
    }

    for (QnTransactionTransport* transport : m_connectingConnections)
    {
        if (transport->connectionGuid() != connectionGuid)
            continue;

        waitForNewTransactionsReadyFunc(transport);
        return;
    }
}

void QnTransactionMessageBus::connectionFailure(const QnUuid& connectionGuid)
{
    QnMutexLocker lock(&m_mutex);
    for (QnTransactionTransport* transport : m_connections)
    {
        if (transport->connectionGuid() != connectionGuid)
            continue;
        //mutex is unlocked if we go to wait
        transport->connectionFailure();
        return;
    }
}

void QnTransactionMessageBus::dropConnections()
{
    QnMutexLocker lock(&m_mutex);
    m_remoteUrls.clear();
    reconnectAllPeers(&lock);
}
void QnTransactionMessageBus::reconnectAllPeers()
{
    QnMutexLocker lock(&m_mutex);
    reconnectAllPeers(&lock);
}

void QnTransactionMessageBus::reconnectAllPeers(QnMutexLockerBase* const /*lock*/)
{
    for (QnTransactionTransport* transport : m_connections)
    {
        NX_LOGX(lm("Disconnected from peer %1").arg(transport->remoteAddr()), cl_logWARNING);
        transport->setState(QnTransactionTransport::Error);
    }
    for (auto transport: m_connectingConnections)
        transport->setState(ec2::QnTransactionTransport::Error);
}

QnTransactionMessageBus::AlivePeersMap QnTransactionMessageBus::alivePeers() const
{
    QnMutexLocker lock(&m_mutex);
    return m_alivePeers;
}

QnPeerSet QnTransactionMessageBus::connectedServerPeers() const
{
    QnPeerSet result;
    for (QnConnectionMap::const_iterator itr = m_connections.begin(); itr != m_connections.end(); ++itr)
    {
        QnTransactionTransport* transport = *itr;
        if (!transport->remotePeer().isClient() && transport->getState() == QnTransactionTransport::ReadyForStreaming)
            result << transport->remotePeer().id;
    }

    return result;
}

QMap<QnUuid, ApiPeerData> QnTransactionMessageBus::aliveServerPeers() const
{
    QnMutexLocker lock(&m_mutex);
    QMap<QnUuid, ApiPeerData> result;
    for (AlivePeersMap::const_iterator itr = m_alivePeers.begin(); itr != m_alivePeers.end(); ++itr)
    {
        if (itr->peer.isServer())
            result.insert(itr.key(), itr->peer);
    }

    return result;
}

QnTransactionMessageBus::AlivePeersMap QnTransactionMessageBus::aliveClientPeers() const
{
    QnMutexLocker lock(&m_mutex);
    AlivePeersMap result;
    for (AlivePeersMap::const_iterator itr = m_alivePeers.begin(); itr != m_alivePeers.end(); ++itr)
    {
        if (itr->peer.isClient())
            result.insert(itr.key(), itr.value());
    }

    return result;
}

QSet<QnUuid> QnTransactionMessageBus::directlyConnectedClientPeers() const
{
    QSet<QnUuid> result;

    auto clients = aliveClientPeers();
    for (auto it = clients.begin(); it != clients.end(); ++it)
    {
        const auto& clientId = it.key();
        if (it->routingInfo.contains(clientId))
            result.insert(clientId);
    }

    return result;
}

QSet<QnUuid> QnTransactionMessageBus::directlyConnectedServerPeers() const
{
    QnMutexLocker lock(&m_mutex);
    return connectedServerPeers();
}

ec2::ApiPeerData QnTransactionMessageBus::localPeer() const
{
    return ec2::ApiPeerData(commonModule()->moduleGUID(), commonModule()->runningInstanceGUID(), m_localPeerType);
}

void QnTransactionMessageBus::at_runtimeDataUpdated(const QnTransaction<ApiRuntimeData>& tran)
{
    // data was changed by local transaction log (old data instance for same peer was removed), emit notification to apply new data version outside
    if (m_handler)
        m_handler->triggerNotification(tran, NotificationSource::Local);
}

void QnTransactionMessageBus::emitRemotePeerUnauthorized(const QnUuid& id)
{
    QnMutexLocker lock(&m_mutex);
    if (!m_alivePeers.contains(id))
        emit remotePeerUnauthorized(id);
}

void QnTransactionMessageBus::onEc2ConnectionSettingsChanged(const QString& key)
{
    //we need break connection only if following settings have been changed:
    //  connectionKeepAliveTimeout
    //  keepAliveProbeCount
    using namespace nx::settings_names;
    const auto& settings = commonModule()->globalSettings();

    if (key == kConnectionKeepAliveTimeoutKey)
    {
        const auto timeout = settings->connectionKeepAliveTimeout();
        QnMutexLocker lock(&m_mutex);
        //resetting connection
        for (auto* transport : m_connections)
        {
            if (transport->connectionKeepAliveTimeout() != timeout)
                transport->setState(ec2::QnTransactionTransport::Error);
        }
    }
    else if (key == kConnectionKeepAliveTimeoutKey)
    {
        const auto probeCount = settings->keepAliveProbeCount();
        QnMutexLocker lock(&m_mutex);
        //resetting connection
        for (auto* transport : m_connections)
        {
            if (transport->keepAliveProbeCount() != probeCount)
                transport->setState(ec2::QnTransactionTransport::Error);
        }
    }
}

QnUuid QnTransactionMessageBus::routeToPeerVia(const QnUuid& dstPeer, int* peerDistance) const
{
    QnMutexLocker lock(&m_mutex);
    *peerDistance = INT_MAX;
    const auto itr = m_alivePeers.find(dstPeer);
    if (itr == m_alivePeers.cend())
        return QnUuid(); // route info not found
    const AlivePeerInfo& peerInfo = itr.value();
    int minDistance = INT_MAX;
    QnUuid result;
    for (auto itr2 = peerInfo.routingInfo.cbegin(); itr2 != peerInfo.routingInfo.cend(); ++itr2)
    {
        int distance = itr2.value().distance;
        if (distance < minDistance)
        {
            minDistance = distance;
            result = itr2.key();
        }
    }
    *peerDistance = minDistance;
    return result;
}

int QnTransactionMessageBus::distanceToPeer(const QnUuid& dstPeer) const
{
    if (dstPeer == commonModule()->moduleGUID())
        return 0;

    int minDistance = INT_MAX;
    for (const RoutingRecord& rec : m_alivePeers.value(dstPeer).routingInfo)
        minDistance = std::min(minDistance, (int) rec.distance);
    return minDistance;
}

} //namespace ec2
