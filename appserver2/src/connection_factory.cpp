/**********************************************************
* 21 jan 2014
* a.kolesnikov
***********************************************************/

#include "connection_factory.h"

#include <functional>

#include <QtCore/QMutexLocker>
#include <QtConcurrent>

#include <network/universal_tcp_listener.h>

#include "ec2_connection.h"
#include "remote_ec_connection.h"
#include "rest/ec2_base_query_http_handler.h"
#include "rest/ec2_query_http_handler.h"
#include "rest/ec2_update_http_handler.h"
#include "rest/server/rest_connection_processor.h"
#include "transaction/transaction.h"
#include "transaction/transaction_message_bus.h"
#include "http/ec2_transaction_tcp_listener.h"
#include "version.h"


namespace ec2
{
    Ec2DirectConnectionFactory::Ec2DirectConnectionFactory()
    {
        srand( ::time(NULL) );

        //registering ec2 types with Qt meta types system
        qRegisterMetaType<ErrorCode>( "ErrorCode" );
        qRegisterMetaType<AbstractECConnectionPtr>( "AbstractECConnectionPtr" );
        qRegisterMetaType<QnFullResourceData>( "QnFullResourceData" ); // TODO: #Elric #EC2 register in a proper place!
        qRegisterMetaType<TransactionTransportHeader>( "TransactionTransportHeader" ); // TODO: #Elric #EC2 register in a proper place!
        qRegisterMetaType<ApiServerAliveData>( "ApiServerAliveData" ); // TODO: #Elric #EC2 register in a proper place!

        ec2::QnTransactionMessageBus::initStaticInstance(new ec2::QnTransactionMessageBus());
    }

    Ec2DirectConnectionFactory::~Ec2DirectConnectionFactory()
    {
        m_directConnection.reset();
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
        if( addr.scheme() == "file" )
            return establishDirectConnection(addr, handler );
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
        registerGetFuncHandler<nullptr_t, ApiResourceTypeDataList>( restProcessorPool, ApiCommand::getResourceTypes );
        //AbstractResourceManager::getResource
        //registerGetFuncHandler<nullptr_t, ApiResourceData>( restProcessorPool, ApiCommand::getResource );
        //AbstractResourceManager::setResourceStatus
        registerUpdateFuncHandler<ApiSetResourceStatusData>( restProcessorPool, ApiCommand::setResourceStatus );
        //AbstractResourceManager::setResourceDisabled
        //registerUpdateFuncHandler<ApiSetResourceDisabledData>( restProcessorPool, ApiCommand::setResourceDisabled );
        //AbstractResourceManager::getKvPairs
        registerGetFuncHandler<QnId, ApiResourceParamsData>( restProcessorPool, ApiCommand::getResourceParams );
        //AbstractResourceManager::save
        registerUpdateFuncHandler<ApiResourceParamsData>( restProcessorPool, ApiCommand::setResourceParams );
        //AbstractResourceManager::save
        registerUpdateFuncHandler<ApiResourceData>( restProcessorPool, ApiCommand::saveResource );
        //AbstractResourceManager::remove
        registerUpdateFuncHandler<ApiIdData>( restProcessorPool, ApiCommand::removeResource );


        //AbstractMediaServerManager::getServers
        registerGetFuncHandler<nullptr_t, ApiMediaServerDataList>( restProcessorPool, ApiCommand::getMediaServerList );
        //AbstractMediaServerManager::save
        registerUpdateFuncHandler<ApiMediaServerData>( restProcessorPool, ApiCommand::saveMediaServer );
        //AbstractMediaServerManager::remove
        registerUpdateFuncHandler<ApiIdData>( restProcessorPool, ApiCommand::removeMediaServer );


        //AbstractCameraManager::addCamera
        registerUpdateFuncHandler<ApiCameraData>( restProcessorPool, ApiCommand::saveCamera );
        //AbstractCameraManager::save
        registerUpdateFuncHandler<ApiCameraDataList>( restProcessorPool, ApiCommand::saveCameras );
        //AbstractCameraManager::getCameras
        registerGetFuncHandler<QnId, ApiCameraDataList>( restProcessorPool, ApiCommand::getCameras );
        //AbstractCameraManager::addCameraHistoryItem
        registerUpdateFuncHandler<ApiCameraServerItemData>( restProcessorPool, ApiCommand::addCameraHistoryItem );
        //AbstractCameraManager::getCameraHistoryList
        registerGetFuncHandler<nullptr_t, ApiCameraServerItemDataList>( restProcessorPool, ApiCommand::getCameraHistoryList );
        //AbstractCameraManager::getBookmarkTags
        registerGetFuncHandler<nullptr_t, ApiCameraBookmarkTagDataList>( restProcessorPool, ApiCommand::getCameraBookmarkTags );

        //TODO AbstractLicenseManager
        registerUpdateFuncHandler<ApiLicenseDataList>( restProcessorPool, ApiCommand::addLicenses );


        //AbstractBusinessEventManager::getBusinessRules
        registerGetFuncHandler<nullptr_t, ApiBusinessRuleDataList>( restProcessorPool, ApiCommand::getBusinessRuleList );
        //AbstractBusinessEventManager::save
        registerUpdateFuncHandler<ApiBusinessRuleData>( restProcessorPool, ApiCommand::saveBusinessRule );
        //AbstractBusinessEventManager::deleteRule
        registerUpdateFuncHandler<ApiIdData>( restProcessorPool, ApiCommand::removeBusinessRule );

        registerUpdateFuncHandler<ApiResetBusinessRuleData>( restProcessorPool, ApiCommand::resetBusinessRules );
        registerUpdateFuncHandler<ApiBusinessActionData>( restProcessorPool, ApiCommand::broadcastBusinessAction );
        registerUpdateFuncHandler<ApiBusinessActionData>( restProcessorPool, ApiCommand::execBusinessAction );

        registerUpdateFuncHandler<ApiEmailSettingsData>( restProcessorPool, ApiCommand::testEmailSettings );
        registerUpdateFuncHandler<ApiEmailData>( restProcessorPool, ApiCommand::sendEmail );


        //AbstractUserManager::getUsers
        registerGetFuncHandler<nullptr_t, ApiUserDataList>( restProcessorPool, ApiCommand::getUserList );
        //AbstractUserManager::save
        registerUpdateFuncHandler<ApiUserData>( restProcessorPool, ApiCommand::saveUser );
        //AbstractUserManager::remove
        registerUpdateFuncHandler<ApiIdData>( restProcessorPool, ApiCommand::removeUser );

        //AbstractVideowallManager::getVideowalls
        registerGetFuncHandler<nullptr_t, ApiVideowallDataList>( restProcessorPool, ApiCommand::getVideowallList );
        //AbstractVideowallManager::save
        registerUpdateFuncHandler<ApiVideowallData>( restProcessorPool, ApiCommand::saveVideowall );
        //AbstractVideowallManager::remove
        registerUpdateFuncHandler<ApiIdData>( restProcessorPool, ApiCommand::removeVideowall );
        registerUpdateFuncHandler<ApiVideowallControlMessageData>( restProcessorPool, ApiCommand::videowallControl );

        //AbstractLayoutManager::getLayouts
        registerGetFuncHandler<nullptr_t, ApiLayoutDataList>( restProcessorPool, ApiCommand::getLayoutList );
        //AbstractLayoutManager::save
        registerUpdateFuncHandler<ApiLayoutDataList>( restProcessorPool, ApiCommand::saveLayouts );
        //AbstractLayoutManager::remove
        registerUpdateFuncHandler<ApiIdData>( restProcessorPool, ApiCommand::removeLayout );


        //AbstractStoredFileManager::listDirectory
        registerGetFuncHandler<ApiStoredFilePath, ApiStoredDirContents>( restProcessorPool, ApiCommand::listDirectory );
        //AbstractStoredFileManager::getStoredFile
        registerGetFuncHandler<ApiStoredFilePath, ApiStoredFileData>( restProcessorPool, ApiCommand::getStoredFile );
        //AbstractStoredFileManager::addStoredFile
        registerUpdateFuncHandler<ApiStoredFileData>( restProcessorPool, ApiCommand::addStoredFile );
        //AbstractStoredFileManager::updateStoredFile
        registerUpdateFuncHandler<ApiStoredFileData>( restProcessorPool, ApiCommand::updateStoredFile );
        //AbstractStoredFileManager::deleteStoredFile
        registerUpdateFuncHandler<ApiIdData>( restProcessorPool, ApiCommand::removeStoredFile );

        //AbstractUpdatesManager::uploadUpdate
        registerUpdateFuncHandler<ApiUpdateUploadData>( restProcessorPool, ApiCommand::uploadUpdate );
        //AbstractUpdatesManager::uploadUpdateResponce
        registerUpdateFuncHandler<ApiUpdateUploadResponceData>( restProcessorPool, ApiCommand::uploadUpdateResponce );
        //AbstractUpdatesManager::installUpdate
        registerUpdateFuncHandler<QString>( restProcessorPool, ApiCommand::installUpdate );

        //AbstractModuleInformationManager::moduleInfo
        registerUpdateFuncHandler<ApiModuleData>(restProcessorPool, ApiCommand::moduleInfo);

        //ApiResourceParamList
        registerGetFuncHandler<nullptr_t, ApiResourceParamDataList>( restProcessorPool, ApiCommand::getSettings );
        registerUpdateFuncHandler<ApiResourceParamDataList>( restProcessorPool, ApiCommand::saveSettings );

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

    int Ec2DirectConnectionFactory::establishDirectConnection( const QUrl& url, impl::ConnectHandlerPtr handler )
    {
        const int reqID = generateRequestID();

        LoginInfo loginInfo;
        QnConnectionInfo connectionInfo;
        fillConnectionInfo( loginInfo, &connectionInfo );   //todo: #ak not appropriate here
        connectionInfo.ecUrl = url;
        {
            QMutexLocker lk( &m_mutex );
            if( !m_directConnection )
                m_directConnection.reset( new Ec2DirectConnection( &m_serverQueryProcessor, m_resCtx, connectionInfo, url.path() ) );
        }
        QnScopedThreadRollback ensureFreeThread(1);
        QtConcurrent::run( std::bind( &impl::ConnectHandler::done, handler, reqID, ec2::ErrorCode::ok, m_directConnection ) );
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
        const LoginInfo& /*loginInfo*/,
        QnConnectionInfo* const connectionInfo )
    {
        //TODO/IMPL
        connectionInfo->version = QnSoftwareVersion(lit(QN_APPLICATION_VERSION));
        connectionInfo->brand = lit(QN_PRODUCT_NAME_SHORT);
        connectionInfo->ecsGuid = lit( "ECS_HUID" );
        connectionInfo->systemName = qnCommon->localSystemName();

        return ErrorCode::ok;
    }

    int Ec2DirectConnectionFactory::testDirectConnection( const QUrl& /*addr*/, impl::TestConnectionHandlerPtr handler )
    {
        const int reqID = generateRequestID();
        QnConnectionInfo connectionInfo;
        fillConnectionInfo( LoginInfo(), &connectionInfo );
        QnScopedThreadRollback ensureFreeThread(1);
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
            new UpdateHttpHandler<InputDataType>(m_directConnection) );
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
            new FlexibleQueryHttpHandler<InputType, OutputType, HandlerType>(cmd, handler) );
    }
}
