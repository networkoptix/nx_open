#pragma once

#include <memory>
#include <map>

#include <common/common_module_aware.h>

#include <nx/utils/thread/mutex.h>
#include <nx/utils/timer_manager.h>

#include <nx/utils/thread/joinable.h>
#include <nx/utils/thread/stoppable.h>

#include <nx_ec/ec_api.h>

#include <transaction/threadsafe_message_bus_adapter.h>
#include <nx/vms/time_sync/abstract_time_sync_manager.h>

namespace ec2 {

class ClientQueryProcessor;

class RemoteConnectionFactory:
    public AbstractECConnectionFactory,
    public QnStoppable,
    public QnJoinable
{
public:
    RemoteConnectionFactory(
        QnCommonModule* commonModule,
        nx::vms::api::PeerType peerType,
        bool isP2pMode);
    virtual ~RemoteConnectionFactory() override;

    virtual void pleaseStop() override;
    virtual void join() override;

    /**
     * Implementation of AbstractECConnectionFactory::testConnectionAsync.
     */
    virtual int testConnectionAsync(
        const nx::utils::Url& addr,
        impl::TestConnectionHandlerPtr handler) override;

    /**
     * Implementation of AbstractECConnectionFactory::connectAsync.
     */
    virtual int connectAsync(
        const nx::utils::Url& addr,
        const nx::vms::api::ClientInfoData& clientInfo,
        impl::ConnectHandlerPtr handler) override;

    virtual TransactionMessageBusAdapter* messageBus() const override;
    virtual nx::vms::time_sync::AbstractTimeSyncManager* timeSyncManager() const override;

    virtual void shutdown() override;

private:
    QnMutex m_mutex;
    std::unique_ptr<ThreadsafeMessageBusAdapter> m_bus;
    std::unique_ptr<QnJsonTransactionSerializer> m_jsonTranSerializer;
    std::unique_ptr<QnUbjsonTransactionSerializer> m_ubjsonTranSerializer;
    std::unique_ptr<nx::vms::time_sync::AbstractTimeSyncManager> m_timeSynchronizationManager;
    bool m_terminated;
    int m_runningRequests;
    bool m_sslEnabled;

    std::unique_ptr<ClientQueryProcessor> m_remoteQueryProcessor;
    bool m_p2pMode = false;
    nx::vms::api::PeerType m_peerType = nx::vms::api::PeerType::notDefined;

private:
    int establishConnectionToRemoteServer(
        const nx::utils::Url& addr,
        impl::ConnectHandlerPtr handler,
        const nx::vms::api::ClientInfoData& clientInfo);

    void tryConnectToOldEC(
        const nx::utils::Url& ecUrl,
        impl::ConnectHandlerPtr handler,
        int reqId);

    template<class Handler>
    void connectToOldEC(const nx::utils::Url& ecURL, Handler completionFunc);

    /**
     * Called on the client side after receiving connection response from a remote server.
     */
    void remoteConnectionFinished(
        int reqId,
        ErrorCode errorCode,
        const QnConnectionInfo& connectionInfo,
        const nx::utils::Url& ecUrl,
        impl::ConnectHandlerPtr handler);

    /**
     * Called on the client side after receiving test connection response from a remote server.
     */
    void remoteTestConnectionFinished(
        int reqId,
        ErrorCode errorCode,
        const QnConnectionInfo& connectionInfo,
        const nx::utils::Url& ecUrl,
        impl::TestConnectionHandlerPtr handler);

    /**
     * Called on server side to handle connection request from remote host.
     */
    ErrorCode fillConnectionInfo(
        const nx::vms::api::ConnectionData& loginInfo,
        QnConnectionInfo* const connectionInfo,
        nx::network::http::Response* response = nullptr);

    int testDirectConnection(const nx::utils::Url& addr, impl::TestConnectionHandlerPtr handler);
    int testRemoteConnection(const nx::utils::Url& addr, impl::TestConnectionHandlerPtr handler);

    template<class Function>
    void statisticsCall(const Function& function);
};

} // namespace ec2
