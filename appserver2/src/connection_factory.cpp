/**********************************************************
* 21 jan 2014
* a.kolesnikov
***********************************************************/

#include "connection_factory.h"

#include <functional>

#include <utils/thread/mutex.h>

#include <network/http_connection_listener.h>
#include <nx_ec/ec_proto_version.h>
#include <utils/common/concurrent.h>
#include <utils/network/http/auth_tools.h>
#include <utils/network/simple_http_client.h>

#include <rest/active_connections_rest_handler.h>

#include "compatibility/old_ec_connection.h"
#include "ec2_connection.h"
#include "ec2_thread_pool.h"
#include "nx_ec/data/api_resource_type_data.h"
#include "nx_ec/data/api_camera_data_ex.h"
#include "nx_ec/data/api_camera_history_data.h"
#include "remote_ec_connection.h"
#include "rest/ec2_base_query_http_handler.h"
#include "rest/ec2_update_http_handler.h"
#include "rest/time_sync_rest_handler.h"
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
        QnMutexLocker lk( &m_mutex );
        m_terminated = true;
    }

    void Ec2DirectConnectionFactory::join()
    {
        QnMutexLocker lk( &m_mutex );
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

        if (m_transactionMessageBus->localPeer().isMobileClient()) {
            QUrlQuery query(url);
            query.removeQueryItem(lit("format"));
            query.addQueryItem(lit("format"), QnLexical::serialized(Qn::JsonFormat));
            url.setQuery(query);
        }

        if (url.isEmpty())
            return testDirectConnection(url, handler);
        else
            return testRemoteConnection(url, handler);
    }

    //!Implementation of AbstractECConnectionFactory::connectAsync
    int Ec2DirectConnectionFactory::connectAsync( const QUrl& addr, const ApiClientInfoData& clientInfo,
                                                  impl::ConnectHandlerPtr handler )
    {
        QUrl url = addr;
        url.setUserName(url.userName().toLower());

        if (m_transactionMessageBus->localPeer().isMobileClient()) {
            QUrlQuery query(url);
            query.removeQueryItem(lit("format"));
            query.addQueryItem(lit("format"), QnLexical::serialized(Qn::JsonFormat));
            url.setQuery(query);
        }

        if (url.scheme() == "file")
            return establishDirectConnection(url, handler);
        else
            return establishConnectionToRemoteServer(url, handler, clientInfo);
    }

    void Ec2DirectConnectionFactory::registerTransactionListener(QnHttpConnectionListener* httpConnectionListener)
    {
        httpConnectionListener->addHandler<QnTransactionTcpProcessor>("HTTP", "ec2/events");
        httpConnectionListener->addHandler<QnHttpTransactionReceiver>("HTTP", INCOMING_TRANSACTIONS_PATH);

        m_sslEnabled = httpConnectionListener->isSslEnabled();
    }

    void Ec2DirectConnectionFactory::registerRestHandlers(QnRestProcessorPool* const p)
    {
        using namespace std::placeholders;

        /**%apidoc GET /ec2/getResourceTypes
         * Read all resource types. Resource type contain object type such as
         * 'Server', 'Camera' e.t.c. Also, resource types contain additional information
         * for cameras such as maximum fps, resolution, e.t.c
         * %param[default] format
         * %return Return object in requested format
         * %// AbstractResourceManager::getResourceTypes
         */
        registerGetFuncHandler<std::nullptr_t, ApiResourceTypeDataList>(p, ApiCommand::getResourceTypes);
        //AbstractResourceManager::getResource
        //registerGetFuncHandler<std::nullptr_t, ApiResourceData>(p, ApiCommand::getResource);
        //AbstractResourceManager::setResourceStatus
        registerUpdateFuncHandler<ApiResourceStatusData>(p, ApiCommand::setResourceStatus);

        /**%apidoc GET /ec2/getResourceParams
         * Read resource (camera, user or server) additional parameters (camera firmware version, e.t.c).
         * List of parameters depends of resource type.
         * %param[default] format
         * %param id Resource unique ID
         * %return Return object in requested format
         * %// AbstractResourceManager::getKvPairs
         */
        registerGetFuncHandler<QnUuid, ApiResourceParamWithRefDataList>(p, ApiCommand::getResourceParams);
        //AbstractResourceManager::save
        registerUpdateFuncHandler<ApiResourceParamWithRefDataList>(p, ApiCommand::setResourceParams);
        //AbstractResourceManager::remove
        registerUpdateFuncHandler<ApiIdData>(p, ApiCommand::removeResource);

        /**%apidoc GET /ec2/getStatusList
         * Read current status values for camers, servers and storages.
         * %param[default] format
         * %param[opt] id Object unique ID
         * %return Returns objects status list data formatted in a requested format. If id parameter is specified, the list contais only one object with that id or nothing, if there is no such object found.
         */
        registerGetFuncHandler<QnUuid, ApiResourceStatusDataList>(p, ApiCommand::getStatusList);

        //AbstractMediaServerManager::getServers
        registerGetFuncHandler<QnUuid, ApiMediaServerDataList>(p, ApiCommand::getMediaServers);
        //AbstractMediaServerManager::save
        registerUpdateFuncHandler<ApiMediaServerData>(p, ApiCommand::saveMediaServer);
        //AbstractCameraManager::saveUserAttributes
        registerUpdateFuncHandler<ApiMediaServerUserAttributesDataList>(p, ApiCommand::saveServerUserAttributesList);
        //AbstractCameraManager::getUserAttributes
        registerGetFuncHandler<QnUuid, ApiMediaServerUserAttributesDataList>(p, ApiCommand::getServerUserAttributes);
        //AbstractMediaServerManager::remove
        registerUpdateFuncHandler<ApiIdData>(p, ApiCommand::removeMediaServer);

        /**%apidoc GET /ec2/getMediaServersEx
         * Return server list
         * %param[default] format
         * %return Return object in requested format
         * %// AbstractMediaServerManager::getServersEx
         */
        registerGetFuncHandler<QnUuid, ApiMediaServerDataExList>(p, ApiCommand::getMediaServersEx);

        registerUpdateFuncHandler<ApiStorageDataList>(p, ApiCommand::saveStorages);
        registerUpdateFuncHandler<ApiStorageData>(p, ApiCommand::saveStorage);
        registerUpdateFuncHandler<ApiIdDataList>(p, ApiCommand::removeStorages);
        registerUpdateFuncHandler<ApiIdData>(p, ApiCommand::removeStorage);
        registerUpdateFuncHandler<ApiIdDataList>(p, ApiCommand::removeResources);

        //AbstractCameraManager::addCamera
        registerUpdateFuncHandler<ApiCameraData>(p, ApiCommand::saveCamera);
        //AbstractCameraManager::save
        registerUpdateFuncHandler<ApiCameraDataList>(p, ApiCommand::saveCameras);
        //AbstractCameraManager::getCameras
        registerGetFuncHandler<QnUuid, ApiCameraDataList>(p, ApiCommand::getCameras);
        //AbstractCameraManager::saveUserAttributes
        registerUpdateFuncHandler<ApiCameraAttributesDataList>(p, ApiCommand::saveCameraUserAttributesList);
        //AbstractCameraManager::getUserAttributes
        registerGetFuncHandler<QnUuid, ApiCameraAttributesDataList>(p, ApiCommand::getCameraUserAttributes);
        //AbstractCameraManager::addCameraHistoryItem
        registerUpdateFuncHandler<ApiServerFootageData>(p, ApiCommand::addCameraHistoryItem);

        /**%apidoc GET /ec2/getCameraHistoryItems
         * Read information about which server hold camera in some time
         * period. This information is used for archive play if camera was moved from
         * one server to another.
         * %param[default] format
         * %return Return object in requested format
         * %// AbstractCameraManager::getCameraHistoryItems
         */
        registerGetFuncHandler<std::nullptr_t, ApiServerFootageDataList>(p, ApiCommand::getCameraHistoryItems);

        /**%apidoc GET /ec2/getCamerasEx
         * Read camera list
         * %param[default] format
         * %param[opt] id Server unique ID
         * %return Returns camera list data formatted in a requested format
         *     %attribute addParams List of additional parameters for camera. This list can contain such information as full ONVIF url, camera maximum fps e.t.c
         *     %attribute audioEnabled Server will push audio stream from a camera if parameter is true
         *     %attribute controlEnabled Server will manage camera (change resolution, fps, create profiles e.t.c) if parameter if true
         *     %attribute dewarpingParams Json string with dewarping parameters
         *     %attribute groupId Internal group identifier. It is used for grouping channels for multi-channels cameras together.
         *     %attribute groupName Group name. This name can be changed by user
         *     %attribute id Object ID. All ID for all objects in a system is GUIDs
         *     %attribute login Login for camera authentication
         *     %attribute mac camera MAC address
         *     %attribute manuallyAdded True if user added camera manually
         *     %attribute maxArchiveDays Maximum archive duration in days for this camera. If value less or equal to zero, it is not used
         *     %attribute minArchiveDays Minimum archive duration in days for this camera. If value less or equal to zero, it is not used
         *     %attribute model Camera model
         *     %attribute motionMask Camera's motion mask. This string defines several rectangles via ':' delimiter. Each rectangle is described by 5 digits: sensitivity, x,y(for left top corner), width, height
         *     %attribute motionType If value is 'MT_SoftwareGrid' then server determine motion, otherwise camera built-in motion will be used
         *     %attribute name Camera name
         *     %attribute parentId Camera's server ID
         *     %attribute physicalId Camera unique identifier. This identifier is used in all requests related to a camera. For instance, in RTSP requests
         *     %attribute password Password for camera authentication
         *     %attribute scheduleEnabled Do record camera archive if true
         *     %attribute scheduleTasks List of 'scheduleTask' objects with camera recording schedule.
         *     %attribute secondaryStreamQuality Secondary stream quality. Possible values:'SSQualityLow', 'SSQualityMedium', 'SSQualityHigh', 'SSQualityDontUse'
         *     %attribute status Camera status. Possible values are: 'Offline', 'Online', 'Recording'
         *     %attribute statusFlags Usually this field is zero. Non zero value is used to mark that a lot of network issues was occurred with this camera
         *     %attribute typeId Unique ID with camera's type. Camera's type can describe predefined information such as camera maximum resolution, fps e.t.c.
         *         Detailed type information can be obtained via 'getResourceTypes' request.
         *     %attribute url Camera plain IP address or full http url if camera was added manually. Also, for multichannel encoders full url is used
         *     %attribute vendor Camera manufacturer
         * %// AbstractCameraManager::getCamerasEx
         */
        registerGetFuncHandler<QnUuid, ApiCameraDataExList>(p, ApiCommand::getCamerasEx);

        /**%apidoc GET /ec2/getStorages
         * Read current storages list
         * %param[default] format
         * %param[opt] id Server unique ID
         * %return Returns storages list data formatted in a requested format. If id parameter is specified, the list contais only that server storages.
         */
        registerGetFuncHandler<QnUuid, ApiStorageDataList>(p, ApiCommand::getStorages);

        //AbstractLicenseManager::addLicenses
        registerUpdateFuncHandler<ApiLicenseDataList>(p, ApiCommand::addLicenses);
        //AbstractLicenseManager::removeLicense
        registerUpdateFuncHandler<ApiLicenseData>(p, ApiCommand::removeLicense);


        /**%apidoc GET /ec2/getBusinessRules
         * Return business rules
         * %param[default] format
         * %return Return object in requested format
         * %// AbstractBusinessEventManager::getBusinessRules
         */
        registerGetFuncHandler<std::nullptr_t, ApiBusinessRuleDataList>(p, ApiCommand::getBusinessRules);

        registerGetFuncHandler<std::nullptr_t, ApiTransactionDataList>(p, ApiCommand::getTransactionLog);

        //AbstractBusinessEventManager::save
        registerUpdateFuncHandler<ApiBusinessRuleData>(p, ApiCommand::saveBusinessRule);
        //AbstractBusinessEventManager::deleteRule
        registerUpdateFuncHandler<ApiIdData>(p, ApiCommand::removeBusinessRule);

        registerUpdateFuncHandler<ApiResetBusinessRuleData>(p, ApiCommand::resetBusinessRules);
        registerUpdateFuncHandler<ApiBusinessActionData>(p, ApiCommand::broadcastBusinessAction);
        registerUpdateFuncHandler<ApiBusinessActionData>(p, ApiCommand::execBusinessAction);


        /**%apidoc GET /ec2/getUsers
         * Return users registered in the system. User's password contain MD5
         * hash data with salt
         * %param[default] format
         * %return Return object in requested format
         * %// AbstractUserManager::getUsers
         */
        registerGetFuncHandler<std::nullptr_t, ApiUserDataList>(p, ApiCommand::getUsers);
        //AbstractUserManager::save
        registerUpdateFuncHandler<ApiUserData>(p, ApiCommand::saveUser);
        //AbstractUserManager::remove
        registerUpdateFuncHandler<ApiIdData>(p, ApiCommand::removeUser);

        /**%apidoc GET /ec2/getVideowalls
         * Return list of video walls
         * %param[default] format
         * %return Return object in requested format
         * %// AbstractVideowallManager::getVideowalls
         */
        registerGetFuncHandler<std::nullptr_t, ApiVideowallDataList>(p, ApiCommand::getVideowalls);
        //AbstractVideowallManager::save
        registerUpdateFuncHandler<ApiVideowallData>(p, ApiCommand::saveVideowall);
        //AbstractVideowallManager::remove
        registerUpdateFuncHandler<ApiIdData>(p, ApiCommand::removeVideowall);
        registerUpdateFuncHandler<ApiVideowallControlMessageData>(p, ApiCommand::videowallControl);

        /**%apidoc GET /ec2/getLayouts
         * Return list of user layout
         * %param[default] format
         * %return Return object in requested format
         * %// AbstractLayoutManager::getLayouts
         */
        registerGetFuncHandler<std::nullptr_t, ApiLayoutDataList>(p, ApiCommand::getLayouts);
        //AbstractLayoutManager::save
        registerUpdateFuncHandler<ApiLayoutDataList>(p, ApiCommand::saveLayouts);
        //AbstractLayoutManager::remove
        registerUpdateFuncHandler<ApiIdData>(p, ApiCommand::removeLayout);

        /**%apidoc GET /ec2/listDirectory
         * Return list of folders and files in a virtual FS stored inside
         * database. This function is used to add files (such audio for notifications)
         * to database.
         * %param[default] format
         * %param[opt] folder Folder name in a virtual FS
         * %return Return object in requested format
         * %// AbstractStoredFileManager::listDirectory
         */
        registerGetFuncHandler<ApiStoredFilePath, ApiStoredDirContents>(p, ApiCommand::listDirectory);

        /**%apidoc GET /ec2/getStoredFile
         * Read file data from a virtual FS
         * %param[default] format
         * %param[opt] folder File name
         * %return Return object in requested format
         * %// AbstractStoredFileManager::getStoredFile
         */
        registerGetFuncHandler<ApiStoredFilePath, ApiStoredFileData>(p, ApiCommand::getStoredFile);
        //AbstractStoredFileManager::addStoredFile
        registerUpdateFuncHandler<ApiStoredFileData>(p, ApiCommand::addStoredFile);
        //AbstractStoredFileManager::updateStoredFile
        registerUpdateFuncHandler<ApiStoredFileData>(p, ApiCommand::updateStoredFile);
        //AbstractStoredFileManager::deleteStoredFile
        registerUpdateFuncHandler<ApiStoredFilePath>(p, ApiCommand::removeStoredFile);

        //AbstractUpdatesManager::uploadUpdate
        registerUpdateFuncHandler<ApiUpdateUploadData>(p, ApiCommand::uploadUpdate);
        //AbstractUpdatesManager::uploadUpdateResponce
        registerUpdateFuncHandler<ApiUpdateUploadResponceData>(p, ApiCommand::uploadUpdateResponce);
        //AbstractUpdatesManager::installUpdate
        registerUpdateFuncHandler<ApiUpdateInstallData>(p, ApiCommand::installUpdate);

        //AbstractDiscoveryManager::discoveredServerChanged
        registerUpdateFuncHandler<ApiDiscoveredServerData>(p, ApiCommand::discoveredServerChanged);
        //AbstractDiscoveryManager::discoveredServersList
        registerUpdateFuncHandler<ApiDiscoveredServerDataList>(p, ApiCommand::discoveredServersList);

        //AbstractDiscoveryManager::discoverPeer
        registerUpdateFuncHandler<ApiDiscoverPeerData>(p, ApiCommand::discoverPeer);
        //AbstractDiscoveryManager::addDiscoveryInformation
        registerUpdateFuncHandler<ApiDiscoveryData>(p, ApiCommand::addDiscoveryInformation);
        //AbstractDiscoveryManager::removeDiscoveryInformation
        registerUpdateFuncHandler<ApiDiscoveryData>(p, ApiCommand::removeDiscoveryInformation);
        //AbstractDiscoveryManager::getDiscoveryData
        registerGetFuncHandler<std::nullptr_t, ApiDiscoveryDataList>(p, ApiCommand::getDiscoveryData);
        //AbstractMiscManager::changeSystemName
        registerUpdateFuncHandler<ApiSystemNameData>(p, ApiCommand::changeSystemName);

       //AbstractECConnection
        registerUpdateFuncHandler<ApiDatabaseDumpData>(p, ApiCommand::restoreDatabase);

        /**%apidoc GET /ec2/getCurrentTime
         * Read current time
         * %param[default] format
         * %param[opt] folder File name
         * %return Return object in requested format
         * %// AbstractTimeManager::getCurrentTimeImpl
         */
        registerGetFuncHandler<std::nullptr_t, ApiTimeData>(p, ApiCommand::getCurrentTime);
        //AbstractTimeManager::forcePrimaryTimeServer
        registerUpdateFuncHandler<ApiIdData>(p, ApiCommand::forcePrimaryTimeServer,
            std::bind(&TimeSynchronizationManager::primaryTimeServerChanged, m_timeSynchronizationManager.get(), _1));
        //TODO #ak register AbstractTimeManager::getPeerTimeInfoList

        //ApiClientInfoData
        registerUpdateFuncHandler<ApiClientInfoData>(p, ApiCommand::saveClientInfo);
        registerGetFuncHandler<std::nullptr_t, ApiClientInfoDataList>(p, ApiCommand::getClientInfos);

        /**%apidoc GET /ec2/getFullInfo
         * Read all data such as all servers, cameras, users
         * e.t.c
         * %param[default] format
         * %param[opt] folder File name
         * %return Return object in requested format
         */
        registerGetFuncHandler<std::nullptr_t, ApiFullInfoData>(p, ApiCommand::getFullInfo);

        /**%apidoc GET /ec2/getLicenses
         * Read license list
         * %param[default] format
         * %param[opt] folder File name
         * %return Return object in requested format
         */
        registerGetFuncHandler<std::nullptr_t, ApiLicenseDataList>(p, ApiCommand::getLicenses);

        registerGetFuncHandler<std::nullptr_t, ApiDatabaseDumpData>(p, ApiCommand::dumpDatabase);
        registerGetFuncHandler<ApiStoredFilePath, qint64>(p, ApiCommand::dumpDatabaseToFile);

        //AbstractECConnectionFactory
        registerFunctorHandler<ApiLoginData, QnConnectionInfo>(p, ApiCommand::connect,
            std::bind(&Ec2DirectConnectionFactory::fillConnectionInfo, this, _1, _2));
        registerFunctorHandler<ApiLoginData, QnConnectionInfo>(p, ApiCommand::testConnection,
            std::bind(&Ec2DirectConnectionFactory::fillConnectionInfo, this, _1, _2));

        /**%apidoc GET /ec2/getSettings
         * Read general system settings such as email address
         * e.t.c
         * %param[default] format
         * %param[opt] folder File name
         * %return Return object in requested format
         */
        registerFunctorHandler<std::nullptr_t, ApiResourceParamDataList>(p, ApiCommand::getSettings,
            std::bind(&Ec2DirectConnectionFactory::getSettings, this, _1, _2));

        //Ec2StaticticsReporter
        registerFunctorHandler<std::nullptr_t, ApiSystemStatistics>(p, ApiCommand::getStatisticsReport,
            [ this ](std::nullptr_t, ApiSystemStatistics* const out) {
                if(!m_directConnection) return ErrorCode::failure;
                return m_directConnection->getStaticticsReporter()->collectReportData(nullptr, out);
            });
        registerFunctorHandler<std::nullptr_t, ApiStatisticsServerInfo>(p, ApiCommand::triggerStatisticsReport,
            [ this ](std::nullptr_t, ApiStatisticsServerInfo* const out) {
                if(!m_directConnection) return ErrorCode::failure;
                return m_directConnection->getStaticticsReporter()->triggerStatisticsReport(nullptr, out);
            });

        p->registerHandler("ec2/activeConnections", new QnActiveConnectionsRestHandler());
        p->registerHandler(QnTimeSyncRestHandler::PATH, new QnTimeSyncRestHandler());

        //using HTTP processor since HTTP REST does not support HTTP interleaving
        //p->registerHandler(
        //    QLatin1String(INCOMING_TRANSACTIONS_PATH),
        //    new QnRestTransactionReceiver());
    }

    void Ec2DirectConnectionFactory::setContext( const ResourceContext& resCtx )
    {
        m_resCtx = resCtx;
        m_timeSynchronizationManager->setContext(m_resCtx);
    }

    void Ec2DirectConnectionFactory::setConfParams( std::map<QString, QVariant> confParams )
    {
        m_settingsInstance.loadParams( std::move( confParams ) );
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
            QnMutexLocker lk( &m_mutex );
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

    int Ec2DirectConnectionFactory::establishConnectionToRemoteServer( const QUrl& addr, impl::ConnectHandlerPtr handler, const ApiClientInfoData& clientInfo )
    {
        const int reqID = generateRequestID();

        ////TODO: #ak return existing connection, if one
        //{
        //    QnMutexLocker lk( &m_mutex );
        //    auto it = m_urlToConnection.find( addr );
        //    if( it != m_urlToConnection.end() )
        //        AbstractECConnectionPtr connection = it->second.second;
        //}

        ApiLoginData loginInfo;
        loginInfo.login = addr.userName();
        loginInfo.passwordHash = nx_http::calcHa1(
            loginInfo.login.toLower(), QnAppInfo::realm(), addr.password() );
        loginInfo.clientInfo = clientInfo;

        {
            QnMutexLocker lk( &m_mutex );
            if( m_terminated )
                return INVALID_REQ_ID;
            ++m_runningRequests;
        }

        const auto info = QString::fromUtf8( QJson::serialized( clientInfo )  );
        NX_LOG( lit("%1 to %2 with %3").arg( Q_FUNC_INFO ).arg( addr.toString() ).arg( info ),
                cl_logDEBUG1 );

        auto func = [this, reqID, addr, handler]( ErrorCode errorCode, const QnConnectionInfo& connectionInfo ) {
            remoteConnectionFinished(reqID, errorCode, connectionInfo, addr, handler); };
        m_remoteQueryProcessor.processQueryAsync<ApiLoginData, QnConnectionInfo>(
            addr, ApiCommand::connect, loginInfo, func );
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
                {
                    if (oldECConnectionInfo.version >= QnSoftwareVersion(2, 3))
                        completionFunc(ErrorCode::ioError, QnConnectionInfo()); //ignoring response from 2.3+ server received using compatibility response
                    else
                        completionFunc(ErrorCode::ok, oldECConnectionInfo);
                }
                else
                {
                    completionFunc(ErrorCode::badResponse, oldECConnectionInfo);
                }
                break;
            }

            case CL_HTTP_AUTH_REQUIRED:
                completionFunc(ErrorCode::unauthorized, QnConnectionInfo());
                break;

            default:
                completionFunc(ErrorCode::ioError, QnConnectionInfo());
                break;
        }

        QnMutexLocker lk( &m_mutex );
        --m_runningRequests;
    }

    void Ec2DirectConnectionFactory::remoteConnectionFinished(
        int reqID,
        ErrorCode errorCode,
        const QnConnectionInfo& connectionInfo,
        const QUrl& ecURL,
        impl::ConnectHandlerPtr handler)
    {
        NX_LOG( QnLog::EC2_TRAN_LOG, lit("Ec2DirectConnectionFactory::remoteConnectionFinished. errorCode = %1, ecURL = %2").
            arg((int)errorCode).arg(ecURL.toString()), cl_logDEBUG2 );

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

        NX_LOG( QnLog::EC2_TRAN_LOG, lit("Ec2DirectConnectionFactory::remoteConnectionFinished (2). errorCode = %1, ecURL = %2").
            arg((int)errorCode).arg(connectionInfoCopy.ecUrl.toString()), cl_logDEBUG2 );

        AbstractECConnectionPtr connection(new RemoteEC2Connection(
            std::make_shared<FixedUrlClientQueryProcessor>(&m_remoteQueryProcessor, connectionInfoCopy.ecUrl),
            m_resCtx,
            connectionInfoCopy));
        handler->done(
            reqID,
            errorCode,
            connection);

        QnMutexLocker lk( &m_mutex );
        --m_runningRequests;
    }

    void Ec2DirectConnectionFactory::remoteTestConnectionFinished(
        int reqID,
        ErrorCode errorCode,
        const QnConnectionInfo& connectionInfo,
        const QUrl& ecURL,
        impl::TestConnectionHandlerPtr handler )
    {
        if( errorCode == ErrorCode::ok || errorCode == ErrorCode::unauthorized || errorCode == ErrorCode::temporary_unauthorized)
        {
            handler->done( reqID, errorCode, connectionInfo );
            QnMutexLocker lk( &m_mutex );
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
        const ApiLoginData& loginInfo,
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
        connectionInfo->ecDbReadOnly = Settings::instance()->dbReadOnly();

		if (!loginInfo.clientInfo.id.isNull())
        {
			auto clientInfo = loginInfo.clientInfo;
			clientInfo.parentId = qnCommon->moduleGUID();

			ApiClientInfoDataList infos;
			auto result = dbManager->doQuery(clientInfo.id, infos);
			if (result != ErrorCode::ok)
				return result;

            if (infos.size() && QJson::serialized(clientInfo) == QJson::serialized(infos.front()))
			{
				NX_LOG(lit("Ec2DirectConnectionFactory: New client had already been registered with the same params"),
					cl_logDEBUG2);
				return ErrorCode::ok;
			}

            QnTransaction<ApiClientInfoData> transaction(ApiCommand::saveClientInfo, clientInfo);
            m_serverQueryProcessor.processUpdateAsync(transaction,
                [&](ErrorCode result) {
					if (result == ErrorCode::ok) {
						NX_LOG(lit("Ec2DirectConnectionFactory: New client has been registered"),
							cl_logINFO);
					}
					else {
						NX_LOG(lit("Ec2DirectConnectionFactory: New client transaction has failed %1")
							.arg(toString(result)), cl_logERROR);
					}
				});
        }

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
            QnMutexLocker lk( &m_mutex );
            if( m_terminated )
                return INVALID_REQ_ID;
            ++m_runningRequests;
        }

        ApiLoginData loginInfo;
        loginInfo.login = addr.userName();
        loginInfo.passwordHash = nx_http::calcHa1(
            loginInfo.login.toLower(), QnAppInfo::realm(), addr.password() );
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
