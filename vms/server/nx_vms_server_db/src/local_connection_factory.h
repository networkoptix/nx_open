#pragma once

#include <memory>
#include <map>

#include <common/common_module_aware.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/timer_manager.h>
#include <nx/utils/thread/joinable.h>
#include <nx/utils/thread/stoppable.h>
#include <nx_ec/ec_api.h>

#include "ec2_connection.h"
#include <nx/vms/network/abstract_server_connector.h>
#include <nx/vms/network/reverse_connection_manager.h>
#include <nx/vms/time_sync/server_time_sync_manager.h>

namespace ec2 {

struct ServerQueryProcessorAccess;
class QnDistributedMutexManager;
class ClientRegistrar;

class LocalConnectionFactory:
    public AbstractECConnectionFactory,
    public QnStoppable,
    public QnJoinable
{
public:
    LocalConnectionFactory(
        QnCommonModule* commonModule,
        nx::vms::api::PeerType peerType,
        bool isP2pMode,
        bool ecDbReadOnly,
        QnHttpConnectionListener* tcpListener);

    virtual ~LocalConnectionFactory();

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

    void registerRestHandlers(QnRestProcessorPool* const restProcessorPool);

    virtual void registerTransactionListener(
        QnHttpConnectionListener* httpConnectionListener);

    TransactionMessageBusAdapter* messageBus() const;
    QnDistributedMutexManager* distributedMutex() const;
    virtual nx::vms::time_sync::AbstractTimeSyncManager* timeSyncManager() const override;

    QnJsonTransactionSerializer* jsonTranSerializer() const;
    QnUbjsonTransactionSerializer* ubjsonTranSerializer() const;
    virtual void shutdown() override;
    nx::vms::network::ReverseConnectionManager* serverConnector() const;

private:
    QnMutex m_mutex;

    std::unique_ptr<QnJsonTransactionSerializer> m_jsonTranSerializer;
    std::unique_ptr<QnUbjsonTransactionSerializer> m_ubjsonTranSerializer;
    Ec2DirectConnectionPtr m_directConnection;
    std::unique_ptr<nx::vms::network::ReverseConnectionManager> m_serverConnector;
    std::unique_ptr<nx::vms::time_sync::ServerTimeSyncManager> m_timeSynchronizationManager;

    std::unique_ptr<detail::QnDbManager> m_dbManager;
    std::unique_ptr<QnTransactionLog> m_transactionLog;
    std::unique_ptr<TransactionMessageBusAdapter> m_bus;

    std::shared_ptr<ServerQueryProcessorAccess> m_serverQueryProcessor;
    std::unique_ptr<QnDistributedMutexManager> m_distributedMutexManager;
    std::unique_ptr<ClientRegistrar> m_clientRegistrar;
    bool m_terminated;
    int m_runningRequests;
    bool m_sslEnabled;

    bool m_p2pMode = false;
    bool m_ecDbReadOnly = false;
private:

    int establishDirectConnection(const nx::utils::Url& url, impl::ConnectHandlerPtr handler);

    void tryConnectToOldEC(const nx::utils::Url& ecUrl, impl::ConnectHandlerPtr handler, int reqId);

    template<class Handler>
    void connectToOldEC(const nx::utils::Url& ecURL, Handler completionFunc);

    /**
    * Called on server side to handle connection request from remote host.
    */
    ErrorCode fillConnectionInfo(
        const nx::vms::api::ConnectionData& loginInfo,
        QnConnectionInfo* const connectionInfo,
        nx::network::http::Response* response = nullptr);

    int testDirectConnection(const nx::utils::Url& addr, impl::TestConnectionHandlerPtr handler);
    ErrorCode getSettings(
        std::nullptr_t,
        nx::vms::api::ResourceParamDataList* const outData,
        const Qn::UserAccessData&);

    template<class InputDataType, class ProcessedDataType = InputDataType>
    void regUpdate(QnRestProcessorPool* const restProcessorPool, ApiCommand::Value cmd,
        GlobalPermission permission = GlobalPermission::none);

    template<class InputDataType, class CustomActionType>
    void regUpdate(QnRestProcessorPool* const restProcessorPool, ApiCommand::Value cmd,
        CustomActionType customAction, GlobalPermission permission = GlobalPermission::none);

    template<class InputDataType, class OutputDataType>
    void regGet(QnRestProcessorPool* const restProcessorPool, ApiCommand::Value cmd,
        GlobalPermission permission = GlobalPermission::none);

    template<class InputType, class OutputType>
    void regFunctor(
        QnRestProcessorPool* const restProcessorPool,
        ApiCommand::Value cmd,
        std::function<ErrorCode(InputType, OutputType*, const Qn::UserAccessData&)> handler,
        GlobalPermission permission = GlobalPermission::none);

    template<class InputType, class OutputType>
    void regFunctorWithResponse(
        QnRestProcessorPool* const restProcessorPool,
        ApiCommand::Value cmd,
        std::function<ErrorCode(InputType, OutputType*, nx::network::http::Response*)> handler,
        GlobalPermission permission = GlobalPermission::none);

    template<class Function>
    void statisticsCall(const Function& function);
};

} // namespace ec2
