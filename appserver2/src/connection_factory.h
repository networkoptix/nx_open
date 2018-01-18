#pragma once

#include <memory>
#include <map>

#include <common/common_module_aware.h>

#include <nx/utils/thread/mutex.h>
#include <nx/utils/timer_manager.h>

#include <nx/utils/thread/joinable.h>
#include <nx/utils/thread/stoppable.h>

#include <nx_ec/ec_api.h>
#include <nx_ec/data/api_connection_data.h>

#include "client_query_processor.h"
#include "server_query_processor.h"
#include "ec2_connection.h"
#include "managers/time_manager.h"
#include "settings.h"
#include <transaction/transaction_log.h>
#include <mutex/distributed_mutex_manager.h>
#include <nx/p2p/p2p_message_bus.h>

#include <transaction/json_transaction_serializer.h>
#include <transaction/ubjson_transaction_serializer.h>

namespace ec2 {

// TODO: #2.4 remove Ec2 prefix to avoid ec2::Ec2DirectConnectionFactory
    class Ec2DirectConnectionFactory :
        public AbstractECConnectionFactory,
        public QnStoppable,
        public QnJoinable,
        public QnCommonModuleAware
    {
    public:
        Ec2DirectConnectionFactory(
            Qn::PeerType peerType,
            nx::utils::TimerManager* const timerManager,
            QnCommonModule* commonModule,
            bool isP2pMode);
        virtual ~Ec2DirectConnectionFactory();

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

        virtual void registerRestHandlers(QnRestProcessorPool* const restProcessorPool) override;

        virtual void registerTransactionListener(
            QnHttpConnectionListener* httpConnectionListener) override;

        virtual void setConfParams(std::map<QString, QVariant> confParams) override;

        virtual TransactionMessageBusAdapter* messageBus() const override;
        virtual QnDistributedMutexManager* distributedMutex() const override;
        virtual TimeSynchronizationManager* timeSyncManager() const override;

        QnJsonTransactionSerializer* jsonTranSerializer() const;
        QnUbjsonTransactionSerializer* ubjsonTranSerializer() const;
        virtual void shutdown() override;

private:
    QnMutex m_mutex;
    Settings m_settingsInstance;

    std::unique_ptr<QnJsonTransactionSerializer> m_jsonTranSerializer;
    std::unique_ptr<QnUbjsonTransactionSerializer> m_ubjsonTranSerializer;

    std::unique_ptr<detail::QnDbManager> m_dbManager;
    std::unique_ptr<QnTransactionLog> m_transactionLog;
    std::unique_ptr<TransactionMessageBusAdapter> m_bus;

    std::unique_ptr<ServerQueryProcessorAccess> m_serverQueryProcessor;
    std::unique_ptr<TimeSynchronizationManager> m_timeSynchronizationManager;
    std::unique_ptr<QnDistributedMutexManager> m_distributedMutexManager;
    bool m_terminated;
    int m_runningRequests;
    bool m_sslEnabled;

    ClientQueryProcessor m_remoteQueryProcessor;
    Ec2DirectConnectionPtr m_directConnection;
    bool m_p2pMode = false;
private:
    int establishDirectConnection(const nx::utils::Url& url, impl::ConnectHandlerPtr handler);
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
    ErrorCode getSettings(nullptr_t, ApiResourceParamDataList* const outData, const Qn::UserAccessData&);

    template<class InputDataType>
    void regUpdate(QnRestProcessorPool* const restProcessorPool, ApiCommand::Value cmd,
        Qn::GlobalPermission permission = Qn::NoGlobalPermissions);

    template<class InputDataType, class CustomActionType>
    void regUpdate(QnRestProcessorPool* const restProcessorPool, ApiCommand::Value cmd,
        CustomActionType customAction, Qn::GlobalPermission permission = Qn::NoGlobalPermissions);

    template<class InputDataType, class OutputDataType>
    void regGet(QnRestProcessorPool* const restProcessorPool, ApiCommand::Value cmd,
        Qn::GlobalPermission permission = Qn::NoGlobalPermissions);

    template<class InputType, class OutputType>
    void regFunctor(
        QnRestProcessorPool* const restProcessorPool,
        ApiCommand::Value cmd,
        std::function<ErrorCode(InputType, OutputType*, const Qn::UserAccessData&)> handler,
        Qn::GlobalPermission permission = Qn::NoGlobalPermissions);

    template<class InputType, class OutputType>
    void regFunctorWithResponse(
        QnRestProcessorPool* const restProcessorPool,
        ApiCommand::Value cmd,
        std::function<ErrorCode(InputType, OutputType*, nx::network::http::Response*)> handler,
        Qn::GlobalPermission permission = Qn::NoGlobalPermissions);

    template<class Function>
    void statisticsCall(const Function& function);
};

} // namespace ec2
