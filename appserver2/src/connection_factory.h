/**********************************************************
* 21 jan 2014
* a.kolesnikov
***********************************************************/

#ifndef EC2_CONNECTION_FACTORY_H
#define EC2_CONNECTION_FACTORY_H

#include <memory>

#include <nx/utils/thread/mutex.h>

#include <utils/common/joinable.h>
#include <utils/common/stoppable.h>

#include "nx_ec/ec_api.h"
#include "nx_ec/data/api_connection_data.h"
#include "client_query_processor.h"
#include "server_query_processor.h"
#include "ec2_connection.h"
#include "managers/time_manager.h"
#include "settings.h"


namespace ec2
{
    // TODO: #2.4 remove Ec2 prefix to avoid ec2::Ec2DirectConnectionFactory
    class Ec2DirectConnectionFactory
    :
        public AbstractECConnectionFactory,
        public QnStoppable,
        public QnJoinable
    {
    public:
        Ec2DirectConnectionFactory( Qn::PeerType peerType );
        virtual ~Ec2DirectConnectionFactory();

        virtual void pleaseStop() override;
        virtual void join() override;

        //!Implementation of AbstractECConnectionFactory::testConnectionAsync
        virtual int testConnectionAsync( const QUrl& addr, impl::TestConnectionHandlerPtr handler ) override;
        //!Implementation of AbstractECConnectionFactory::connectAsync
        virtual int connectAsync( const QUrl& addr, const ApiClientInfoData& clientInfo,
                                  impl::ConnectHandlerPtr handler ) override;

        virtual void registerRestHandlers( QnRestProcessorPool* const restProcessorPool ) override;
        virtual void registerTransactionListener(QnHttpConnectionListener* httpConnectionListener) override;
        virtual void setConfParams( std::map<QString, QVariant> confParams ) override;

    private:
        ServerQueryProcessorAccess m_serverQueryProcessor;
        ClientQueryProcessor m_remoteQueryProcessor;
        QnMutex m_mutex;
        Settings m_settingsInstance;
        std::unique_ptr<detail::QnDbManager> m_dbManager;
        std::unique_ptr<TimeSynchronizationManager> m_timeSynchronizationManager;
        std::unique_ptr<QnTransactionMessageBus> m_transactionMessageBus;
        Ec2DirectConnectionPtr m_directConnection;
        Ec2ThreadPool m_ec2ThreadPool;
        bool m_terminated;
        int m_runningRequests;
        bool m_sslEnabled;

        int establishDirectConnection(const QUrl& url, impl::ConnectHandlerPtr handler);
        int establishConnectionToRemoteServer( const QUrl& addr, impl::ConnectHandlerPtr handler,
                                               const ApiClientInfoData& clientInfo );
        template<class Handler>
        void connectToOldEC( const QUrl& ecURL, Handler completionFunc );
            //!Called on client side after receiving connection response from remote server
        void remoteConnectionFinished(
            int reqID,
            ErrorCode errorCode,
            const QnConnectionInfo& connectionInfo,
            const QUrl& ecURL,
            impl::ConnectHandlerPtr handler );
        //!Called on client side after receiving test connection response from remote server
        void remoteTestConnectionFinished(
            int reqID,
            ErrorCode errorCode,
            const QnConnectionInfo& connectionInfo,
            const QUrl& ecURL,
            impl::TestConnectionHandlerPtr handler );
        //!Called on server side to handle connection request from remote host
        ErrorCode fillConnectionInfo(
            const ApiLoginData& loginInfo,
            QnConnectionInfo* const connectionInfo,
            nx_http::Response* response = nullptr);
        int testDirectConnection( const QUrl& addr, impl::TestConnectionHandlerPtr handler );
        int testRemoteConnection( const QUrl& addr, impl::TestConnectionHandlerPtr handler );
        ErrorCode getSettings( std::nullptr_t, ApiResourceParamDataList* const outData );

        template<class InputDataType>
            void registerUpdateFuncHandler( QnRestProcessorPool* const restProcessorPool, ApiCommand::Value cmd, Qn::GlobalPermission permission = Qn::NoGlobalPermissions);

        template<class InputDataType, class CustomActionType>
            void registerUpdateFuncHandler( QnRestProcessorPool* const restProcessorPool, ApiCommand::Value cmd, CustomActionType customAction, Qn::GlobalPermission permission = Qn::NoGlobalPermissions);

        template<class InputDataType, class OutputDataType>
            void registerGetFuncHandler( QnRestProcessorPool* const restProcessorPool, ApiCommand::Value cmd, Qn::GlobalPermission permission = Qn::NoGlobalPermissions);

        template<class InputType, class OutputType>
            void registerFunctorHandler(
                QnRestProcessorPool* const restProcessorPool,
                ApiCommand::Value cmd,
                std::function<ErrorCode(InputType, OutputType*)> handler, Qn::GlobalPermission permission = Qn::NoGlobalPermissions);
        template<class InputType, class OutputType>
            void registerFunctorWithResponseHandler(
                QnRestProcessorPool* const restProcessorPool,
                ApiCommand::Value cmd,
                std::function<ErrorCode(InputType, OutputType*, nx_http::Response*)> handler, Qn::GlobalPermission permission = Qn::NoGlobalPermissions);

        template<class Function>
            void statisticsCall(const Function& function);
    };
}

#endif  //EC2_CONNECTION_FACTORY_H
