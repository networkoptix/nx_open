/**********************************************************
* 21 jan 2014
* a.kolesnikov
***********************************************************/

#include "connection_factory.h"

#include <functional>

#include <QtCore/QMutexLocker>

#include <network/authenticate_helper.h>
#include <network/universal_tcp_listener.h>
#include <utils/common/concurrent.h>
#include <utils/network/simple_http_client.h>

#include "compatibility/old_ec_connection.h"
#include "ec2_connection.h"
#include "ec2_thread_pool.h"
#include "remote_ec_connection.h"
#include "rest/ec2_base_query_http_handler.h"
#include "rest/ec2_update_http_handler.h"
#include "rest/server/rest_connection_processor.h"
#include "transaction/transaction.h"
#include "transaction/transaction_message_bus.h"
#include "http/ec2_transaction_tcp_listener.h"
#include "version.h"


namespace ec2
{
    Ec2DirectConnectionFactory::Ec2DirectConnectionFactory( Qn::PeerType peerType )
    :
        m_dbManager( peerType == Qn::PT_Server ? new QnDbManager() : nullptr ),   //dbmanager is initialized by direct connection
        m_transactionMessageBus( new ec2::QnTransactionMessageBus() ),
        m_timeSynchronizationManager( new TimeSynchronizationManager(peerType) ),
        m_terminated( false ),
        m_runningRequests( 0 ),
        m_sslEnabled( false )
    {
        srand( ::time(NULL) );

        //registering ec2 types with Qt meta types system
        qRegisterMetaType<QnTransactionTransportHeader>( "QnTransactionTransportHeader" ); // TODO: #Elric #EC2 register in a proper place!
    }

    Ec2DirectConnectionFactory::~Ec2DirectConnectionFactory()
    {
        pleaseStop();
        join();
    }

    void Ec2DirectConnectionFactory::pleaseStop()
    {
        QMutexLocker lk( &m_mutex );
        m_terminated = true;
    }

    void Ec2DirectConnectionFactory::join()
    {
        QMutexLocker lk( &m_mutex );
        while( m_runningRequests > 0 )
        {
            lk.unlock();
            QThread::msleep( 1000 );
            lk.relock();
        }
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

        m_sslEnabled = universalTcpListener->isSslEnabled();
    }

    void Ec2DirectConnectionFactory::registerRestHandlers( QnRestProcessorPool* const restProcessorPool )
    {
        using namespace std::placeholders;

        //AbstractResourceManager::getResourceTypes
        registerGetFuncHandler<std::nullptr_t, ApiResourceTypeDataList>( restProcessorPool, ApiCommand::getResourceTypes );
        //AbstractResourceManager::getResource
        //registerGetFuncHandler<std::nullptr_t, ApiResourceData>( restProcessorPool, ApiCommand::getResource );
        //AbstractResourceManager::setResourceStatus
        registerUpdateFuncHandler<ApiSetResourceStatusData>( restProcessorPool, ApiCommand::setResourceStatus );
        //AbstractResourceManager::getKvPairs
        registerGetFuncHandler<QUuid, ApiResourceParamsData>( restProcessorPool, ApiCommand::getResourceParams );
        //AbstractResourceManager::save
        registerUpdateFuncHandler<ApiResourceParamsData>( restProcessorPool, ApiCommand::setResourceParams );
        //AbstractResourceManager::save
        registerUpdateFuncHandler<ApiResourceData>( restProcessorPool, ApiCommand::saveResource );
        //AbstractResourceManager::remove
        registerUpdateFuncHandler<ApiIdData>( restProcessorPool, ApiCommand::removeResource );


        //AbstractMediaServerManager::getServers
        registerGetFuncHandler<std::nullptr_t, ApiMediaServerDataList>( restProcessorPool, ApiCommand::getMediaServers );
        //AbstractMediaServerManager::save
        registerUpdateFuncHandler<ApiMediaServerData>( restProcessorPool, ApiCommand::saveMediaServer );
        //AbstractMediaServerManager::remove
        registerUpdateFuncHandler<ApiIdData>( restProcessorPool, ApiCommand::removeMediaServer );


        //AbstractCameraManager::addCamera
        registerUpdateFuncHandler<ApiCameraData>( restProcessorPool, ApiCommand::saveCamera );
        //AbstractCameraManager::save
        registerUpdateFuncHandler<ApiCameraDataList>( restProcessorPool, ApiCommand::saveCameras );
        //AbstractCameraManager::getCameras
        registerGetFuncHandler<QUuid, ApiCameraDataList>( restProcessorPool, ApiCommand::getCameras );
        //AbstractCameraManager::addCameraHistoryItem
        registerUpdateFuncHandler<ApiCameraServerItemData>( restProcessorPool, ApiCommand::addCameraHistoryItem );
        //AbstractCameraManager::getCameraHistoryItems
        registerGetFuncHandler<std::nullptr_t, ApiCameraServerItemDataList>( restProcessorPool, ApiCommand::getCameraHistoryItems );
        //AbstractCameraManager::getBookmarkTags
        registerGetFuncHandler<std::nullptr_t, ApiCameraBookmarkTagDataList>( restProcessorPool, ApiCommand::getCameraBookmarkTags );

        //AbstractCameraManager::getBookmarkTags

        registerUpdateFuncHandler<ApiLicenseDataList>( restProcessorPool, ApiCommand::addLicenses );
        registerUpdateFuncHandler<ApiLicenseData>( restProcessorPool, ApiCommand::removeLicense );


        //AbstractBusinessEventManager::getBusinessRules
        registerGetFuncHandler<std::nullptr_t, ApiBusinessRuleDataList>( restProcessorPool, ApiCommand::getBusinessRules );
        //AbstractBusinessEventManager::save
        registerUpdateFuncHandler<ApiBusinessRuleData>( restProcessorPool, ApiCommand::saveBusinessRule );
        //AbstractBusinessEventManager::deleteRule
        registerUpdateFuncHandler<ApiIdData>( restProcessorPool, ApiCommand::removeBusinessRule );

        registerUpdateFuncHandler<ApiResetBusinessRuleData>( restProcessorPool, ApiCommand::resetBusinessRules );
        registerUpdateFuncHandler<ApiBusinessActionData>( restProcessorPool, ApiCommand::broadcastBusinessAction );
        registerUpdateFuncHandler<ApiBusinessActionData>( restProcessorPool, ApiCommand::execBusinessAction );


        //AbstractUserManager::getUsers
        registerGetFuncHandler<std::nullptr_t, ApiUserDataList>( restProcessorPool, ApiCommand::getUsers );
        //AbstractUserManager::save
        registerUpdateFuncHandler<ApiUserData>( restProcessorPool, ApiCommand::saveUser );
        //AbstractUserManager::remove
        registerUpdateFuncHandler<ApiIdData>( restProcessorPool, ApiCommand::removeUser );

        //AbstractVideowallManager::getVideowalls
        registerGetFuncHandler<std::nullptr_t, ApiVideowallDataList>( restProcessorPool, ApiCommand::getVideowalls );
        //AbstractVideowallManager::save
        registerUpdateFuncHandler<ApiVideowallData>( restProcessorPool, ApiCommand::saveVideowall );
        //AbstractVideowallManager::remove
        registerUpdateFuncHandler<ApiIdData>( restProcessorPool, ApiCommand::removeVideowall );
        registerUpdateFuncHandler<ApiVideowallControlMessageData>( restProcessorPool, ApiCommand::videowallControl );

        //AbstractLayoutManager::getLayouts
        registerGetFuncHandler<std::nullptr_t, ApiLayoutDataList>( restProcessorPool, ApiCommand::getLayouts );
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
        registerUpdateFuncHandler<ApiUpdateInstallData>( restProcessorPool, ApiCommand::installUpdate );

        //AbstractMiscManager::moduleInfo
        registerUpdateFuncHandler<ApiModuleData>(restProcessorPool, ApiCommand::moduleInfo);
        //AbstractMiscManager::moduleInfoList
        registerUpdateFuncHandler<ApiModuleDataList>(restProcessorPool, ApiCommand::moduleInfoList);

        //AbstractMiscManager::discoverPeer
        registerUpdateFuncHandler<ApiDiscoverPeerData>(restProcessorPool, ApiCommand::discoverPeer);
        //AbstractMiscManager::addDiscoveryInformation
        registerUpdateFuncHandler<ApiDiscoveryDataList>(restProcessorPool, ApiCommand::addDiscoveryInformation);
        //AbstractMiscManager::removeDiscoveryInformation
        registerUpdateFuncHandler<ApiDiscoveryDataList>(restProcessorPool, ApiCommand::removeDiscoveryInformation);
        //AbstractMiscManager::changeSystemName
        registerUpdateFuncHandler<ApiSystemNameData>(restProcessorPool, ApiCommand::changeSystemName);

        //AbstractMiscManager::addConnection
        registerUpdateFuncHandler<ApiConnectionData>(restProcessorPool, ApiCommand::addConnection);
        //AbstractMiscManager::removeConnection
        registerUpdateFuncHandler<ApiConnectionData>(restProcessorPool, ApiCommand::removeConnection);
        //AbstractMiscManager::availableConnections
        registerUpdateFuncHandler<ApiConnectionDataList>(restProcessorPool, ApiCommand::availableConnections);

       //AbstractECConnection
        registerUpdateFuncHandler<ApiDatabaseDumpData>( restProcessorPool, ApiCommand::resotreDatabase );

        //AbstractECConnection
        registerGetFuncHandler<std::nullptr_t, ApiTimeData>( restProcessorPool, ApiCommand::getCurrentTime );
        registerUpdateFuncHandler<ApiIdData>(
            restProcessorPool,
            ApiCommand::forcePrimaryTimeServer,
            std::bind( &TimeSynchronizationManager::primaryTimeServerChanged, m_timeSynchronizationManager.get(), _1 ) );


        registerGetFuncHandler<std::nullptr_t, ApiFullInfoData>( restProcessorPool, ApiCommand::getFullInfo );
        registerGetFuncHandler<std::nullptr_t, ApiLicenseDataList>( restProcessorPool, ApiCommand::getLicenses );

        registerGetFuncHandler<std::nullptr_t, ApiDatabaseDumpData>( restProcessorPool, ApiCommand::dumpDatabase );

        //AbstractECConnectionFactory
        registerFunctorHandler<ApiLoginData, QnConnectionInfo>( restProcessorPool, ApiCommand::connect,
            std::bind( &Ec2DirectConnectionFactory::fillConnectionInfo, this, _1, _2 ) );
        registerFunctorHandler<ApiLoginData, QnConnectionInfo>( restProcessorPool, ApiCommand::testConnection,
            std::bind( &Ec2DirectConnectionFactory::fillConnectionInfo, this, _1, _2 ) );
    }

    void Ec2DirectConnectionFactory::setContext( const ResourceContext& resCtx )
    {
        m_resCtx = resCtx;
    }

    int Ec2DirectConnectionFactory::establishDirectConnection( const QUrl& url, impl::ConnectHandlerPtr handler )
    {
        const int reqID = generateRequestID();

        ApiLoginData loginInfo;
        QnConnectionInfo connectionInfo;
        fillConnectionInfo( loginInfo, &connectionInfo );   //todo: #ak not appropriate here
        connectionInfo.ecUrl = url;
        {
            QMutexLocker lk( &m_mutex );
            if( !m_directConnection ) {
                m_directConnection.reset( new Ec2DirectConnection( &m_serverQueryProcessor, m_resCtx, connectionInfo, url ) );
            }
        }
        QnScopedThreadRollback ensureFreeThread( 1, Ec2ThreadPool::instance() );
        QnConcurrent::run( Ec2ThreadPool::instance(), std::bind( &impl::ConnectHandler::done, handler, reqID, ec2::ErrorCode::ok, m_directConnection ) );
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

        ApiLoginData loginInfo;
        loginInfo.login = addr.userName();
        loginInfo.passwordHash = QnAuthHelper::createUserPasswordDigest( loginInfo.login, addr.password() );
        {
            QMutexLocker lk( &m_mutex );
            if( m_terminated )
                return INVALID_REQ_ID;
            ++m_runningRequests;
        }
#if 1
        auto func = [this, reqID, addr, handler]( ErrorCode errorCode, const QnConnectionInfo& connectionInfo ) {
            remoteConnectionFinished(reqID, errorCode, connectionInfo, addr, handler); };
        m_remoteQueryProcessor.processQueryAsync<ApiLoginData, QnConnectionInfo>(
            addr, ApiCommand::connect, loginInfo, func );
#else
        //TODO: #ak following does not compile due to msvc2012 restriction: no more than 6 arguments to std::bind
        using namespace std::placeholders;
        m_remoteQueryProcessor.processQueryAsync<ApiLoginData, QnConnectionInfo>(
            addr, ApiCommand::connect, loginInfo, std::bind(&Ec2DirectConnectionFactory::remoteConnectionFinished, this, reqID, _1, _2, addr, handler) );
#endif
        return reqID;
    }

    const char oldEcConnectPath[] = "/api/connect/?format=pb&guid&ping=1";

    static bool parseOldECConnectionInfo(const QByteArray& oldECConnectResponse, QnConnectionInfo* const connectionInfo)
    {
        static const char PROTOBUF_FIELD_TYPE_STRING = 0x0a;

        if( oldECConnectResponse.isEmpty() )
            return false;

        const char* data = oldECConnectResponse.data();
        const char* dataEnd = oldECConnectResponse.data() + oldECConnectResponse.size();
        if( data + 2 >= dataEnd )
            return false;
        if( *data != PROTOBUF_FIELD_TYPE_STRING )
            return false;
        ++data;
        const int fieldLen = *data;
        ++data;
        if( data + fieldLen >= dataEnd )
            return false;
        connectionInfo->version = QnSoftwareVersion(QByteArray::fromRawData(data, fieldLen));
        return true;
    }

    template<class Handler>
    void Ec2DirectConnectionFactory::connectToOldEC( const QUrl& ecURL, Handler completionFunc )
    {
        QAuthenticator auth;
        auth.setUser(ecURL.userName());
        auth.setPassword(ecURL.password());
        CLSimpleHTTPClient simpleHttpClient(ecURL, 3000, auth);
        const CLHttpStatus statusCode = simpleHttpClient.doGET(QByteArray::fromRawData(oldEcConnectPath, sizeof(oldEcConnectPath)));
        switch( statusCode )
        {
            case CL_HTTP_SUCCESS:
            {
                //reading mesasge body
                QByteArray oldECResponse;
                simpleHttpClient.readAll(oldECResponse);
                QnConnectionInfo oldECConnectionInfo;
                oldECConnectionInfo.ecUrl = ecURL;
                if( parseOldECConnectionInfo(oldECResponse, &oldECConnectionInfo) )
                    completionFunc(ErrorCode::ok, oldECConnectionInfo);
                else
                    completionFunc(ErrorCode::badResponse, oldECConnectionInfo);
                break;
            }

            case CL_HTTP_AUTH_REQUIRED:
                completionFunc(ErrorCode::unauthorized, QnConnectionInfo());
                break;

            default:
                completionFunc(ErrorCode::ioError, QnConnectionInfo());
                break;
        }

        QMutexLocker lk( &m_mutex );
        --m_runningRequests;
    }

    void Ec2DirectConnectionFactory::remoteConnectionFinished(
        int reqID,
        ErrorCode errorCode,
        const QnConnectionInfo& connectionInfo,
        const QUrl& ecURL,
        impl::ConnectHandlerPtr handler)
    {
        if( errorCode != ErrorCode::ok )
        {
            //checking for old EC
            QnScopedThreadRollback ensureFreeThread(1, Ec2ThreadPool::instance());
            QnConcurrent::run(
                Ec2ThreadPool::instance(),
                [this, ecURL, handler, reqID]() {
                    using namespace std::placeholders;
                    return connectToOldEC(
                        ecURL,
                        [reqID, handler](ErrorCode errorCode, const QnConnectionInfo& oldECConnectionInfo) {
                            handler->done(reqID, errorCode, std::make_shared<OldEcConnection>(oldECConnectionInfo));
                        }
                    );
                }
            );
            return;
        }
        QnConnectionInfo connectionInfoCopy(connectionInfo);
        connectionInfoCopy.ecUrl = ecURL;
        connectionInfoCopy.ecUrl.setScheme( connectionInfoCopy.allowSslConnections ? lit("https") : lit("http") );

        AbstractECConnectionPtr connection(new RemoteEC2Connection(
            std::make_shared<FixedUrlClientQueryProcessor>(&m_remoteQueryProcessor, connectionInfoCopy.ecUrl),
            m_resCtx,
            connectionInfoCopy));
        handler->done(
            reqID,
            errorCode,
            connection);

        QMutexLocker lk( &m_mutex );
        --m_runningRequests;
    }

    void Ec2DirectConnectionFactory::remoteTestConnectionFinished(
        int reqID,
        ErrorCode errorCode,
        const QnConnectionInfo& connectionInfo,
        const QUrl& ecURL,
        impl::TestConnectionHandlerPtr handler )
    {
        if( errorCode == ErrorCode::ok )
        {
            handler->done( reqID, errorCode, connectionInfo );
            QMutexLocker lk( &m_mutex );
            --m_runningRequests;
            return;
        }

        //checking for old EC
        QnScopedThreadRollback ensureFreeThread(1, Ec2ThreadPool::instance());
        QnConcurrent::run(
            Ec2ThreadPool::instance(),
            [this, ecURL, handler, reqID]() {
                using namespace std::placeholders;
                connectToOldEC(ecURL, std::bind(&impl::TestConnectionHandler::done, handler, reqID, _1, _2));
            }
        );
    }

    ErrorCode Ec2DirectConnectionFactory::fillConnectionInfo(
        const ApiLoginData& /*loginInfo*/,
        QnConnectionInfo* const connectionInfo )
    {
        connectionInfo->version = qnCommon->engineVersion();
        connectionInfo->brand = isCompatibilityMode() ? QString() : lit(QN_PRODUCT_NAME_SHORT);
        connectionInfo->systemName = qnCommon->localSystemName();
        connectionInfo->ecsGuid = qnCommon->moduleGUID().toString();
        connectionInfo->systemName = qnCommon->localSystemName();
        connectionInfo->box = lit(QN_ARM_BOX);
        connectionInfo->allowSslConnections = m_sslEnabled;
        return ErrorCode::ok;
    }

    int Ec2DirectConnectionFactory::testDirectConnection( const QUrl& /*addr*/, impl::TestConnectionHandlerPtr handler )
    {
        const int reqID = generateRequestID();
        QnConnectionInfo connectionInfo;
        fillConnectionInfo( ApiLoginData(), &connectionInfo );
        QnScopedThreadRollback ensureFreeThread( 1, Ec2ThreadPool::instance() );
        QnConcurrent::run( Ec2ThreadPool::instance(), std::bind( &impl::TestConnectionHandler::done, handler, reqID, ec2::ErrorCode::ok, connectionInfo ) );
        return reqID;
    }

    int Ec2DirectConnectionFactory::testRemoteConnection( const QUrl& addr, impl::TestConnectionHandlerPtr handler )
    {
        const int reqID = generateRequestID();

        {
            QMutexLocker lk( &m_mutex );
            if( m_terminated )
                return INVALID_REQ_ID;
            ++m_runningRequests;
        }

        ApiLoginData loginInfo;
        loginInfo.login = addr.userName();
        loginInfo.passwordHash = QnAuthHelper::createUserPasswordDigest( loginInfo.login, addr.password() );
        auto func = [this, reqID, addr, handler]( ErrorCode errorCode, const QnConnectionInfo& connectionInfo ) {
            remoteTestConnectionFinished(reqID, errorCode, connectionInfo, addr, handler); };
        m_remoteQueryProcessor.processQueryAsync<ApiLoginData, QnConnectionInfo>(
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

    template<class InputDataType, class CustomActionType>
    void Ec2DirectConnectionFactory::registerUpdateFuncHandler( QnRestProcessorPool* const restProcessorPool, ApiCommand::Value cmd, CustomActionType customAction )
    {
        restProcessorPool->registerHandler(
            lit("ec2/%1").arg(ApiCommand::toString(cmd)),
            new UpdateHttpHandler<InputDataType>(m_directConnection, customAction) );
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
