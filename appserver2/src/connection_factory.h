/**********************************************************
* 21 jan 2014
* a.kolesnikov
***********************************************************/

#ifndef EC2_CONNECTION_FACTORY_H
#define EC2_CONNECTION_FACTORY_H

#include <memory>
#include <mutex>

#include "nx_ec/ec_api.h"
#include "nx_ec/data/connection_data.h"
#include "client_query_processor.h"
#include "server_query_processor.h"


namespace ec2
{
    class Ec2DirectConnectionFactory
    :
        public AbstractECConnectionFactory
    {
    public:
        Ec2DirectConnectionFactory();
        virtual ~Ec2DirectConnectionFactory();

        //!Implementation of AbstractECConnectionFactory::testConnectionAsync
        virtual ReqID testConnectionAsync( const QUrl& addr, impl::SimpleHandlerPtr handler ) override;
        //!Implementation of AbstractECConnectionFactory::connectAsync
        virtual ReqID connectAsync( const QUrl& addr, impl::ConnectHandlerPtr handler ) override;

        virtual void registerRestHandlers( QnRestProcessorPool* const restProcessorPool ) override;
        virtual void setContext( const ResourceContext& resCtx ) override;

    private:
        ServerQueryProcessor m_serverQueryProcessor;
        ClientQueryProcessor m_remoteQueryProcessor;
        AbstractECConnectionPtr m_directConnection;
        std::mutex m_mutex;
        ResourceContext m_resCtx;

        ReqID establishDirectConnection( impl::ConnectHandlerPtr handler );
        ReqID establishConnectionToRemoteServer( const QUrl& addr, impl::ConnectHandlerPtr handler );
        //!Called on client side after receiving connection response from remote server
        void remoteConnectionFinished(
            ReqID reqID,
            ErrorCode errorCode,
            const ConnectionInfo& connectionInfo,
            impl::ConnectHandlerPtr handler );
        //!Called on server side to handle connection request from remote host
        ErrorCode doRemoteConnectionSync(
            const LoginInfo& loginInfo,
            ConnectionInfo* const connectionInfo );
    };
}

#endif  //EC2_CONNECTION_FACTORY_H
