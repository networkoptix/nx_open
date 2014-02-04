/**********************************************************
* 21 jan 2014
* a.kolesnikov
***********************************************************/

#include "connection_factory.h"

#include <functional>

#include <QtConcurrent>

#include "ec2_connection.h"
#include "remote_ec_connection.h"
#include "rest/ec2_base_query_http_handler.h"
#include "rest/ec2_query_http_handler.h"
#include "rest/ec2_update_http_handler.h"
#include "transaction/transaction.h"
#include "version.h"


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
        const ReqID reqID = generateRequestID();
        QtConcurrent::run( std::bind( std::mem_fn( &impl::SimpleHandler::done ), handler, reqID, ec2::ErrorCode::ok ) );
        return reqID;
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
        using namespace std::placeholders;

        auto doRemoteConnectionSyncFunc = std::bind( &Ec2DirectConnectionFactory::doRemoteConnectionSync, this, _1, _2 );
        restProcessorPool->registerHandler(
            lit("ec2/%1").arg(ApiCommand::toString(ApiCommand::connect)),
            new FlexibleQueryHttpHandler<LoginInfo, ConnectionInfo, decltype(doRemoteConnectionSyncFunc)>
                (ApiCommand::connect, doRemoteConnectionSyncFunc) );

        auto getCurrentTimeSyncFunc = std::bind( &CommonRequestsProcessor::getCurrentTime, _1, _2 );
        restProcessorPool->registerHandler(
            lit("ec2/%1").arg(ApiCommand::toString(ApiCommand::getCurrentTime)),
            new FlexibleQueryHttpHandler<nullptr_t, qint64, decltype(getCurrentTimeSyncFunc)>
                (ApiCommand::getCurrentTime, getCurrentTimeSyncFunc) );

        restProcessorPool->registerHandler(
            lit("ec2/%1").arg(ApiCommand::toString(ApiCommand::getResourceTypes)),
            new QueryHttpHandler2<nullptr_t, ApiResourceTypeList>(ApiCommand::getResourceTypes, &m_serverQueryProcessor) );

        restProcessorPool->registerHandler(
            lit("ec2/%1").arg(ApiCommand::toString(ApiCommand::addCamera)),
            new UpdateHttpHandler<ApiCameraData>(&m_serverQueryProcessor) );
        restProcessorPool->registerHandler(
            lit("ec2/%1").arg(ApiCommand::toString(ApiCommand::getCameras)),
            new QueryHttpHandler<QnId, ApiCameraDataList>(&m_serverQueryProcessor, ApiCommand::getCameras) );
    }

    void Ec2DirectConnectionFactory::setContext( const ResourceContext& resCtx )
    {
        m_resCtx = resCtx;
    }

    ReqID Ec2DirectConnectionFactory::establishDirectConnection( impl::ConnectHandlerPtr handler )
    {
        const ReqID reqID = generateRequestID();

        LoginInfo loginInfo;
        ConnectionInfo internalConnectionInfo;
        doRemoteConnectionSync( loginInfo, &internalConnectionInfo );   //todo: #ak not appropriate here
        QnConnectionInfo connectionInfo;
        internalConnectionInfo.copy( &connectionInfo );
        {
            std::lock_guard<std::mutex> lk( m_mutex );
            if( !m_directConnection )
                m_directConnection.reset( new Ec2DirectConnection( &m_serverQueryProcessor, m_resCtx, connectionInfo ) );
        }
        QtConcurrent::run( std::bind( std::mem_fn( &impl::ConnectHandler::done ), handler, reqID, ec2::ErrorCode::ok, m_directConnection ) );
        return reqID;
    }

    ReqID Ec2DirectConnectionFactory::establishConnectionToRemoteServer( const QUrl& addr, impl::ConnectHandlerPtr handler )
    {
        const ReqID reqID = generateRequestID();

        LoginInfo loginInfo;
#if 1
        auto func = [this, reqID, handler]( ErrorCode errorCode, const ConnectionInfo& connectionInfo ) {
            remoteConnectionFinished(reqID, errorCode, connectionInfo, handler); };
        m_remoteQueryProcessor.processQueryAsync<LoginInfo, ConnectionInfo>(
            ApiCommand::connect, loginInfo, func );
#else
        using namespace std::placeholders;
        m_remoteQueryProcessor.processQueryAsync<LoginInfo, ConnectionInfo>(
            ApiCommand::connect, loginInfo, std::bind(&Ec2DirectConnectionFactory::remoteConnectionFinished, this, _1, _2) );
#endif
        return reqID;
    }

    void Ec2DirectConnectionFactory::remoteConnectionFinished(
        ReqID reqID,
        ErrorCode errorCode,
        const ConnectionInfo& connectionInfo,
        impl::ConnectHandlerPtr handler )
    {
        if( errorCode != ErrorCode::ok )
            return handler->done( reqID, errorCode, AbstractECConnectionPtr() );
        QnConnectionInfo outConnectionInfo;
        connectionInfo.copy( &outConnectionInfo );
        return handler->done(
            reqID,
            errorCode,
            AbstractECConnectionPtr(new RemoteEC2Connection( &m_remoteQueryProcessor, m_resCtx, outConnectionInfo )) );
    }

    ErrorCode Ec2DirectConnectionFactory::doRemoteConnectionSync(
        const LoginInfo& loginInfo,
        ConnectionInfo* const connectionInfo )
    {
        //TODO/IMPL
        connectionInfo->version = lit(QN_APPLICATION_VERSION);
        connectionInfo->brand = lit(QN_CUSTOMIZATION_NAME);
        connectionInfo->ecsGuid = lit( "ECS_HUID" );

        return ErrorCode::ok;
    }
}
