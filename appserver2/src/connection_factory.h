/**********************************************************
* 21 jan 2014
* a.kolesnikov
***********************************************************/

#ifndef EC2_CONNECTION_FACTORY_H
#define EC2_CONNECTION_FACTORY_H

#include <memory>

#include <QtCore/QMutex>

#include "nx_ec/ec_api.h"
#include "nx_ec/data/api_connection_data.h"
#include "client_query_processor.h"
#include "server_query_processor.h"
#include "ec2_connection.h"
#include "managers/time_manager.h"


namespace ec2
{
    class Ec2DirectConnectionFactory
    :
        public AbstractECConnectionFactory
    {
    public:
        Ec2DirectConnectionFactory( Qn::PeerType peerType );
        virtual ~Ec2DirectConnectionFactory();

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
        //std::map<QUrl, AbstractECConnectionPtr> m_urlToConnection;
        Ec2DirectConnectionPtr m_directConnection;

        int establishDirectConnection(const QUrl& url, impl::ConnectHandlerPtr handler);
        int establishConnectionToRemoteServer( const QUrl& addr, impl::ConnectHandlerPtr handler );
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
