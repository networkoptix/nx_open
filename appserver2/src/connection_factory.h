/**********************************************************
* 21 jan 2014
* a.kolesnikov
***********************************************************/

#ifndef EC2_CONNECTION_FACTORY_H
#define EC2_CONNECTION_FACTORY_H

#include <memory>

#include <QtCore/QMutex>

#include <utils/common/joinable.h>
#include <utils/common/stoppable.h>

#include "nx_ec/ec_api.h"
#include "nx_ec/data/api_connection_data.h"
#include "client_query_processor.h"
#include "server_query_processor.h"
#include "ec2_connection.h"
#include "managers/time_manager.h"


namespace ec2
{
    class QnRuntimeTransactionLog;

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
        virtual int connectAsync( const QUrl& addr, impl::ConnectHandlerPtr handler ) override;

        virtual void registerRestHandlers( QnRestProcessorPool* const restProcessorPool ) override;
        virtual void registerTransactionListener( QnUniversalTcpListener* universalTcpListener ) override;
        virtual void setContext( const ResourceContext& resCtx ) override;

    private:
        ServerQueryProcessor m_serverQueryProcessor;
        ClientQueryProcessor m_remoteQueryProcessor;
        QMutex m_mutex;
        ResourceContext m_resCtx;
        std::unique_ptr<QnDbManager> m_dbManager;
        std::unique_ptr<QnTransactionMessageBus> m_transactionMessageBus;
        std::unique_ptr<TimeSynchronizationManager> m_timeSynchronizationManager;
        Ec2DirectConnectionPtr m_directConnection;
        Ec2ThreadPool m_ec2ThreadPool;
        bool m_terminated;
        int m_runningRequests;
        bool m_sslEnabled;
        std::unique_ptr<QnRuntimeTransactionLog> m_runtimeTransactionLog;

        int establishDirectConnection(const QUrl& url, impl::ConnectHandlerPtr handler);
        int establishConnectionToRemoteServer( const QUrl& addr, impl::ConnectHandlerPtr handler );
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
            QnConnectionInfo* const connectionInfo );
        int testDirectConnection( const QUrl& addr, impl::TestConnectionHandlerPtr handler );
        int testRemoteConnection( const QUrl& addr, impl::TestConnectionHandlerPtr handler );

        template<class InputDataType>
            void registerUpdateFuncHandler( QnRestProcessorPool* const restProcessorPool, ApiCommand::Value cmd );

        template<class InputDataType, class CustomActionType>
            void registerUpdateFuncHandler( QnRestProcessorPool* const restProcessorPool, ApiCommand::Value cmd, CustomActionType customAction );

        template<class InputDataType, class OutputDataType>
            void registerGetFuncHandler( QnRestProcessorPool* const restProcessorPool, ApiCommand::Value cmd );

        template<class InputType, class OutputType, class HandlerType>
            void registerFunctorHandler(
                QnRestProcessorPool* const restProcessorPool,
                ApiCommand::Value cmd,
                HandlerType handler );
    };
}

#endif  //EC2_CONNECTION_FACTORY_H
