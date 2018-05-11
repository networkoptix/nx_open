#pragma once

#include <memory>

#include <QtCore/QElapsedTimer>

#include <common/common_module.h>

#include <nx_ec/ec_api.h>
#include <nx/vms/api/data/lock_data.h>
#include <nx_ec/data/api_peer_data.h>
#include "transaction.h"
#include "transaction_transport.h"
#include "runtime_transaction_log.h"
#include "transport_connection_info.h"
#include "ec_connection_notification_manager.h"
#include <core/resource_access/resource_access_manager.h>

#include "transaction_message_bus_base.h"

class QTimer;
class QnRuntimeTransactionLog;

namespace ec2 {

class ECConnectionNotificationManager;
class TimeSynchronizationManager;

class QnTransactionMessageBus: public TransactionMessageBusBase
{
    Q_OBJECT
    using base_type = TransactionMessageBusBase;

public:
    QnTransactionMessageBus(
        Qn::PeerType peerType,
        QnCommonModule* commonModule,
        QnJsonTransactionSerializer* jsonTranSerializer,
        QnUbjsonTransactionSerializer* ubjsonTranSerializer);

    ~QnTransactionMessageBus() override;

    //void addConnectionToPeer(const QUrl& url);
    //void removeConnectionFromPeer(const QUrl& url);
    virtual void addOutgoingConnectionToPeer(const QnUuid& id, const nx::utils::Url& url) override;
    virtual void removeOutgoingConnectionFromPeer(const QnUuid& id) override;


    virtual QVector<QnTransportConnectionInfo> connectionsInfo() const override;
    bool moveConnectionToReadyForStreaming(const QnUuid& connectionGuid);
    //!Blocks till connection \a connectionGuid is ready to accept new transactions
    void waitForNewTransactionsReady(const QnUuid& connectionGuid);
    void connectionFailure(const QnUuid& connectionGuid);

    ApiPeerData localPeer() const;

    virtual void stop() override;

    template<typename T>
    void sendTransaction(const QnTransaction<T>& tran, const QnPeerSet& dstPeers = QnPeerSet())
    {
        NX_ASSERT(tran.command != ApiCommand::NotDefined);

        QnMutexLocker lock(&m_mutex);

        if (m_connections.isEmpty())
            return;

        QnTransactionTransportHeader header(
            connectedServerPeers() << commonModule()->moduleGUID(), dstPeers);
        header.fillSequence(commonModule()->moduleGUID(), commonModule()->runningInstanceGUID());
        sendTransactionInternal(tran, header);
    }

    template<typename T>
    void sendTransaction(const QnTransaction<T>& tran, const QnUuid& dstPeerId)
    {
        dstPeerId.isNull() ? sendTransaction(tran) : sendTransaction(tran, {dstPeerId});
    }

    typedef QMap<QnUuid, RoutingRecord> RoutingInfo;
    struct AlivePeerInfo
    {
        AlivePeerInfo() : peer(QnUuid(), QnUuid(), Qn::PT_Server) {}
        AlivePeerInfo(const ApiPeerData &peer) : peer(peer) {}
        ApiPeerData peer;

        RoutingInfo routingInfo; // key: route throw, value - distance in hops
    };
    typedef QMap<QnUuid, AlivePeerInfo> AlivePeersMap;

    /*
     * Return all alive peers
     */
    AlivePeersMap alivePeers() const;

    /*
     * Return all alive server peers
     */
    QMap<QnUuid, ApiPeerData> aliveServerPeers() const;
    AlivePeersMap aliveClientPeers() const;

    virtual QSet<QnUuid> directlyConnectedClientPeers() const override;
    virtual QSet<QnUuid> directlyConnectedServerPeers() const override;
    virtual void dropConnections() override;

signals:
    void gotLockRequest(nx::vms::api::LockData);
    //void gotUnlockRequest(nx::vms::api::LockData);
    void gotLockResponse(nx::vms::api::LockData);

public slots:
    void reconnectAllPeers();

    /*
    * Return routing information: how to access to a dstPeer.
    * if peer can be access directly then return same value as input.
    * If can't find route info then return null value.
    * Otherwise return route gateway.
    */
    virtual QnUuid routeToPeerVia(const QnUuid& dstPeer, int* distance) const override;

    virtual int distanceToPeer(const QnUuid& dstPeer) const override;

private:
    friend class QnTransactionTransport;
    friend struct GotTransactionFuction;

    bool isExists(const QnUuid& removeGuid) const;
    bool isConnecting(const QnUuid& removeGuid) const;

protected:
    typedef QMap<QnUuid, QnTransactionTransport*> QnConnectionMap;

private:
    template<typename T>
    void sendTransactionInternal(
        const QnTransaction<T>& tran, const QnTransactionTransportHeader& header)
    {
        QnPeerSet toSendRest = header.dstPeers;
        QnPeerSet sentPeers;
        bool sendToAll = header.dstPeers.isEmpty();

        for (QnTransactionTransport* transport: m_connections)
        {
            if (!sendToAll && !header.dstPeers.contains(transport->remotePeer().id))
                continue;

            if (!transport->isReadyToSend(tran.command))
                continue;

            transport->sendTransaction(tran, header);
            sentPeers << transport->remotePeer().id;
            toSendRest.remove(transport->remotePeer().id);
        }

        // Some destination peers are not accessible directly, send broadcast (to all connected
        // peers except of just sent).
        if (!toSendRest.isEmpty() && !tran.isLocal())
        {
            for (QnTransactionTransport* transport: m_connections)
            {
                if (!transport->isReadyToSend(tran.command))
                    continue;

                if (sentPeers.contains(transport->remotePeer().id))
                    continue; //< Already sent.

                transport->sendTransaction(tran, header);
            }
        }
    }

    template <class T>
    void gotTransaction(const QnTransaction<T> &tran, QnTransactionTransport* sender, const QnTransactionTransportHeader &transportHeader);

	void onGotTransactionSyncResponse(QnTransactionTransport* sender, const QnTransaction<QnTranStateResponse> &tran);
    void onGotTransactionSyncDone(QnTransactionTransport* sender, const QnTransaction<ApiTranSyncDoneData> &tran);
    void onGotDistributedMutexTransaction(const QnTransaction<nx::vms::api::LockData>& tran);

    void connectToPeerEstablished(const ApiPeerData &peerInfo);
    void connectToPeerLost(const QnUuid& id);
    void handlePeerAliveChanged(const ApiPeerData& peer, bool isAlive, bool sendTran);
    bool isPeerUsing(const nx::utils::Url& url);
    void onGotServerAliveInfo(const QnTransaction<ApiPeerAliveData> &tran, QnTransactionTransport* transport, const QnTransactionTransportHeader& ttHeader);
    bool onGotServerRuntimeInfo(const QnTransaction<ApiRuntimeData> &tran, QnTransactionTransport* transport, const QnTransactionTransportHeader& ttHeader);

protected:
    /*
    * Return true if alive transaction accepted or false if it should be ignored (offline data is deprecated)
    */
    virtual bool gotAliveData(const ApiPeerAliveData &aliveData, QnTransactionTransport* transport, const QnTransactionTransportHeader* ttHeader);

	virtual bool checkSequence(const QnTransactionTransportHeader& transportHeader, const QnAbstractTransaction& tran, QnTransactionTransport* transport);

	void resyncWithPeer(QnTransactionTransport* transport);

	virtual void onGotTransactionSyncRequest(QnTransactionTransport* sender, const QnTransaction<ApiSyncRequestData> &tran);
	virtual void queueSyncRequest(QnTransactionTransport* transport);
	virtual bool sendInitialData(QnTransactionTransport* transport);
	virtual void fillExtraAliveTransactionParams(ApiPeerAliveData* outAliveData);
	virtual void logTransactionState();

	virtual void handleIncomingTransaction(
		QnTransactionTransport* sender,
		Qn::SerializationFormat tranFormat,
		QByteArray serializedTran,
		const QnTransactionTransportHeader &transportHeader);

    virtual ErrorCode updatePersistentMarker(
        const QnTransaction<nx::vms::api::UpdateSequenceData>& tran);

	void sendRuntimeInfo(QnTransactionTransport* transport, const QnTransactionTransportHeader& transportHeader, const QnTranState& runtimeState);

    template<typename T>
    void proxyTransaction(
        const QnTransaction<T>& tran,
        const QnTransactionTransportHeader& transportHeader);

    template<typename T>
    bool processSpecialTransaction(
        const QnTransaction<T>& tran,
        QnTransactionTransport* sender,
        const QnTransactionTransportHeader& transportHeader);

private:
    QnPeerSet connectedServerPeers() const;

    void addAlivePeerInfo(const ApiPeerData& peerData, const QnUuid& gotFromPeer, int distance);
    void removeAlivePeer(const QnUuid& id, bool sendTran, bool isRecursive = false);
    void removeTTSequenceForPeer(const QnUuid& id);
    void removePeersWithTimeout(const QSet<QnUuid>& lostPeers);
    QSet<QnUuid> checkAlivePeerRouteTimeout();
    void updateLastActivity(QnTransactionTransport* sender, const QnTransactionTransportHeader& transportHeader);
    void addDelayedAliveTran(QnTransaction<ApiPeerAliveData>&& tranToSend, int timeout);
    void sendDelayedAliveTran();
    void reconnectAllPeers(QnMutexLockerBase* const /*lock*/);
    nx::utils::Url updateOutgoingUrl(const QnUuid& peer, const nx::utils::Url& srcUrl) const;

    static void printTransaction(
        const char* prefix,
        const QnAbstractTransaction& tran,
        const QnUuid& hash,
        const QnTransactionTransportHeader& transportHeader,
        QnTransactionTransport* sender);

protected slots:
    void at_stateChanged(QnTransactionTransport::State state);
    void at_gotTransaction(
        Qn::SerializationFormat tranFormat,
        QByteArray serializedTran,
        const QnTransactionTransportHeader &transportHeader);
    void doPeriodicTasks();
    void at_peerIdDiscovered(const nx::utils::Url &url, const QnUuid& id);
    void at_runtimeDataUpdated(const QnTransaction<ApiRuntimeData>& data);
    void emitRemotePeerUnauthorized(const QnUuid& id);
    void onEc2ConnectionSettingsChanged(const QString& key);

protected:
	QnConnectionMap m_connections;
	std::shared_ptr<QnRuntimeTransactionLog> m_runtimeTransactionLog;
	bool m_restartPending = false;
	QVector<QnTransactionTransport*> m_connectingConnections;

private:
    struct RemoteUrlConnectInfo
    {
        RemoteUrlConnectInfo(const QnUuid& id = QnUuid()):
            id(id)
        {
            lastConnectedTime.invalidate();
        }
        QElapsedTimer lastConnectedTime;
        QnUuid discoveredPeer;
        QElapsedTimer discoveredTimeout;
        QnUuid id;
    };

    QMap<nx::utils::Url, RemoteUrlConnectInfo> m_remoteUrls;
    QTimer* m_timer = nullptr;

    QMap<ApiPersistentIdData, int> m_lastTransportSeq;

    // alive control
    QElapsedTimer m_aliveSendTimer;
    QElapsedTimer m_currentTimeTimer;

    struct DelayedAliveData
    {
        QnTransaction<ApiPeerAliveData> tran;
        qint64 timeToSend;
    };

    QMap<QnUuid, DelayedAliveData> m_delayedAliveTran;
    QElapsedTimer m_relativeTimer;

    AlivePeersMap m_alivePeers; //< alive peers in a system

};

} // namespace ec2
