#pragma once

#include <transaction/transaction_message_bus.h>

namespace ec2 {

class ServerTransactionMessageBus: public QnTransactionMessageBus
{
    using base_type = QnTransactionMessageBus;
public:
	ServerTransactionMessageBus(
        nx::vms::api::PeerType peerType,
        QnCommonModule* commonModule,
        QnJsonTransactionSerializer* jsonTranSerializer,
        QnUbjsonTransactionSerializer* ubjsonTranSerializer);

    virtual ~ServerTransactionMessageBus();

    void setDatabase(detail::QnDbManager* db);

	void gotConnectionFromRemotePeer(const QnUuid& connectionGuid,
		ConnectionLockGuard connectionLockGuard,
		QSharedPointer<nx::network::AbstractStreamSocket> socket,
		ConnectionType::Type connectionType,
		const nx::vms::api::PeerData& remotePeer,
		qint64 remoteSystemIdentityTime,
		const nx::network::http::Request& request,
		const QByteArray& contentEncoding,
		std::function<void()> ttFinishCallback,
		const Qn::UserAccessData &userAccessData);

	//!Report socket to receive transactions from
	/*!
	\param requestBuf Contains serialized \a request and (possibly) partial (or full) message body
	*/
	void gotIncomingTransactionsConnectionFromRemotePeer(
		const QnUuid& connectionGuid,
		QSharedPointer<nx::network::AbstractStreamSocket> socket,
		const nx::vms::api::PeerData &remotePeer,
		qint64 remoteSystemIdentityTime,
		const nx::network::http::Request& request,
		const QByteArray& requestBuf);

	bool gotTransactionFromRemotePeer(
		const QnUuid& connectionGuid,
		const nx::network::http::Request& request,
		const QByteArray& requestMsgBody);
protected:
	virtual bool gotAliveData(
		const ApiPeerAliveData &aliveData,
		QnTransactionTransport* transport,
		const QnTransactionTransportHeader* ttHeader) override;

	virtual bool checkSequence(
		const QnTransactionTransportHeader& transportHeader,
		const QnAbstractTransaction& tran,
		QnTransactionTransport* transport) override;

	virtual void onGotTransactionSyncRequest(
		QnTransactionTransport* sender,
		const QnTransaction<ApiSyncRequestData> &tran) override;

	virtual void queueSyncRequest(QnTransactionTransport* transport) override;
	virtual bool sendInitialData(QnTransactionTransport* transport) override;
	virtual void fillExtraAliveTransactionParams(ApiPeerAliveData* outAliveData) override;
	virtual void logTransactionState() override;
    virtual ErrorCode updatePersistentMarker(
        const QnTransaction<nx::vms::api::UpdateSequenceData>& tran) override;

	virtual void handleIncomingTransaction(
		QnTransactionTransport* sender,
		Qn::SerializationFormat tranFormat,
		QByteArray serializedTran,
		const QnTransactionTransportHeader &transportHeader) override;
private:
	friend struct SendTransactionToTransportFuction;
    friend struct GotTransactionFuction;

	template <class T>
	void sendTransactionToTransport(const QnTransaction<T> &tran, QnTransactionTransport* transport, const QnTransactionTransportHeader &transportHeader);

	bool isSyncInProgress() const;
	bool readApiFullInfoData(
		const Qn::UserAccessData& userAccess,
		const nx::vms::api::PeerData& remotePeer,
		ApiFullInfoData* outData);
	void printTranState(const QnTranState& tranState);

    void proxyFillerTransaction(const QnAbstractTransaction& tran, const QnTransactionTransportHeader& transportHeader);

    template <class T>
    void gotTransaction(const QnTransaction<T> &tran, QnTransactionTransport* sender, const QnTransactionTransportHeader &transportHeader);

private:
	detail::QnDbManager* m_db = nullptr;
};

} //namespace ec2
