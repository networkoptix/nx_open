#pragma once

#include "p2p_message_bus.h"

namespace ec2 {
namespace detail {
class QnDbManager;
}
}

namespace nx {
namespace p2p {


class ServerMessageBus: public MessageBus
{
    using base_type = MessageBus;
public:
	ServerMessageBus(
        Qn::PeerType peerType,
        QnCommonModule* commonModule,
        ec2::QnJsonTransactionSerializer* jsonTranSerializer,
        QnUbjsonTransactionSerializer* ubjsonTranSerializer);
	virtual ~ServerMessageBus();

    void setDatabase(ec2::detail::QnDbManager* db);

	void gotConnectionFromRemotePeer(
		const ec2::ApiPeerDataEx& remotePeer,
		ec2::ConnectionLockGuard connectionLockGuard,
		nx::network::WebSocketPtr webSocket,
		const Qn::UserAccessData& userAccessData,
		std::function<void()> onConnectionClosedCallback);
protected:
	virtual void doPeriodicTasks() override;
	virtual void stop() override;
	virtual void addOfflinePeersFromDb() override;
	virtual void sendInitialDataToCloud(const P2pConnectionPtr& connection) override;
	virtual bool handlePushTransactionData(
		const P2pConnectionPtr& connection,
		const QByteArray& serializedTran,
		const TransportHeader& header) override;
	virtual bool handlePushImpersistentBroadcastTransaction(
		const P2pConnectionPtr& connection,
		const QByteArray& payload);

    /**
     * Select next portion of data from transaction log and send it.
     * @param addImplicitData - if parameter is false then select only data from newSubscription list.
     * If parameter is true then add all data from sequence 0 what are not mentioned in the list.
     */
    virtual bool selectAndSendTransactions(
        const P2pConnectionPtr& connection,
        QnTranState newSubscription,
        bool addImplicitData) override;

	template <class T>
	bool writeTransactionToPersistantStorage(const QnTransaction<T>& tran);

	void gotTransaction(
		const QnTransaction<ApiUpdateSequenceData> &tran,
		const P2pConnectionPtr& connection,
		const TransportHeader& transportHeader);

	template <class T>
	void gotTransaction(const QnTransaction<T>& tran, const P2pConnectionPtr& connection, const TransportHeader& transportHeader);
private:
    friend struct GotTransactionFuction;

	void sendAlivePeersMessage(const P2pConnectionPtr& connection = P2pConnectionPtr());
	void startStopConnections(const QMap<ApiPersistentIdData, P2pConnectionPtr>& currentSubscription);
	P2pConnectionPtr findBestConnectionToSubscribe(
		const QVector<ApiPersistentIdData>& viaList,
		QMap<P2pConnectionPtr, int> newSubscriptions) const;
	void doSubscribe(const QMap<ApiPersistentIdData, P2pConnectionPtr>& currentSubscription);
	void commitLazyData();
	void sendInitialDataToClient(const P2pConnectionPtr& connection);
	void resubscribePeers(QMap<ApiPersistentIdData, P2pConnectionPtr> newSubscription);
	void proxyFillerTransaction(
		const ec2::QnAbstractTransaction& tran,
		const TransportHeader& transportHeader);
	void resotreAfterDbError();
	bool needSubscribeDelay();

	bool pushTransactionList(
		const P2pConnectionPtr& connection,
		const QList<QByteArray>& tranList);

    void printSubscribeMessage(
        const QnUuid& remoteId,
        const QVector<ApiPersistentIdData>& subscribedTo,
        const QVector<qint32>& sequences) const;

	bool readApiFullInfoData(
		const Qn::UserAccessData& userAccess,
		const ec2::ApiPeerData& remotePeer,
		ApiFullInfoData* outData);
private:
	ec2::detail::QnDbManager* m_db;
	QElapsedTimer m_dbCommitTimer;
	QElapsedTimer m_wantToSubscribeTimer;
};

} // namespace p2p
} // namespace nx
