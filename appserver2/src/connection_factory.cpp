/**********************************************************
* 21 jan 2014
* a.kolesnikov
***********************************************************/

#include "connection_factory.h"

#include <functional>

#include <QtCore/QMutexLocker>

#include <network/authenticate_helper.h>
#include <network/universal_tcp_listener.h>
#include <nx_ec/ec_proto_version.h>
#include <utils/common/concurrent.h>
#include <utils/network/simple_http_client.h>

#include "compatibility/old_ec_connection.h"
#include "ec2_connection.h"
#include "ec2_thread_pool.h"
#include "nx_ec/data/api_resource_type_data.h"
#include "nx_ec/data/api_camera_data_ex.h"
#include "remote_ec_connection.h"
#include "rest/ec2_base_query_http_handler.h"
#include "rest/ec2_update_http_handler.h"
#include "rest/server/rest_connection_processor.h"
#include "transaction/transaction.h"
#include "transaction/transaction_message_bus.h"
#include "http/ec2_transaction_tcp_listener.h"
#include "http/http_transaction_receiver.h"
#include <utils/common/app_info.h>
#include "mutex/distributed_mutex_manager.h"


static const char INCOMING_TRANSACTIONS_PATH[] = "ec2/forward_events";

namespace ec2
{
    Ec2DirectConnectionFactory::Ec2DirectConnectionFactory( Qn::PeerType peerType )
    :
        m_dbManager( peerType == Qn::PT_Server ? new QnDbManager() : nullptr ),   //dbmanager is initialized by direct connection
        m_timeSynchronizationManager( new TimeSynchronizationManager(peerType) ),
        m_transactionMessageBus( new ec2::QnTransactionMessageBus(peerType) ),
        m_terminated( false ),
        m_runningRequests( 0 ),
        m_sslEnabled( false )
    {
        m_timeSynchronizationManager->start();  //unfortunately cannot do it in TimeSynchronizationManager 
            //constructor to keep valid object destruction order

        srand( ::time(NULL) );

        //registering ec2 types with Qt meta types system
        qRegisterMetaType<QnTransactionTransportHeader>( "QnTransactionTransportHeader" ); // TODO: #Elric #EC2 register in a proper place!

        ec2::QnDistributedMutexManager::initStaticInstance( new ec2::QnDistributedMutexManager() );

        //m_transactionMessageBus->start();
    }

    Ec2DirectConnectionFactory::~Ec2DirectConnectionFactory()
    {
        pleaseStop();
        join();

        m_timeSynchronizationManager->pleaseStop(); //have to do it before m_transactionMessageBus destruction 
            //since TimeSynchronizationManager uses QnTransactionMessageBus

        ec2::QnDistributedMutexManager::initStaticInstance(0);
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
        QUrl url = addr;
        url.setUserName(url.userName().toLower());

        if (url.isEmpty())
            return testDirectConnection(url, handler);
        else
            return testRemoteConnection(url, handler);
    }

    //!Implementation of AbstractECConnectionFactory::connectAsync
    int Ec2DirectConnectionFactory::connectAsync( const QUrl& addr, impl::ConnectHandlerPtr handler )
    {
        QUrl url = addr;
        url.setUserName(url.userName().toLower());

        if (url.scheme() == "file")
            return establishDirectConnection(url, handler);
        else
            return establishConnectionToRemoteServer(url, handler);
    }

    void Ec2DirectConnectionFactory::registerTransactionListener( QnUniversalTcpListener* universalTcpListener )
    {
        universalTcpListener->addHandler<QnTransactionTcpProcessor>("HTTP", "ec2/events");
        universalTcpListener->addHandler<QnHttpTransactionReceiver>("HTTP", INCOMING_TRANSACTIONS_PATH);

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
        registerUpdateFuncHandler<ApiResourceStatusData>( restProcessorPool, ApiCommand::setResourceStatus );
        //AbstractResourceManager::getKvPairs
        registerGetFuncHandler<QnUuid, ApiResourceParamWithRefDataList>( restProcessorPool, ApiCommand::getResourceParams );
        //AbstractResourceManager::save
        registerUpdateFuncHandler<ApiResourceParamWithRefDataList>( restProcessorPool, ApiCommand::setResourceParams );
        //AbstractResourceManager::remove
        registerUpdateFuncHandler<ApiIdData>( restProcessorPool, ApiCommand::removeResource );


        //AbstractMediaServerManager::getServers
        registerGetFuncHandler<QnUuid, ApiMediaServerDataList>( restProcessorPool, ApiCommand::getMediaServers );
        //AbstractMediaServerManager::save
        registerUpdateFuncHandler<ApiMediaServerData>( restProcessorPool, ApiCommand::saveMediaServer );
        //AbstractCameraManager::saveUserAttributes
        registerUpdateFuncHandler<ApiMediaServerUserAttributesDataList>( restProcessorPool, ApiCommand::saveServerUserAttributesList );
        //AbstractCameraManager::getUserAttributes
        registerGetFuncHandler<QnUuid, ApiMediaServerUserAttributesDataList>( restProcessorPool, ApiCommand::getServerUserAttributes );
        //AbstractMediaServerManager::remove
        registerUpdateFuncHandler<ApiIdData>( restProcessorPool, ApiCommand::removeMediaServer );

        //AbstractMediaServerManager::getServersEx
        registerGetFuncHandler<QnUuid, ApiMediaServerDataExList>( restProcessorPool, ApiCommand::getMediaServersEx );

        registerUpdateFuncHandler<ApiStorageDataList>( restProcessorPool, ApiCommand::saveStorages);
        registerUpdateFuncHandler<ApiStorageData>( restProcessorPool, ApiCommand::saveStorage);
        registerUpdateFuncHandler<ApiIdDataList>( restProcessorPool, ApiCommand::removeStorages);
        registerUpdateFuncHandler<ApiIdData>( restProcessorPool, ApiCommand::removeStorage);
        registerUpdateFuncHandler<ApiIdDataList>( restProcessorPool, ApiCommand::removeResources);

        //AbstractCameraManager::addCamera
        registerUpdateFuncHandler<ApiCameraData>( restProcessorPool, ApiCommand::saveCamera );
        //AbstractCameraManager::save
        registerUpdateFuncHandler<ApiCameraDataList>( restProcessorPool, ApiCommand::saveCameras );
        //AbstractCameraManager::getCameras
        registerGetFuncHandler<QnUuid, ApiCameraDataList>( restProcessorPool, ApiCommand::getCameras );
        //AbstractCameraManager::saveUserAttributes
        registerUpdateFuncHandler<ApiCameraAttributesDataList>( restProcessorPool, ApiCommand::saveCameraUserAttributesList );
        //AbstractCameraManager::getUserAttributes
        registerGetFuncHandler<QnUuid, ApiCameraAttributesDataList>( restProcessorPool, ApiCommand::getCameraUserAttributes );
        //AbstractCameraManager::addCameraHistoryItem
        registerUpdateFuncHandler<ApiCameraServerItemData>( restProcessorPool, ApiCommand::addCameraHistoryItem );
        //AbstractCameraManager::removeCameraHistoryItem
        registerUpdateFuncHandler<ApiCameraServerItemData>( restProcessorPool, ApiCommand::removeCameraHistoryItem );
        //AbstractCameraManager::getCameraHistoryItems
        registerGetFuncHandler<std::nullptr_t, ApiCameraServerItemDataList>( restProcessorPool, ApiCommand::getCameraHistoryItems );
        //AbstractCameraManager::getBookmarkTags
        registerGetFuncHandler<std::nullptr_t, ApiCameraBookmarkTagDataList>( restProcessorPool, ApiCommand::getCameraBookmarkTags );
        //AbstractCameraManager::getCamerasEx
        registerGetFuncHandler<QnUuid, ApiCameraDataExList>( restProcessorPool, ApiCommand::getCamerasEx );

        registerGetFuncHandler<QnUuid, ApiStorageDataList>( restProcessorPool, ApiCommand::getStorages );

        //AbstractCameraManager::getBookmarkTags

        registerUpdateFuncHandler<ApiLicenseDataList>( restProcessorPool, ApiCommand::addLicenses );
        registerUpdateFuncHandler<ApiLicenseData>( restProcessorPool, ApiCommand::removeLicense );


        //AbstractBusinessEventManager::getBusinessRules
        registerGetFuncHandler<std::nullptr_t, ApiBusinessRuleDataList>( restProcessorPool, ApiCommand::getBusinessRules );

        registerGetFuncHandler<std::nullptr_t, ApiTransactionDataList>( restProcessorPool, ApiCommand::getTransactionLog );

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
        registerUpdateFuncHandler<ApiStoredFilePath>( restProcessorPool, ApiCommand::removeStoredFile );

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

        //AbstractDiscoveryManager::discoverPeer
        registerUpdateFuncHandler<ApiDiscoverPeerData>(restProcessorPool, ApiCommand::discoverPeer);
        //AbstractDiscoveryManager::addDiscoveryInformation
        registerUpdateFuncHandler<ApiDiscoveryData>(restProcessorPool, ApiCommand::addDiscoveryInformation);
        //AbstractDiscoveryManager::removeDiscoveryInformation
        registerUpdateFuncHandler<ApiDiscoveryData>(restProcessorPool, ApiCommand::removeDiscoveryInformation);
        //AbstractDiscoveryManager::getDiscoveryData
        registerGetFuncHandler<std::nullptr_t, ApiDiscoveryDataList>(restProcessorPool, ApiCommand::getDiscoveryData);
        //AbstractMiscManager::changeSystemName
        registerUpdateFuncHandler<ApiSystemNameData>(restProcessorPool, ApiCommand::changeSystemName);

       //AbstractECConnection
        registerUpdateFuncHandler<ApiDatabaseDumpData>( restProcessorPool, ApiCommand::restoreDatabase );

        //AbstractTimeManager::getCurrentTimeImpl
        registerGetFuncHandler<std::nullptr_t, ApiTimeData>( restProcessorPool, ApiCommand::getCurrentTime );
        //AbstractTimeManager::forcePrimaryTimeServer
        registerUpdateFuncHandler<ApiIdData>(
            restProcessorPool,
            ApiCommand::forcePrimaryTimeServer,
            std::bind( &TimeSynchronizationManager::primaryTimeServerChanged, m_timeSynchronizationManager.get(), _1 ) );
        //TODO #ak register AbstractTimeManager::getPeerTimeInfoList


        registerGetFuncHandler<std::nullptr_t, ApiFullInfoData>( restProcessorPool, ApiCommand::getFullInfo );
        registerGetFuncHandler<std::nullptr_t, ApiLicenseDataList>( restProcessorPool, ApiCommand::getLicenses );

        registerGetFuncHandler<std::nullptr_t, ApiDatabaseDumpData>( restProcessorPool, ApiCommand::dumpDatabase );
        registerGetFuncHandler<ApiStoredFilePath, qint64>( restProcessorPool, ApiCommand::dumpDatabaseToFile );

        //AbstractECConnectionFactory
        registerFunctorHandler<ApiLoginData, QnConnectionInfo>( restProcessorPool, ApiCommand::connect,
            std::bind( &Ec2DirectConnectionFactory::fillConnectionInfo, this, _1, _2 ) );
        registerFunctorHandler<ApiLoginData, QnConnectionInfo>( restProcessorPool, ApiCommand::testConnection,
            std::bind( &Ec2DirectConnectionFactory::fillConnectionInfo, this, _1, _2 ) );

        registerFunctorHandler<std::nullptr_t, ApiResourceParamDataList>( restProcessorPool, ApiCommand::getSettings,
            std::bind( &Ec2DirectConnectionFactory::getSettings, this, _1, _2 ) );

        //using HTTP processor since HTTP REST does not support HTTP interleaving
        //restProcessorPool->registerHandler(
        //    QLatin1String(INCOMING_TRANSACTIONS_PATH),
        //    new QnRestTransactionReceiver() );
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
        ec2::ErrorCode connectionInitializationResult = ec2::ErrorCode::ok;
        {
            QMutexLocker lk( &m_mutex );
            if( !m_directConnection ) {
                m_directConnection.reset( new Ec2DirectConnection( &m_serverQueryProcessor, m_resCtx, connectionInfo, url ) );
                if( !m_directConnection->initialized() )
                {
                    connectionInitializationResult = ec2::ErrorCode::dbError;
                    m_directConnection.reset();
                }
            }
        }
        QnConcurrent::run( Ec2ThreadPool::instance(), std::bind( &impl::ConnectHandler::done, handler, reqID, connectionInitializationResult, m_directConnection ) );
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
        m_remoteQueryProcessor.processQueryAsync<std::nullptr_t, QnConnectionInfo>(
            addr, ApiCommand::connect, std::nullptr_t(), func );
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
        QUrl httpsEcUrl = ecURL;    //old EC supports only https
        httpsEcUrl.setScheme( lit("https") );

        QAuthenticator auth;
        auth.setUser(httpsEcUrl.userName());
        auth.setPassword(httpsEcUrl.password());
        CLSimpleHTTPClient simpleHttpClient(httpsEcUrl, 3000, auth);
        const CLHttpStatus statusCode = simpleHttpClient.doGET(QByteArray::fromRawData(oldEcConnectPath, sizeof(oldEcConnectPath)));
        switch( statusCode )
        {
            case CL_HTTP_SUCCESS:
            {
                //reading mesasge body
                QByteArray oldECResponse;
                simpleHttpClient.readAll(oldECResponse);
                QnConnectionInfo oldECConnectionInfo;
                oldECConnectionInfo.ecUrl = httpsEcUrl;
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
        //TODO #ak async ssl is working now, make async request to old ec here

        if( (errorCode != ErrorCode::ok) && (errorCode != ErrorCode::unauthorized) )
        {
            //checking for old EC
            QnConcurrent::run(
                Ec2ThreadPool::instance(),
                [this, ecURL, handler, reqID]() {
                    using namespace std::placeholders;
                    return connectToOldEC(
                        ecURL,
                        [reqID, handler](ErrorCode errorCode, const QnConnectionInfo& oldECConnectionInfo) {
                            if( errorCode == ErrorCode::ok && oldECConnectionInfo.version >= SoftwareVersionType( 2, 3, 0 ) )
                                handler->done(  //somehow, connected to 2.3 server with old ec connection. Returning error, since could not connect to ec 2.3 during normal connect
                                    reqID,
                                    ErrorCode::ioError,
                                    AbstractECConnectionPtr() );
                            else
                                handler->done(
                                    reqID,
                                    errorCode,
                                    errorCode == ErrorCode::ok ? std::make_shared<OldEcConnection>(oldECConnectionInfo) : AbstractECConnectionPtr() );
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
        if( errorCode == ErrorCode::ok || errorCode == ErrorCode::unauthorized )
        {
            handler->done( reqID, errorCode, connectionInfo );
            QMutexLocker lk( &m_mutex );
            --m_runningRequests;
            return;
        }

        //checking for old EC
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
        connectionInfo->brand = isCompatibilityMode() ? QString() : QnAppInfo::productNameShort();
        connectionInfo->systemName = qnCommon->localSystemName();
        connectionInfo->ecsGuid = qnCommon->moduleGUID().toString();
#ifdef __arm__
        connectionInfo->box = QnAppInfo::armBox();
#endif
        connectionInfo->allowSslConnections = m_sslEnabled;
        connectionInfo->nxClusterProtoVersion = nx_ec::EC2_PROTO_VERSION;
        return ErrorCode::ok;
    }

    int Ec2DirectConnectionFactory::testDirectConnection( const QUrl& /*addr*/, impl::TestConnectionHandlerPtr handler )
    {
        const int reqID = generateRequestID();
        QnConnectionInfo connectionInfo;
        fillConnectionInfo( ApiLoginData(), &connectionInfo );
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
        m_remoteQueryProcessor.processQueryAsync<std::nullptr_t, QnConnectionInfo>(
            addr, ApiCommand::testConnection, std::nullptr_t(), func );
        return reqID;
    }

    ErrorCode Ec2DirectConnectionFactory::getSettings( std::nullptr_t, ApiResourceParamDataList* const outData )
    {
        if( !QnDbManager::instance() )
            return ErrorCode::ioError;
        return QnDbManager::instance()->readSettings( *outData );
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
