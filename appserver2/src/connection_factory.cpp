/**********************************************************
* 21 jan 2014
* a.kolesnikov
***********************************************************/

#include "connection_factory.h"

#include <functional>

#include <QtConcurrent>
#include <QtCore/QMutexLocker>

#include "ec2_connection.h"
#include "remote_ec_connection.h"
#include "rest/ec2_base_query_http_handler.h"
#include "rest/ec2_query_http_handler.h"
#include "rest/ec2_update_http_handler.h"
#include "transaction/transaction.h"
#include "http/ec2_transaction_tcp_listener.h"
#include "version.h"


namespace ec2
{
    Ec2DirectConnectionFactory::Ec2DirectConnectionFactory()
    {
        srand( ::time(NULL) );

        //registering ec2 types with Qt meta types system
        qRegisterMetaType<ErrorCode>();
        qRegisterMetaType<AbstractECConnectionPtr>();

        ec2::QnTransactionMessageBus::initStaticInstance(new ec2::QnTransactionMessageBus());
    }

    Ec2DirectConnectionFactory::~Ec2DirectConnectionFactory()
    {
        delete ec2::QnTransactionMessageBus::instance();
        ec2::QnTransactionMessageBus::initStaticInstance(0);
    }

    //!Implementation of AbstractECConnectionFactory::testConnectionAsync
    int Ec2DirectConnectionFactory::testConnectionAsync( const QUrl& addr, impl::TestConnectionHandlerPtr handler )
    {
        if( addr.isEmpty() )
            return testDirectConnection( addr, handler );
        else
            return testRemoteConnection( addr, handler );
    }

    //!Implementation of AbstractECConnectionFactory::connectAsync
    int Ec2DirectConnectionFactory::connectAsync( const QUrl& addr, impl::ConnectHandlerPtr handler )
    {
        if( addr.isEmpty() )
            return establishDirectConnection( handler );
        else
            return establishConnectionToRemoteServer( addr, handler );
    }

    void Ec2DirectConnectionFactory::registerTransactionListener( QnUniversalTcpListener* universalTcpListener )
    {
        universalTcpListener->addHandler<QnTransactionTcpProcessor>("HTTP", "ec2/events");
    }

    void Ec2DirectConnectionFactory::registerRestHandlers( QnRestProcessorPool* const restProcessorPool )
    {
        using namespace std::placeholders;

        //AbstractResourceManager::getResourceTypes
        registerGetFuncHandler<nullptr_t, ApiResourceTypeList>( restProcessorPool, ApiCommand::getResourceTypes );
        //AbstractResourceManager::getResources
        registerGetFuncHandler<nullptr_t, ApiResourceDataList>( restProcessorPool, ApiCommand::getResources );
        //AbstractResourceManager::getResource
        //registerGetFuncHandler<nullptr_t, ApiResourceData>( restProcessorPool, ApiCommand::getResource );
        //AbstractResourceManager::setResourceStatus
        registerUpdateFuncHandler<ApiSetResourceStatusData>( restProcessorPool, ApiCommand::setResourceStatus );
        //AbstractResourceManager::getKvPairs
        registerGetFuncHandler<QnId, ApiResourceParams>( restProcessorPool, ApiCommand::getResourceParams );
        //AbstractResourceManager::save
        registerUpdateFuncHandler<ApiResourceParams>( restProcessorPool, ApiCommand::setResourceParams );
        //AbstractResourceManager::save
        registerUpdateFuncHandler<ApiResourceData>( restProcessorPool, ApiCommand::saveResource );
        //AbstractResourceManager::remove
        registerUpdateFuncHandler<ApiIdData>( restProcessorPool, ApiCommand::removeResource );


        //AbstractMediaServerManager::getServers
        registerGetFuncHandler<nullptr_t, ApiMediaServerDataList>( restProcessorPool, ApiCommand::getMediaServerList );
        //AbstractMediaServerManager::save
        registerUpdateFuncHandler<ApiMediaServerData>( restProcessorPool, ApiCommand::addMediaServer );
        registerUpdateFuncHandler<ApiMediaServerData>( restProcessorPool, ApiCommand::updateMediaServer );
        //AbstractMediaServerManager::remove
        registerUpdateFuncHandler<ApiIdData>( restProcessorPool, ApiCommand::removeMediaServer );


        //AbstractCameraManager::addCamera
        registerUpdateFuncHandler<ApiCameraData>( restProcessorPool, ApiCommand::addCamera );
        //AbstractCameraManager::getCameras
        registerGetFuncHandler<QnId, ApiCameraDataList>( restProcessorPool, ApiCommand::getCameras );
        //AbstractCameraManager::addCameraHistoryItem
        registerUpdateFuncHandler<ApiCameraServerItemData>( restProcessorPool, ApiCommand::addCameraHistoryItem );
        //AbstractCameraManager::getCameraHistoryList
        registerGetFuncHandler<nullptr_t, ApiCameraServerItemDataList>( restProcessorPool, ApiCommand::getCameraHistoryList );


        //TODO AbstractLicenseManager


        //AbstractBusinessEventManager::getBusinessRules
        registerGetFuncHandler<nullptr_t, ApiBusinessRuleDataList>( restProcessorPool, ApiCommand::getBusinessRuleList );
        //AbstractBusinessEventManager::save
        registerUpdateFuncHandler<ApiBusinessRuleData>( restProcessorPool, ApiCommand::addBusinessRule );
        registerUpdateFuncHandler<ApiBusinessRuleData>( restProcessorPool, ApiCommand::updateBusinessRule );
        //AbstractBusinessEventManager::deleteRule
        registerUpdateFuncHandler<ApiIdData>( restProcessorPool, ApiCommand::removeBusinessRule );
        //TODO AbstractBusinessEventManager::testEmailSettings
        //TODO AbstractBusinessEventManager::sendEmail
        //TODO AbstractBusinessEventManager::broadcastBusinessAction
        //TODO AbstractBusinessEventManager::resetBusinessRules


        //AbstractUserManager::getUsers
        registerGetFuncHandler<nullptr_t, ApiUserDataList>( restProcessorPool, ApiCommand::getUserList );
        //AbstractUserManager::save
        registerUpdateFuncHandler<ApiUserData>( restProcessorPool, ApiCommand::addUser );
        //AbstractUserManager::save
        registerUpdateFuncHandler<ApiUserData>( restProcessorPool, ApiCommand::updateUser );
        //AbstractUserManager::remove
        registerUpdateFuncHandler<ApiIdData>( restProcessorPool, ApiCommand::removeUser );


        //AbstractLayoutManager::getLayouts
        registerGetFuncHandler<nullptr_t, ApiLayoutDataList>( restProcessorPool, ApiCommand::getLayoutList );
        //AbstractLayoutManager::save
        registerUpdateFuncHandler<ApiLayoutDataList>( restProcessorPool, ApiCommand::addOrUpdateLayouts );
        //AbstractLayoutManager::remove
        registerUpdateFuncHandler<ApiIdData>( restProcessorPool, ApiCommand::removeLayout );


        //AbstractStoredFileManager::listDirectory
        registerGetFuncHandler<ApiStoredFilePath, ApiStoredDirContents>( restProcessorPool, ApiCommand::listDirectory );
        //AbstractStoredFileManager::getStoredFile
        registerGetFuncHandler<ApiStoredFilePath, ApiStoredFileData>( restProcessorPool, ApiCommand::getStoredFile );
        //AbstractStoredFileManager::addStoredFile
        registerUpdateFuncHandler<ApiStoredFileData>( restProcessorPool, ApiCommand::addOrUpdateStoredFile );
        //AbstractStoredFileManager::deleteStoredFile
        registerUpdateFuncHandler<ApiIdData>( restProcessorPool, ApiCommand::removeStoredFile );


        //AbstractECConnection
        registerGetFuncHandler<nullptr_t, qint64>( restProcessorPool, ApiCommand::getCurrentTime );


        //AbstractECConnectionFactory
        registerFunctorHandler<LoginInfo, QnConnectionInfo>( restProcessorPool, ApiCommand::connect,
            std::bind( &Ec2DirectConnectionFactory::fillConnectionInfo, this, _1, _2 ) );
        registerFunctorHandler<LoginInfo, QnConnectionInfo>( restProcessorPool, ApiCommand::testConnection,
            std::bind( &Ec2DirectConnectionFactory::fillConnectionInfo, this, _1, _2 ) );
    }

    void Ec2DirectConnectionFactory::setContext( const ResourceContext& resCtx )
    {
        m_resCtx = resCtx;
    }

    int Ec2DirectConnectionFactory::establishDirectConnection( impl::ConnectHandlerPtr handler )
    {
        const int reqID = generateRequestID();

        LoginInfo loginInfo;
        QnConnectionInfo connectionInfo;
        fillConnectionInfo( loginInfo, &connectionInfo );   //todo: #ak not appropriate here
        {
            QMutexLocker lk( &m_mutex );
            if( !m_directConnection )
                m_directConnection.reset( new Ec2DirectConnection( &m_serverQueryProcessor, m_resCtx, connectionInfo ) );
        }
        QtConcurrent::run( std::bind( std::mem_fn( &impl::ConnectHandler::done ), handler, reqID, ec2::ErrorCode::ok, m_directConnection ) );
        return reqID;
    }

    int Ec2DirectConnectionFactory::establishConnectionToRemoteServer( const QUrl& addr, impl::ConnectHandlerPtr handler )
    {
        const int reqID = generateRequestID();

        ////TODO: #ak return existing connection, if one
        //{
        //    QMutexLocker lk( &m_mutex );
        //    auto it = m_urlToConnection.find( addr );
        //    if( it != m_urlToConnection.end() )
        //        AbstractECConnectionPtr connection = it->second.second;
        //}

        LoginInfo loginInfo;
#if 1
        auto func = [this, reqID, addr, handler]( ErrorCode errorCode, const QnConnectionInfo& connectionInfo ) {
            remoteConnectionFinished(reqID, errorCode, connectionInfo, addr, handler); };
        m_remoteQueryProcessor.processQueryAsync<LoginInfo, QnConnectionInfo>(
            addr, ApiCommand::connect, loginInfo, func );
#else
        //TODO: #ak investigate, what's wrong with following code
        using namespace std::placeholders;
        m_remoteQueryProcessor.processQueryAsync<LoginInfo, QnConnectionInfo>(
            ApiCommand::connect, loginInfo, std::bind(&Ec2DirectConnectionFactory::remoteConnectionFinished, this, _1, _2) );
#endif
        return reqID;
    }

    void Ec2DirectConnectionFactory::remoteConnectionFinished(
        int reqID,
        ErrorCode errorCode,
        const QnConnectionInfo& connectionInfo,
        const QUrl& ecURL,
        impl::ConnectHandlerPtr handler )
    {
        if( errorCode != ErrorCode::ok )
            return handler->done( reqID, errorCode, AbstractECConnectionPtr() );
        QnConnectionInfo connectionInfoCopy( connectionInfo );
        connectionInfoCopy.ecUrl = ecURL;
        return handler->done(
            reqID,
            errorCode,
            std::make_shared<RemoteEC2Connection>(
                std::make_shared<FixedUrlClientQueryProcessor>(&m_remoteQueryProcessor, ecURL),
                m_resCtx,
                connectionInfoCopy ) );
    }

    void Ec2DirectConnectionFactory::remoteTestConnectionFinished(
        int reqID,
        ErrorCode errorCode,
        const QnConnectionInfo& connectionInfo,
        const QUrl& /*ecURL*/,
        impl::TestConnectionHandlerPtr handler )
    {
        return handler->done( reqID, errorCode, connectionInfo );
    }

    ErrorCode Ec2DirectConnectionFactory::fillConnectionInfo(
        const LoginInfo& loginInfo,
        QnConnectionInfo* const connectionInfo )
    {
        //TODO/IMPL
        connectionInfo->version = QnSoftwareVersion(lit(QN_APPLICATION_VERSION));
        connectionInfo->brand = lit(QN_PRODUCT_NAME_SHORT);
        connectionInfo->ecsGuid = lit( "ECS_HUID" );

        return ErrorCode::ok;
    }

    int Ec2DirectConnectionFactory::testDirectConnection( const QUrl& addr, impl::TestConnectionHandlerPtr handler )
    {
        const int reqID = generateRequestID();
        QnConnectionInfo connectionInfo;
        fillConnectionInfo( LoginInfo(), &connectionInfo );
        QtConcurrent::run( std::bind( &impl::TestConnectionHandler::done, handler, reqID, ec2::ErrorCode::ok, connectionInfo ) );
        return reqID;
    }

    int Ec2DirectConnectionFactory::testRemoteConnection( const QUrl& addr, impl::TestConnectionHandlerPtr handler )
    {
        const int reqID = generateRequestID();

        LoginInfo loginInfo;
        auto func = [this, reqID, addr, handler]( ErrorCode errorCode, const QnConnectionInfo& connectionInfo ) {
            remoteTestConnectionFinished(reqID, errorCode, connectionInfo, addr, handler); };
        m_remoteQueryProcessor.processQueryAsync<LoginInfo, QnConnectionInfo>(
            addr, ApiCommand::testConnection, loginInfo, func );
        return reqID;
    }

    template<class InputDataType>
    void Ec2DirectConnectionFactory::registerUpdateFuncHandler( QnRestProcessorPool* const restProcessorPool, ApiCommand::Value cmd )
    {
        restProcessorPool->registerHandler(
            lit("ec2/%1").arg(ApiCommand::toString(cmd)),
            new UpdateHttpHandler<InputDataType>(&m_serverQueryProcessor) );
    }

    template<class InputDataType, class OutputDataType>
    void Ec2DirectConnectionFactory::registerGetFuncHandler( QnRestProcessorPool* const restProcessorPool, ApiCommand::Value cmd )
    {
        restProcessorPool->registerHandler(
            lit("ec2/%1").arg(ApiCommand::toString(cmd)),
            new QueryHttpHandler2<InputDataType, OutputDataType>(cmd, &m_serverQueryProcessor) );
    }

    template<class InputType, class OutputType, class HandlerType>
    void Ec2DirectConnectionFactory::registerFunctorHandler(
        QnRestProcessorPool* const restProcessorPool,
        ApiCommand::Value cmd,
        HandlerType handler )
    {
        restProcessorPool->registerHandler(
            lit("ec2/%1").arg(ApiCommand::toString(cmd)),
            new FlexibleQueryHttpHandler<InputType, OutputType, decltype(handler)>(cmd, handler) );
    }
}
