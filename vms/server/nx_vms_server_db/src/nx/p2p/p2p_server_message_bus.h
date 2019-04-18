#pragma once

#include <nx/p2p/p2p_message_bus.h>

namespace ec2 { namespace detail { class QnDbManager; }}

namespace nx {
namespace p2p {

class ServerMessageBus: public MessageBus
{
    using base_type = MessageBus;
    Q_OBJECT;
public:
    ServerMessageBus(
        vms::api::PeerType peerType,
        QnCommonModule* commonModule,
        ec2::QnJsonTransactionSerializer* jsonTranSerializer,
        QnUbjsonTransactionSerializer* ubjsonTranSerializer);

    virtual ~ServerMessageBus();

    void setDatabase(ec2::detail::QnDbManager* db);

    void gotConnectionFromRemotePeer(
        const vms::api::PeerDataEx& remotePeer,
        ec2::ConnectionLockGuard connectionLockGuard,
        P2pTransportPtr p2pTransport,
        const QUrlQuery& requestUrlQuery,
        const Qn::UserAccessData& userAccessData,
        std::function<void()> onConnectionClosedCallback);

    bool gotPostConnection(
        const vms::api::PeerDataEx& remotePeer,
        std::unique_ptr<nx::network::AbstractStreamSocket> socket,
        nx::Buffer requestBody);

    virtual bool validateRemotePeerData(const vms::api::PeerDataEx& remotePeer) override;
signals:
    void lazyDataCommtDone();
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
        vms::api::TranState newSubscription,
        bool addImplicitData) override;

    void gotTransaction(
        const QnTransaction<nx::vms::api::UpdateSequenceData>& tran,
        const P2pConnectionPtr& connection,
        const TransportHeader& transportHeader);

    template <class T>
    void gotTransaction(
        const QnTransaction<T>& tran,
        const P2pConnectionPtr& connection,
        const TransportHeader& transportHeader);

private:
    friend struct GotTransactionFuction;

    void sendAlivePeersMessage(const P2pConnectionPtr& connection = P2pConnectionPtr());
    void startStopConnections(
        const QMap<vms::api::PersistentIdData, P2pConnectionPtr>& currentSubscription);

    P2pConnectionPtr findBestConnectionToSubscribe(
        const QList<vms::api::PersistentIdData>& viaList,
        QMap<P2pConnectionPtr, int> newSubscriptions) const;

    void doSubscribe(const QMap<vms::api::PersistentIdData, P2pConnectionPtr>& currentSubscription);
    void commitLazyData();
    void sendInitialDataToClient(const P2pConnectionPtr& connection);
    void resubscribePeers(QMap<vms::api::PersistentIdData, P2pConnectionPtr> newSubscription);

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
        const QVector<vms::api::PersistentIdData>& subscribedTo,
        const QVector<qint32>& sequences) const;

    bool readFullInfoData(
        const Qn::UserAccessData& userAccess,
        const vms::api::PeerData& remotePeer,
        vms::api::FullInfoData* outData);

private:
    ec2::detail::QnDbManager* m_db;
    QElapsedTimer m_dbCommitTimer;
    QElapsedTimer m_wantToSubscribeTimer;
    std::atomic_flag m_restartPending = ATOMIC_FLAG_INIT;
};

} // namespace p2p
} // namespace nx
