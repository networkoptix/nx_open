#include "media_server_module.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QThread>

#include <common/common_globals.h>
#include <common/common_module.h>

#include <media_server/new_system_flag_watcher.h>

#ifdef ENABLE_ONVIF
#include <soap/soapserver.h>
#endif

#include "server/server_globals.h"
#include <plugins/resource/onvif/onvif_helper.h>

#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/cloud/mediator_connector.h>
#include <nx/network/socket_global.h>

#include <translation/translation_manager.h>

#include <utils/common/buffered_file.h>
#include <utils/common/writer_pool.h>

#include <utils/common/delayed.h>
#include <utils/common/util.h>
#include <plugins/storage/dts/vmax480/vmax480_tcp_server.h>
#include <plugins/storage/dts/vmax480/vmax480_resource_proxy.h>
#include <streaming/streaming_chunk_cache.h>
#include "streaming/streaming_chunk_transcoder.h"
#include <recorder/file_deletor.h>

#include <core/resource/avi/avi_resource.h>
#include <nx/vms/server/ptz/server_ptz_controller_pool.h>
#include <nx/vms/server/event/rule_processor.h>
#include <core/dataprovider/data_provider_factory.h>

#include <recorder/storage_db_pool.h>
#include <recorder/storage_manager.h>
#include <recorder/archive_integrity_watcher.h>
#include <recorder/wearable_archive_synchronizer.h>
#include <common/static_common_module.h>
#include <utils/common/app_info.h>

#include <nx/vms/server/event/event_message_bus.h>
#include <nx/vms/server/unused_wallpapers_watcher.h>
#include <nx/vms/server/license_watcher.h>
#include <nx/vms/server/videowall_license_watcher.h>
#include <nx/vms/server/analytics/manager.h>
#include <nx/vms/server/analytics/event_rule_watcher.h>
#include <nx/vms/server/resource/shared_context_pool.h>
#include <nx/vms/server/resource/camera.h>
#include <nx/vms/server/root_fs.h>
#include <nx/vms/server/update/update_manager.h>
#include <nx/vms/server/meta_types.h>
#include <nx/vms/server/network/multicast_address_registry.h>

#include <nx/vms/server/nvr/i_service.h>
#include <nx/vms/server/nvr/service_factory.h>
#include <nx/vms/server/nvr/hanwha/service_provider.h>

#include <nx/vms/server/analytics/analytics_db.h>
#include <nx/vms/server/analytics/sdk_object_factory.h>

#include <media_server/serverutil.h>
#include <nx/core/access/access_types.h>
#include <core/resource_management/resource_pool.h>

#include <nx/vms/common/p2p/downloader/downloader.h>
#include <plugins/plugin_manager.h>
#include <nx/analytics/db/analytics_db.h>

#include "wearable_lock_manager.h"
#include "wearable_upload_manager.h"
#include <core/resource/resource_command_processor.h>
#include <nx/vms/network/reverse_connection_manager.h>
#include <nx/vms/time_sync/server_time_sync_manager.h>
#include <nx/vms/server/event/event_connector.h>
#include <nx/vms/server/event/extended_rule_processor.h>
#include <nx/vms/server/event/server_runtime_event_manager.h>
#include <nx/vms/server/system/nx1/info.h>
#include <motion/motion_helper.h>
#include <audit/mserver_audit_manager.h>
#include <database/server_db.h>
#include <streaming/audio_streamer_pool.h>
#include <recorder/recording_manager.h>
#include <server/host_system_password_synchronizer.h>
#include "camera/camera_pool.h"
#include <recorder/schedule_sync.h>
#include "media_server_process.h"
#include <camera/camera_error_processor.h>
#include <nx/vms/server/update/update_manager.h>
#include "media_server_resource_searchers.h"
#include <plugins/resource/upnp/global_settings_to_device_searcher_settings_adapter.h>
#include <core/resource_management/mserver_resource_discovery_manager.h>
#include "server_connector.h"
#include "resource_status_watcher.h"
#include <core/resource/media_server_resource.h>
#include <nx/network/url/url_builder.h>
#include <plugins/storage/dts/vmax480/vmax480_resource.h>
#include <nx_vms_server_ini.h>
#include <nx/metrics/metrics_storage.h>
#include <nx/vms/server/analytics/iframe_search_helper.h>
#include <nx/vms/server/statistics/reporter.h>
#include <nx/analytics/db/movable_analytics_db.h>
#include <nx/vms/server/analytics/11/object_type_dictionary.h>

using namespace nx;
using namespace nx::vms::server;

namespace {

const auto kLastRunningTime = lit("lastRunningTime");

} // namespace

class ServerDataProviderFactory:
    public QnDataProviderFactory,
    public /*mixin*/ nx::vms::server::ServerModuleAware
{
public:
    ServerDataProviderFactory(QnMediaServerModule* serverModule):
        QnDataProviderFactory(),
        nx::vms::server::ServerModuleAware(serverModule)
    {
    }

    virtual QnAbstractStreamDataProvider* createDataProvider(
        const QnResourcePtr& resource,
        Qn::ConnectionRole role = Qn::CR_Default) override
    {
        auto result = QnDataProviderFactory::createDataProvider(resource, role);
        auto mediaProvider = dynamic_cast<QnAbstractMediaStreamDataProvider*> (result);
        if (mediaProvider)
        {
            connect(
                mediaProvider,
                &QnAbstractMediaStreamDataProvider::streamEvent,
                serverModule()->cameraErrorProcessor(),
                &nx::vms::server::camera::ErrorProcessor::onStreamReaderEvent,
                Qt::DirectConnection);
        }
        return result;
    }

};

void QnMediaServerModule::initOutgoingSocketCounter()
{
    if (m_settings->settings().noOutgoingConnectionsMetric())
        return;

    nx::network::SocketFactory::setCreateStreamSocketFunc(
        [weakRef = this->commonModule()->metricsWeakRef()](
            bool sslRequired,
            nx::network::NatTraversalSupport natTraversalRequired,
            boost::optional<int> ipVersion)
        {
            auto socket = nx::network::SocketFactory::defaultStreamSocketFactoryFunc(
                sslRequired,
                natTraversalRequired,
                ipVersion);
            if (!socket)
                return socket;
            if (auto ref = weakRef.lock())
            {
                ref->tcpConnections().outgoing()++;
                socket->setBeforeDestroyCallback(
                    [weakRef]()
                    {
                        if (auto ref = weakRef.lock())
                            ref->tcpConnections().outgoing()--;
                    });
            }
            return socket;
        });
}

QnMediaServerModule::QnMediaServerModule(
    const nx::vms::server::CmdLineArguments* arguments,
    std::unique_ptr<MSSettings> serverSettings,
    QObject* /*parent*/)
{
    std::unique_ptr<CmdLineArguments> defaultArguments;
    if (!arguments)
    {
        defaultArguments = std::make_unique<CmdLineArguments>(QCoreApplication::arguments());
        arguments = defaultArguments.get();
    }

    const auto enforcedMediatorEndpoint = arguments->enforcedMediatorEndpoint;

    Q_INIT_RESOURCE(nx_vms_server);
    Q_INIT_RESOURCE(nx_vms_server_db);

    if (serverSettings.get())
    {
        m_settings = store(serverSettings.release());
    }
    else
    {
        m_settings = store(new MSSettings(
            arguments->configFilePath,
            arguments->rwConfigFilePath));
    }

    m_commonModule = store(new QnCommonModule(/*clientMode*/ false, nx::core::access::Mode::direct));

    const bool isRootToolEnabled = !settings().ignoreRootTool();
    m_rootFileSystem = nx::vms::server::instantiateRootFileSystem(
        isRootToolEnabled,
        qApp->applicationFilePath());

    m_platform = store(new QnPlatformAbstraction(timerManager()));
    #if defined(Q_OS_LINUX)
        m_platform->monitor()->setPartitionInformationProvider(
            std::make_unique<nx::vms::server::fs::PartitionsInformationProvider>(
                m_commonModule->globalSettings(),
                m_rootFileSystem.get()));
    #endif

    m_platform->process(nullptr)->setPriority(QnPlatformProcess::HighPriority);

    nx::vms::server::registerSerializers();

    // TODO: #dmishin NVR service factory.

#ifdef ENABLE_VMAX
    // It depend on Vmax480Resources in the pool. Pool should be cleared before QnVMax480Server destructor.
    store(new QnVMax480Server());
#endif
    nx::vms::server::nvr::ServiceFactory serviceFactory;
    serviceFactory.registerServiceProvider(
        std::make_unique<nx::vms::server::nvr::hanwha::ServiceProvider>(this));

    m_nvrService = serviceFactory.createService();

    m_serverRuntimeEventManager =
        std::make_unique<nx::vms::server::event::ServerRuntimeEventManager>(this);

    initOutgoingSocketCounter();

#ifdef ENABLE_VMAX
    store(new QnVmax480ResourceProxy(commonModule()));
#endif

    instance<QnWriterPool>();
#ifdef ENABLE_ONVIF

    const bool isDiscoveryDisabled = m_settings->settings().noResourceDiscovery();
    QnSoapServer* soapServer = nullptr;
    if (!isDiscoveryDisabled)
    {
        soapServer = store(new QnSoapServer());
        soapServer->bind();
        // Starting soap server to accept event notifications from onvif cameras.
        soapServer->start();
    }
#endif //ENABLE_ONVIF

    if (!enforcedMediatorEndpoint.isEmpty())
    {
        nx::network::SocketGlobals::cloud().mediatorConnector().mockupMediatorAddress({
            enforcedMediatorEndpoint,
            enforcedMediatorEndpoint});
    }

    store(new QnNewSystemServerFlagWatcher(commonModule()));
    m_unusedWallpapersWatcher = store(new nx::vms::server::UnusedWallpapersWatcher(commonModule()));
    m_licenseWatcher = store(new nx::vms::server::LicenseWatcher(commonModule()));
    m_videoWallLicenseWatcher = store(new nx::vms::server::VideoWallLicenseWatcher(this));

    m_eventMessageBus = store(new nx::vms::server::event::EventMessageBus(commonModule()));

    m_ptzControllerPool = store(new nx::vms::server::ptz::ServerPtzControllerPool(commonModule()));

    m_storageDbPool = store(new QnStorageDbPool(this));

    m_resourceDataProviderFactory = store(new ServerDataProviderFactory(this));
    registerResourceDataProviders();

    m_videoCameraPool = store(new QnVideoCameraPool(
        settings(),
        m_resourceDataProviderFactory,
        commonModule()->resourcePool()));

    m_streamingChunkTranscoder = store(
        new StreamingChunkTranscoder(
            this,
            StreamingChunkTranscoder::fBeginOfRangeInclusive));

    m_streamingChunkCache = store(new StreamingChunkCache(this, m_streamingChunkTranscoder));

    // std::shared_pointer based singletones should be placed after InstanceStorage singletones

    m_context.reset(new UniquePtrContext());

    m_analyticsIframeSearchHelper = store(
        new nx::vms::server::analytics::IframeSearchHelper(
            commonModule()->resourcePool(), m_videoCameraPool));
    m_objectTypeDictionary = store(
        new nx::vms::server::analytics::ObjectTypeDictionary(
            commonModule()->analyticsObjectTypeDescriptorManager()));
    
    auto analyticsDb = new nx::analytics::db::MovableAnalyticsDb(
        [this, helper = m_analyticsIframeSearchHelper, typeDictionary = m_objectTypeDictionary]()
        {
            return std::make_unique<nx::vms::server::analytics::AnalyticsDb>(
                this, helper, typeDictionary);
        });
    m_analyticsEventsStorage = store(analyticsDb);

    m_context->normalStorageManager.reset(
        new QnStorageManager(
            this,
            m_analyticsEventsStorage,
            QnServer::StoragePool::Normal,
            "normalStorageManager"
        ));

   m_context->backupStorageManager.reset(
        new QnStorageManager(
            this,
            nullptr,
            QnServer::StoragePool::Backup,
            "backupStorageManager"

        ));

    m_context->normalStorageManager->startAuxTimerTasks();
    m_context->backupStorageManager->startAuxTimerTasks();

    if (QnAppInfo::isNx1())
        m_settings->mutableSettings()->setBootedFromSdCard(Nx1::isBootedFromSD(this));

    m_fileDeletor = store(new QnFileDeletor(this));

    m_pluginManager = store(new PluginManager(this));


    if (!settings().noPlugins())
        m_pluginManager->loadPlugins(roSettings());

    m_eventRuleProcessor = store(new nx::vms::server::event::ExtendedRuleProcessor(this));
    m_eventConnector = store(new nx::vms::server::event::EventConnector(this));

    m_analyticsEventRuleWatcher = store(new nx::vms::server::analytics::EventRuleWatcher(this));
    m_analyticsManager = store(new nx::vms::server::analytics::Manager(this));

    m_sharedContextPool = store(new nx::vms::server::resource::SharedContextPool(this));
    if (!ini().disableArchiveIntegrityWatcher)
        m_archiveIntegrityWatcher = store(new nx::vms::server::ServerArchiveIntegrityWatcher(this));
    m_updateManager = store(new nx::vms::server::UpdateManager(this));
    auto dataDir = closeDirPath(settings().dataDir()) + QString("record_catalog");
    m_motionHelper = store(new QnMotionHelper(dataDir, this));

    m_resourceCommandProcessor.reset(new QnResourceCommandProcessor());

    store(new nx::vms::server::recorder::WearableArchiveSynchronizer(this));
    store(new QnWearableLockManager(this));
    store(new QnWearableUploadManager(this));

    m_serverDb = store(new QnServerDb(this));
    auto auditManager = store(new QnMServerAuditManager(this));
    commonModule()->setAuditManager(auditManager);

    m_audioStreamPool = store(new QnAudioStreamerPool(this));

    m_recordingManager = store(new QnRecordingManager(this, nullptr)); //< Mutex manager disabled

    m_hostSystemPasswordSynchronizer = store(new HostSystemPasswordSynchronizer(this));
    m_cameraErrorProcessor = store(new nx::vms::server::camera::ErrorProcessor());

    commonModule()->setResourceDiscoveryManager(
        new QnMServerResourceDiscoveryManager(this));

    m_mdnsListener.reset(new QnMdnsListener());
    auto settingsToDeviceSearcherSettingsAdaptor =
        std::make_unique<GlobalSettingsToDeviceSearcherSettingsAdapter>(commonModule()->resourceDiscoveryManager());
    m_upnpDeviceSearcher = std::make_unique<nx::network::upnp::DeviceSearcher>(
        timerManager(),
        std::move(settingsToDeviceSearcherSettingsAdaptor));
    m_resourceSearchers.reset(new QnMediaServerResourceSearchers(this));
    m_serverConnector = store(new QnServerConnector(commonModule()));
    m_statusWatcher = store(new QnResourceStatusWatcher(commonModule()));
    m_sdkObjectFactory = store(new nx::vms::server::analytics::SdkObjectFactory(this));

    m_hlsSessionPool = store(new nx::vms::server::hls::SessionPool(
        commonModule()->timerManager()));
    m_multicastAddressRegistry = store(new nx::vms::server::network::MulticastAddressRegistry());

    m_statisticsReporter =
        std::make_unique<nx::vms::server::statistics::Reporter>(commonModule());

    // Initialize TranslationManager.
    QPointer<QnTranslationManager> translationManager = instance<QnTranslationManager>();

    // Right now QTranslators are created with qApp as a parent,
    // so we need to call installTranslation() in the main thread.
    auto installProc = [translationManager]()
    {
        if (!translationManager)
            return;
        auto locale = QnAppInfo::defaultLanguage();
        auto defaultTranslation = translationManager->loadTranslation(locale);
        QnTranslationManager::installTranslation(defaultTranslation);
    };
    executeDelayed(installProc, kDefaultDelay, qApp->thread());
}

void QnMediaServerModule::initializeP2PDownloader()
{
    m_p2pDownloader = store(new nx::vms::common::p2p::downloader::Downloader(
        downloadsDirectory(), commonModule(), {}, this));
}

QDir QnMediaServerModule::downloadsDirectory() const
{
    static const QString kDownloads("downloads");
    QDir dir(settings().dataDir());
    const auto downloadsDir = QDir(dir.absoluteFilePath(kDownloads));
    if (downloadsDir.exists())
        return downloadsDir;

    if (!dir.mkdir(kDownloads))
    {
        NX_ERROR(this, "%1() - failed to create directory %2, base dir %3: %4",
            __func__, downloadsDir.absolutePath(),
            dir.exists() ? "exists" : "does not exist", strerror(errno));
    }
    else
    {
        NX_VERBOSE(this, "%1() - created directory %2", __func__, downloadsDir.absolutePath());
    }

    return downloadsDir;
}

void QnMediaServerModule::stopStorages()
{
    backupStorageManager()->scheduleSync()->stop();
    NX_VERBOSE(this, "QnScheduleSync::stop() done");

    normalStorageManager()->cancelRebuildCatalogAsync();
    backupStorageManager()->cancelRebuildCatalogAsync();
    normalStorageManager()->stopAsyncTasks();
    backupStorageManager()->stopAsyncTasks();
}

void QnMediaServerModule::stop()
{
    m_isStopping = true;
    // TODO: Find out why arent all of these are long runnables.

    m_upnpDeviceSearcher->pleaseStop();
    resourceDiscoveryManager()->pleaseStop();

    stopStorages();
    stopLongRunnables();
    m_recordingManager->stop();
    m_videoCameraPool->stop();
    m_serverConnector->stop();
    m_statusWatcher->stop();
    resourceDiscoveryManager()->stop();

    m_videoWallLicenseWatcher->stop();
    m_licenseWatcher->stop();
    m_resourceSearchers->clear();

    #ifdef ENABLE_VMAX
        QnPlVmax480Resource::stopChunkReaders();
    #endif

    m_streamingChunkTranscoder->stop();
    m_eventConnector->stop();
    m_statisticsReporter.reset();
}

void QnMediaServerModule::stopLongRunnables()
{
    for (const auto object: instances())
    {
        if (auto longRunable = dynamic_cast<QnLongRunnable*>(object))
            longRunable->pleaseStop();
    }
    for (const auto object: instances())
    {
        if (auto longRunable = dynamic_cast<QnLongRunnable*>(object))
            longRunable->stop();
    }
}

QnMediaServerModule::~QnMediaServerModule()
{
    stop();
    nx::network::SocketFactory::setCreateStreamSocketFunc(nullptr);
    m_context.reset();
    m_commonModule->resourcePool()->clear();
    clear();
}

StreamingChunkCache* QnMediaServerModule::streamingChunkCache() const
{
    return m_streamingChunkCache;
}

QnCommonModule* QnMediaServerModule::commonModule() const
{
    return m_commonModule;
}

QSettings* QnMediaServerModule::roSettings() const
{
    return m_settings->roSettings();
}

void QnMediaServerModule::syncRoSettings() const
{
    m_settings->syncRoSettings();
}

QSettings* QnMediaServerModule::runTimeSettings() const
{
    return m_settings->runTimeSettings();
}

std::chrono::milliseconds QnMediaServerModule::lastRunningTime() const
{
    return std::chrono::milliseconds(runTimeSettings()->value(kLastRunningTime).toLongLong());
}

std::chrono::milliseconds QnMediaServerModule::lastRunningTimeBeforeRestart() const
{
    if (!m_lastRunningTimeBeforeRestart)
        m_lastRunningTimeBeforeRestart = lastRunningTime();

    return *m_lastRunningTimeBeforeRestart;
}

void QnMediaServerModule::setLastRunningTime(std::chrono::milliseconds value) const
{
    if (!m_lastRunningTimeBeforeRestart)
        m_lastRunningTimeBeforeRestart = lastRunningTime();

    runTimeSettings()->setValue(kLastRunningTime, (qlonglong) value.count());
    runTimeSettings()->sync();
}

nx::vms::server::UnusedWallpapersWatcher* QnMediaServerModule::unusedWallpapersWatcher() const
{
    return m_unusedWallpapersWatcher;
}

nx::vms::server::LicenseWatcher* QnMediaServerModule::licenseWatcher() const
{
    return m_licenseWatcher;
}

nx::vms::server::VideoWallLicenseWatcher* QnMediaServerModule::videoWallLicenseWatcher() const
{
    return m_videoWallLicenseWatcher;
}

nx::vms::server::event::EventMessageBus* QnMediaServerModule::eventMessageBus() const
{
    return m_eventMessageBus;
}

PluginManager* QnMediaServerModule::pluginManager() const
{
    return m_pluginManager;
}

nx::vms::server::analytics::Manager* QnMediaServerModule::analyticsManager() const
{
    return m_analyticsManager;
}

nx::vms::server::analytics::EventRuleWatcher* QnMediaServerModule::analyticsEventRuleWatcher()
    const
{
    return m_analyticsEventRuleWatcher;
}

nx::vms::server::analytics::SdkObjectFactory* QnMediaServerModule::sdkObjectFactory() const
{
    return m_sdkObjectFactory;
}

nx::vms::server::resource::SharedContextPool* QnMediaServerModule::sharedContextPool() const
{
    return m_sharedContextPool;
}

AbstractArchiveIntegrityWatcher* QnMediaServerModule::archiveIntegrityWatcher() const
{
    return m_archiveIntegrityWatcher;
}

nx::analytics::db::AbstractEventsStorage* QnMediaServerModule::analyticsEventsStorage() const
{
    return m_analyticsEventsStorage;
}

nx::vms::server::RootFileSystem* QnMediaServerModule::rootFileSystem() const
{
    return m_rootFileSystem.get();
}

void QnMediaServerModule::registerResourceDataProviders()
{
    m_resourceDataProviderFactory->registerResourceType<QnAviResource>();
    m_resourceDataProviderFactory->registerResourceType<nx::vms::server::resource::Camera>();
}

nx::vms::server::UpdateManager* QnMediaServerModule::updateManager() const
{
    return m_updateManager;
}

QnDataProviderFactory* QnMediaServerModule::dataProviderFactory() const
{
    return m_resourceDataProviderFactory;
}

QnResourceCommandProcessor* QnMediaServerModule::resourceCommandProcessor() const
{
    return m_resourceCommandProcessor.data();
}

QnResourcePool* QnMediaServerModule::resourcePool() const
{
    return commonModule()->resourcePool();
}

QnResourcePropertyDictionary* QnMediaServerModule::resourcePropertyDictionary() const
{
    return commonModule()->resourcePropertyDictionary();
}

QnCameraHistoryPool* QnMediaServerModule::cameraHistoryPool() const
{
    return commonModule()->cameraHistoryPool();
}

QnStorageManager* QnMediaServerModule::normalStorageManager() const
{
    return m_context->normalStorageManager.get();
}

QnStorageManager* QnMediaServerModule::backupStorageManager() const
{
    return m_context->backupStorageManager.get();
}

nx::vms::server::event::EventConnector* QnMediaServerModule::eventConnector() const
{
    return m_eventConnector;
}

nx::vms::server::event::ExtendedRuleProcessor* QnMediaServerModule::eventRuleProcessor() const
{
    return m_eventRuleProcessor;
}

std::shared_ptr<ec2::AbstractECConnection> QnMediaServerModule::ec2Connection() const
{
    return m_commonModule->ec2Connection();
}

QnGlobalSettings* QnMediaServerModule::globalSettings() const
{
    return m_commonModule->globalSettings();
}

QnMotionHelper* QnMediaServerModule::motionHelper() const
{
    return  m_motionHelper;
}

nx::vms::common::p2p::downloader::Downloader* QnMediaServerModule::p2pDownloader() const
{
    return  m_p2pDownloader;
}

QnServerDb* QnMediaServerModule::serverDb() const
{
    return m_serverDb;
}

QnAudioStreamerPool* QnMediaServerModule::audioStreamPool() const
{
    return m_audioStreamPool;
}

QnStorageDbPool* QnMediaServerModule::storageDbPool() const
{
    return m_storageDbPool;
}

QnRecordingManager* QnMediaServerModule::recordingManager() const
{
    return m_recordingManager;
}

HostSystemPasswordSynchronizer* QnMediaServerModule::hostSystemPasswordSynchronizer() const
{
    return m_hostSystemPasswordSynchronizer;
}

QnVideoCameraPool* QnMediaServerModule::videoCameraPool() const
{
    return m_videoCameraPool;
}

QnPtzControllerPool* QnMediaServerModule::ptzControllerPool() const
{
    return m_ptzControllerPool;
}

QnFileDeletor* QnMediaServerModule::fileDeletor() const
{
    return m_fileDeletor;
}

QnResourceAccessManager* QnMediaServerModule::resourceAccessManager() const
{
    return m_commonModule->resourceAccessManager();
}

QnAuditManager* QnMediaServerModule::auditManager() const
{
    return m_commonModule->auditManager();
}

QnResourceDiscoveryManager* QnMediaServerModule::resourceDiscoveryManager() const
{
    return m_commonModule->resourceDiscoveryManager();
}

nx::vms::server::camera::ErrorProcessor* QnMediaServerModule::cameraErrorProcessor() const
{
    return m_cameraErrorProcessor;
}

QnMediaServerResourceSearchers* QnMediaServerModule::resourceSearchers() const
{
    return m_resourceSearchers.get();
}

QnPlatformAbstraction* QnMediaServerModule::platform() const
{
    return m_platform;
}

QnServerConnector* QnMediaServerModule::serverConnector() const
{
    return m_serverConnector;
}

QnResourceStatusWatcher* QnMediaServerModule::statusWatcher() const
{
    return m_statusWatcher;
}

QnMdnsListener* QnMediaServerModule::mdnsListener() const
{
    return m_mdnsListener.get();
}

nx::utils::TimerManager* QnMediaServerModule::timerManager() const
{
    return m_commonModule->timerManager();
}

nx::network::upnp::DeviceSearcher* QnMediaServerModule::upnpDeviceSearcher() const
{
    return m_upnpDeviceSearcher.get();
}

nx::vms::server::hls::SessionPool* QnMediaServerModule::hlsSessionPool() const
{
    return m_hlsSessionPool;
}

nx::vms::server::network::MulticastAddressRegistry*
    QnMediaServerModule::multicastAddressRegistry() const
{
    return m_multicastAddressRegistry;
}

nx::vms::server::nvr::IService* QnMediaServerModule::nvrService() const
{
    return m_nvrService.get();
}

nx::vms::server::event::ServerRuntimeEventManager*
    QnMediaServerModule::serverRuntimeEventManager() const
{
    return m_serverRuntimeEventManager.get();
}

QnStoragePluginFactory* QnMediaServerModule::storagePluginFactory() const
{
    return m_commonModule->storagePluginFactory();
}

QString QnMediaServerModule::metadataDatabaseDir() const
{
    auto server = resourcePool()->getResourceById<QnMediaServerResource>(commonModule()->moduleGUID());
    const auto defaultDir = nx::network::url::normalizePath(settings().dataDir());
    if (!server)
        return defaultDir;
    auto storageResource = resourcePool()->getResourceById<QnStorageResource>(
        server->metadataStorageId());
    const auto pathBase = storageResource ? storageResource->getPath() : defaultDir;
    return closeDirPath(pathBase);
}

using namespace nx::vms::server::analytics;

nx::analytics::db::AbstractIframeSearchHelper* QnMediaServerModule::iFrameSearchHelper() const
{
    return m_analyticsIframeSearchHelper;
}

nx::vms::server::statistics::Reporter* QnMediaServerModule::statisticsReporter() const
{
    return m_statisticsReporter.get();
}
