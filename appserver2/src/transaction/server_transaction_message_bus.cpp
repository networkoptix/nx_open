#include "server_transaction_message_bus.h"

#include <database/db_manager.h>
#include "transaction_message_bus_priv.h"

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
    void operator()(QnTransactionMessageBus *bus, const QnTransaction<T> &transaction, QnTransactionTransport *sender, const QnTransactionTransportHeader &transportHeader) const
    {
        bus->sendTransactionToTransport(transaction, sender, transportHeader);
    }
};

struct SendTransactionToTransportFastFuction
{
    bool operator()(QnTransactionMessageBus* /*bus*/, Qn::SerializationFormat srcFormat, const QByteArray& serializedTran, QnTransactionTransport *sender,
        const QnTransactionTransportHeader &transportHeader) const
    {
        return sender->sendSerializedTransaction(srcFormat, serializedTran, transportHeader);
    }
};

// --------------------------------- ServerTransactionMessageBus ------------------------------

void ServerTransactionMessageBus::printTranState(const QnTranState& tranState)
{
	if (!nx::utils::log::isToBeLogged(nx::utils::log::Level::debug, QnLog::EC2_TRAN_LOG))
		return;

	for (auto itr = tranState.values.constBegin(); itr != tranState.values.constEnd(); ++itr)
	{
		NX_LOG(QnLog::EC2_TRAN_LOG, lit("key=%1 (dbID=%2) need after=%3").arg(itr.key().id.toString()).arg(itr.key().persistentId.toString()).arg(itr.value()), cl_logDEBUG1);
	}
}

bool ServerTransactionMessageBus::gotAliveData(
	const ApiPeerAliveData &aliveData, 
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
				NX_LOG(QnLog::EC2_TRAN_LOG, lit("DETECT transaction GAP via update message. Resync with peer %1").
					arg(transport->remotePeer().id.toString()), cl_logDEBUG1);
				NX_LOG(QnLog::EC2_TRAN_LOG, lit("peer state:"), cl_logDEBUG1);
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

	ApiPersistentIdData persistentKey(tran.peerID, tran.persistentInfo.dbID);
	int persistentSeq = m_db->transactionLog()->getLatestSequence(persistentKey);

	if (nx::utils::log::isToBeLogged(nx::utils::log::Level::warning, QnLog::EC2_TRAN_LOG))
	{
		if (!transport->isSyncDone() && transport->isReadSync(ApiCommand::NotDefined) && transportHeader.sender != transport->remotePeer().id)
		{
			NX_LOG(QnLog::EC2_TRAN_LOG, lit("Got transaction from peer %1 while sync with peer %2 in progress").
				arg(transportHeader.sender.toString()).arg(transport->remotePeer().id.toString()), cl_logWARNING);
		}
	}

	if (tran.persistentInfo.sequence > persistentSeq + 1)
	{
		if (transport->isSyncDone())
		{
			// gap in persistent data detect, do resync
			NX_LOG(QnLog::EC2_TRAN_LOG, lit("GAP in persistent data detected! for peer %1 Expected seq=%2, but got seq=%3").
				arg(tran.peerID.toString()).arg(persistentSeq + 1).arg(tran.persistentInfo.sequence), cl_logDEBUG1);

			if (!transport->remotePeer().isClient() && !ApiPeerData::isClient(m_localPeerType))
				queueSyncRequest(transport);
			else
				transport->setState(QnTransactionTransport::Error); // reopen
			return false;
		}
		else
		{
			NX_LOG(QnLog::EC2_TRAN_LOG, lit("GAP in persistent data, but sync in progress %1. Expected seq=%2, but got seq=%3").
				arg(tran.peerID.toString()).arg(persistentSeq + 1).arg(tran.persistentInfo.sequence), cl_logDEBUG1);
		}
	}
	return true;
}

void ServerTransactionMessageBus::onGotTransactionSyncRequest(
	QnTransactionTransport* sender,
	const QnTransaction<ApiSyncRequestData> &tran)
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
		sender->remotePeer().peerType == Qn::PeerType::PT_CloudServer,
		serializedTransactions);

	if (errorCode == ErrorCode::ok)
	{
		NX_LOG(QnLog::EC2_TRAN_LOG, lit("got sync request from peer %1. Need transactions after:").arg(sender->remotePeer().id.toString()), cl_logDEBUG1);
		printTranState(tran.params.persistentState);
		NX_LOG(QnLog::EC2_TRAN_LOG, lit("exist %1 new transactions").arg(serializedTransactions.size()), cl_logDEBUG1);

		NX_ASSERT(m_connections.contains(sender->remotePeer().id));
		NX_ASSERT(sender->getState() >= QnTransactionTransport::ReadyForStreaming);
		QnTransaction<QnTranStateResponse> tranSyncResponse(
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

		QnTransaction<ApiTranSyncDoneData> tranSyncDone(
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

	QnTransaction<ApiSyncRequestData> requestTran(
		ApiCommand::tranSyncRequest,
		commonModule()->moduleGUID());
	requestTran.params.persistentState = m_db->transactionLog()->getTransactionsState();
	requestTran.params.runtimeState = m_runtimeTransactionLog->getTransactionsState();

	NX_LOG(QnLog::EC2_TRAN_LOG, lit("send syncRequest to peer %1").arg(transport->remotePeer().id.toString()), cl_logDEBUG1);
	printTranState(requestTran.params.persistentState);
	transport->sendTransaction(requestTran, QnPeerSet() << transport->remotePeer().id << commonModule()->moduleGUID());
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

bool ServerTransactionMessageBus::readApiFullInfoData(
	const Qn::UserAccessData& userAccess,
	const ec2::ApiPeerData& remotePeer,
	ApiFullInfoData* outData)
{
	ErrorCode errorCode;
	if (remotePeer.peerType == Qn::PT_MobileClient)
		errorCode = dbManager(m_db, userAccess).readApiFullInfoDataForMobileClient(outData, userAccess.userId);
	else
		errorCode = dbManager(m_db, userAccess).readApiFullInfoDataComplete(outData);

	if (errorCode != ErrorCode::ok)
	{
		qWarning() << "Cannot execute query for ApiFullInfoData:" << toString(errorCode);
		return false;
	}
	return true;
}

bool ServerTransactionMessageBus::sendInitialData(QnTransactionTransport* transport)
{
	/** Send all data to the client peers on the connect. */
	QnPeerSet processedPeers = QnPeerSet() << transport->remotePeer().id << commonModule()->moduleGUID();
	if (!base_type::sendInitialData(transport))
		return false;
	
	if (transport->remotePeer().peerType == Qn::PT_DesktopClient ||
		transport->remotePeer().peerType == Qn::PT_VideowallClient ||
		transport->remotePeer().peerType == Qn::PT_MobileClient)
	{
		/** Request all data to be sent to the Desktop Client peers on the connect. */
		QnTransaction<ApiFullInfoData> tran;
		tran.command = ApiCommand::getFullInfo;
		tran.peerID = commonModule()->moduleGUID();
		if (!readApiFullInfoData(transport->getUserAccessData(), transport->remotePeer(), &tran.params))
			return false;

		transport->setWriteSync(true);
		sendRuntimeInfo(transport, processedPeers, QnTranState());
		transport->sendTransaction(tran, processedPeers);
		transport->setReadSync(true);
	}
	else if (transport->remotePeer().peerType == Qn::PT_OldMobileClient)
	{
		/** Request all data to be sent to the client peers on the connect. */
		QnTransaction<ApiMediaServerDataExList> tranServers;
		tranServers.command = ApiCommand::getMediaServersEx;
		tranServers.peerID = commonModule()->moduleGUID();
		if (dbManager(m_db, transport->getUserAccessData()).doQuery(QnUuid(), tranServers.params) != ErrorCode::ok)
		{
			qWarning() << "Can't execute query for sync with client peer!";
			return false;
		}

		ec2::ApiCameraDataExList cameras;
		if (dbManager(m_db, transport->getUserAccessData()).doQuery(QnUuid(), cameras) != ErrorCode::ok)
		{
			qWarning() << "Can't execute query for sync with client peer!";
			return false;
		}
		QnTransaction<ApiCameraDataExList> tranCameras;
		tranCameras.command = ApiCommand::getCamerasEx;
		tranCameras.peerID = commonModule()->moduleGUID();

		// Filter out desktop cameras.
		// Usually, there are only a few desktop cameras relatively to total cameras count.
		tranCameras.params.reserve(cameras.size());
		std::copy_if(cameras.cbegin(), cameras.cend(), std::back_inserter(tranCameras.params),
			[](const ec2::ApiCameraData& camera)
		{
			return camera.typeId != QnResourceTypePool::kDesktopCameraTypeUuid;
		});

		QnTransaction<ApiUserDataList> tranUsers;
		tranUsers.command = ApiCommand::getUsers;
		tranUsers.peerID = commonModule()->moduleGUID();
		if (dbManager(m_db, transport->getUserAccessData()).doQuery(QnUuid(), tranUsers.params) != ErrorCode::ok)
		{
			qWarning() << "Can't execute query for sync with client peer!";
			return false;
		}

		QnTransaction<ApiLayoutDataList> tranLayouts;
		tranLayouts.command = ApiCommand::getLayouts;
		tranLayouts.peerID = commonModule()->moduleGUID();
		if (dbManager(m_db, transport->getUserAccessData()).doQuery(QnUuid(), tranLayouts.params) != ErrorCode::ok)
		{
			qWarning() << "Can't execute query for sync with client peer!";
			return false;
		}

		QnTransaction<ApiServerFootageDataList> tranCameraHistory;
		tranCameraHistory.command = ApiCommand::getCameraHistoryItems;
		tranCameraHistory.peerID = commonModule()->moduleGUID();
		if (dbManager(m_db, transport->getUserAccessData()).doQuery(nullptr, tranCameraHistory.params) != ErrorCode::ok)
		{
			qWarning() << "Can't execute query for sync with client peer!";
			return false;
		}

		transport->setWriteSync(true);
		sendRuntimeInfo(transport, processedPeers, QnTranState());
		transport->sendTransaction(tranServers, processedPeers);
		transport->sendTransaction(tranCameras, processedPeers);
		transport->sendTransaction(tranUsers, processedPeers);
		transport->sendTransaction(tranLayouts, processedPeers);
		transport->sendTransaction(tranCameraHistory, processedPeers);
		transport->setReadSync(true);
	}

	if (!transport->remotePeer().isClient() && !ApiPeerData::isClient(m_localPeerType))
		queueSyncRequest(transport);

	return true;
}

void ServerTransactionMessageBus::fillExtraAliveTransactionParams(ApiPeerAliveData* outAliveData)
{
	if (outAliveData->isAlive && outAliveData->peer.id == commonModule()->moduleGUID())
	{
		outAliveData->persistentState = m_db->transactionLog()->getTransactionsState();
		outAliveData->runtimeState = m_runtimeTransactionLog->getTransactionsState();
	}
}

void ServerTransactionMessageBus::logTransactionState()
{
	NX_LOG(QnLog::EC2_TRAN_LOG, "Current transaction state:", cl_logDEBUG1);
	printTranState(m_db->transactionLog()->getTransactionsState());
}

void ServerTransactionMessageBus::gotConnectionFromRemotePeer(
	const QnUuid& connectionGuid,
	ConnectionLockGuard connectionLockGuard,
	QSharedPointer<nx::network::AbstractStreamSocket> socket,
	ConnectionType::Type connectionType,
	const ApiPeerData& remotePeer,
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
	const QnUuid& connectionGuid,
	QSharedPointer<nx::network::AbstractStreamSocket> socket,
	const ApiPeerData &/*remotePeer*/,
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
	const QnUuid& connectionGuid,
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

template <class T>
ErrorCode ServerTransactionMessageBus::writePersistentTransaction(const QnTransaction<T> &tran)
{
	QByteArray serializedTran = m_ubjsonTranSerializer->serializedTransaction(tran);
	return dbManager(m_db, sender->getUserAccessData()).executeTransaction(tran, serializedTran);
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
