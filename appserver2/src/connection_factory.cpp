/**********************************************************
* 21 jan 2014
* a.kolesnikov
***********************************************************/

#include "connection_factory.h"

#include <functional>

#include <QtConcurrent>

#include "rest/ec2_query_http_handler.h"
#include "rest/ec2_update_http_handler.h"
#include "transaction/transaction.h"


namespace ec2
{
    Ec2DirectConnectionFactory::Ec2DirectConnectionFactory()
    {
    }

    Ec2DirectConnectionFactory::~Ec2DirectConnectionFactory()
    {
    }

    //!Implementation of AbstractECConnectionFactory::testConnectionAsync
    ReqID Ec2DirectConnectionFactory::testConnectionAsync( const QUrl& /*addr*/, impl::SimpleHandlerPtr handler )
    {
        QtConcurrent::run( std::bind( std::mem_fn( &impl::SimpleHandler::done ), handler, ec2::ErrorCode::ok ) );
        return 0;
    }

    //!Implementation of AbstractECConnectionFactory::connectAsync
    ReqID Ec2DirectConnectionFactory::connectAsync( const QUrl& addr, impl::ConnectHandlerPtr handler )
    {
        if( addr.isEmpty() )
            return establishDirectConnection( handler );
        else
            return establishConnectionToRemoteServer( addr, handler );
    }

    void Ec2DirectConnectionFactory::registerRestHandlers( QnRestProcessorPool* const restProcessorPool )
    {
        restProcessorPool->registerHandler(
            QString::fromLatin1("ec2/%1").arg(ApiCommand::toString(ApiCommand::getResourceTypes)),
            new QueryHttpHandler<nullptr_t, ApiResourceTypeList>(&m_serverQueryProcessor, ApiCommand::getResourceTypes) );

        restProcessorPool->registerHandler(
            QString::fromLatin1("ec2/%1").arg(ApiCommand::toString(ApiCommand::addCamera)),
            new UpdateHttpHandler<ApiCameraData>(&m_serverQueryProcessor) );
        restProcessorPool->registerHandler(
            QString::fromLatin1("ec2/%1").arg(ApiCommand::toString(ApiCommand::getCameras)),
            new QueryHttpHandler<QnId, ApiCameraDataList>(&m_serverQueryProcessor, ApiCommand::getCameras) );
    }

    void Ec2DirectConnectionFactory::setContext( const ResourceContext& resCtx )
    {
        m_resCtx = resCtx;
    }

    ReqID Ec2DirectConnectionFactory::establishDirectConnection( impl::ConnectHandlerPtr handler )
    {
        {
            std::lock_guard<std::mutex> lk( m_mutex );
            if( !m_directConnection )
                m_directConnection.reset( new Ec2DirectConnection( &m_serverQueryProcessor, m_resCtx ) );
        }
        QtConcurrent::run( std::bind( std::mem_fn( &impl::ConnectHandler::done ), handler, ec2::ErrorCode::ok, m_directConnection ) );
        return 0;
    }

    ReqID Ec2DirectConnectionFactory::establishConnectionToRemoteServer( const QUrl& addr, impl::ConnectHandlerPtr handler )
    {
        LoginInfo loginInfo;
        m_remoteQueryProcessor.processQueryAsync<LoginInfo, ConnectionInfo>(
            ApiCommand::connect, std::move(loginInfo), std::bind(std::mem_fn(&Ec2DirectConnectionFactory::remoteConnectionFinished), this) );
        //TODO/IMPL
        return 0;
    }

    void Ec2DirectConnectionFactory::remoteConnectionFinished( ErrorCode errorCode, const ConnectionInfo& connectionInfo )
    {
        //TODO/IMPL
    }
}
