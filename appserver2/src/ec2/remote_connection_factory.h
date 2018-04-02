#pragma once

#include <memory>
#include <map>

#include <common/common_module_aware.h>

#include <nx/utils/thread/mutex.h>
#include <nx/utils/timer_manager.h>

#include <nx/utils/thread/joinable.h>
#include <nx/utils/thread/stoppable.h>

#include <nx_ec/ec_api.h>

#include "settings.h"

namespace ec2 {

	class ClientQueryProcessor;

// TODO: #2.4 remove Ec2 prefix to avoid ec2::RemoteConnectionFactory
    class RemoteConnectionFactory :
        public AbstractECConnectionFactory,
        public QnStoppable,
        public QnJoinable,
        public QnCommonModuleAware
    {
    public:
        RemoteConnectionFactory(
            Qn::PeerType peerType,
            nx::utils::TimerManager* const timerManager,
            QnCommonModule* commonModule,
            bool isP2pMode);
        virtual ~RemoteConnectionFactory();

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
            const ApiClientInfoData& clientInfo,
            impl::ConnectHandlerPtr handler) override;

        virtual void setConfParams(std::map<QString, QVariant> confParams) override;

		virtual TransactionMessageBusAdapter* messageBus() const override;
        virtual TimeSynchronizationManager* timeSyncManager() const override;

        virtual void shutdown() override;

private:
    QnMutex m_mutex;
    Settings m_settingsInstance;
	std::unique_ptr<TransactionMessageBusAdapter> m_bus;
	std::unique_ptr<TimeSynchronizationManager> m_timeSynchronizationManager;
    bool m_terminated;
    int m_runningRequests;
    bool m_sslEnabled;

    std::unique_ptr<ClientQueryProcessor> m_remoteQueryProcessor;
    bool m_p2pMode = false;
private:
    int establishConnectionToRemoteServer(
        const nx::utils::Url& addr, impl::ConnectHandlerPtr handler, const ApiClientInfoData& clientInfo);

    void tryConnectToOldEC(const nx::utils::Url& ecUrl, impl::ConnectHandlerPtr handler, int reqId);

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
        const ApiLoginData& loginInfo,
        QnConnectionInfo* const connectionInfo,
        nx::network::http::Response* response = nullptr);

    int testDirectConnection(const nx::utils::Url& addr, impl::TestConnectionHandlerPtr handler);
    int testRemoteConnection(const nx::utils::Url& addr, impl::TestConnectionHandlerPtr handler);

    template<class Function>
    void statisticsCall(const Function& function);
};

} // namespace ec2
