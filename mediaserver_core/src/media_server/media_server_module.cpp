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

#include <utils/media/ffmpeg_initializer.h>
#include <utils/common/buffered_file.h>
#include <utils/common/writer_pool.h>

#include <utils/common/delayed.h>
#include <utils/common/util.h>
#include <plugins/storage/dts/vmax480/vmax480_tcp_server.h>
#include <streaming/streaming_chunk_cache.h>
#include "streaming/streaming_chunk_transcoder.h"
#include <recorder/file_deletor.h>

#include <core/resource/avi/avi_resource.h>
#include <nx/mediaserver_core/ptz/server_ptz_controller_pool.h>
#include <nx/mediaserver/event/rule_processor.h>
#include <core/dataprovider/data_provider_factory.h>

#include <recorder/storage_db_pool.h>
#include <recorder/storage_manager.h>
#include <recorder/archive_integrity_watcher.h>
#include <recorder/wearable_archive_synchronizer.h>
#include <common/static_common_module.h>
#include <utils/common/app_info.h>

#include <nx/mediaserver/event/event_message_bus.h>
#include <nx/mediaserver/unused_wallpapers_watcher.h>
#include <nx/mediaserver/license_watcher.h>
#include <nx/mediaserver/metadata/manager_pool.h>
#include <nx/mediaserver/metadata/event_rule_watcher.h>
#include <nx/mediaserver/resource/shared_context_pool.h>
#include <nx/mediaserver/resource/camera.h>
#include <nx/mediaserver/root_tool.h>
#include <nx/mediaserver/server_update_manager.h>

#include <media_server/serverutil.h>
#include <nx/core/access/access_types.h>
#include <core/resource_management/resource_pool.h>

#include <nx/vms/common/p2p/downloader/downloader.h>
#include <plugins/plugin_manager.h>
#include <nx/mediaserver/server_meta_types.h>
#include <analytics/detected_objects_storage/analytics_events_storage.h>

#include "wearable_lock_manager.h"
#include "wearable_upload_manager.h"
#include <core/resource/resource_command_processor.h>
#include <nx/vms/network/reverse_connection_manager.h>
#include <nx/vms/time_sync/server_time_sync_manager.h>
#include <nx/mediaserver/event/event_connector.h>
#include <nx/mediaserver/event/extended_rule_processor.h>
#include "server_update_tool.h"
#include <motion/motion_helper.h>
#include <audit/mserver_audit_manager.h>
#include <database/server_db.h>
#include <streaming/audio_streamer_pool.h>
#include <recorder/recording_manager.h>
#include <server/host_system_password_synchronizer.h>
#include "camera/camera_pool.h"

using namespace nx;
using namespace nx::mediaserver;

namespace {

const auto kLastRunningTime = lit("lastRunningTime");

void installTranslations()
{
    static const QString kDefaultPath(":/translations/common_en_US.qm");

    QnTranslationManager translationManager;
    QnTranslation defaultTranslation = translationManager.loadTranslation(kDefaultPath);
    QnTranslationManager::installTranslation(defaultTranslation);
}

} // namespace


QnMediaServerModule::QnMediaServerModule(
    const QString& enforcedMediatorEndpoint,
    const QString& roSettingsPath,
    const QString& rwSettingsPath,
    QObject* parent)
{
    Q_INIT_RESOURCE(mediaserver_core);
    Q_INIT_RESOURCE(appserver2);
    nx::mediaserver::MetaTypes::initialize();

    m_settings = store(new MSSettings(roSettingsPath, rwSettingsPath));

#ifdef ENABLE_VMAX
    // It depend on Vmax480Resources in the pool. Pool should be cleared before QnVMax480Server destructor.
    store(new QnVMax480Server());
#endif
    m_commonModule = store(new QnCommonModule(/*clientMode*/ false, nx::core::access::Mode::direct));

    instance<QnWriterPool>();
#ifdef ENABLE_ONVIF

    const bool isDiscoveryDisabled = m_settings->settings().noResourceDiscovery();
    QnSoapServer* soapServer = nullptr;
    if (!isDiscoveryDisabled)
    {
        soapServer = store(new QnSoapServer());
        soapServer->bind();
        soapServer->start();     //starting soap server to accept event notifications from onvif cameras
    }
#endif //ENABLE_ONVIF

    store(new QnFfmpegInitializer());

    if (!enforcedMediatorEndpoint.isEmpty())
    {
        nx::network::SocketGlobals::cloud().mediatorConnector().mockupMediatorUrl(
            enforcedMediatorEndpoint,
            enforcedMediatorEndpoint);
    }
    nx::network::SocketGlobals::cloud().mediatorConnector().enable(true);

    store(new QnNewSystemServerFlagWatcher(commonModule()));
    m_unusedWallpapersWatcher = store(new nx::mediaserver::UnusedWallpapersWatcher(commonModule()));
    m_licenseWatcher = store(new nx::mediaserver::LicenseWatcher(commonModule()));

    m_eventMessageBus = store(new nx::mediaserver::event::EventMessageBus(commonModule()));

    store(new nx::mediaserver_core::ptz::ServerPtzControllerPool(commonModule()));

    m_storageDbPool = store(new QnStorageDbPool(this));

    auto streamingChunkTranscoder = store(
        new StreamingChunkTranscoder(
            this,
            nullptr, //< TODO: #ak pass videoCameraPool here. Currently, it is created later.
            StreamingChunkTranscoder::fBeginOfRangeInclusive));

    m_streamingChunkCache = store(new StreamingChunkCache(this, streamingChunkTranscoder));

    // std::shared_pointer based singletones should be placed after InstanceStorage singletones

    m_context.reset(new UniquePtrContext());

    m_analyticsEventsStorage =
        nx::analytics::storage::EventsStorageFactory::instance()
            .create(m_settings->analyticEventsStorage());

    m_context->normalStorageManager.reset(
        new QnStorageManager(
            this,
            m_analyticsEventsStorage.get(),
            QnServer::StoragePool::Normal
        ));

   m_context->backupStorageManager.reset(
        new QnStorageManager(
            this,
            nullptr,
            QnServer::StoragePool::Backup
        ));

    m_rootTool = nx::mediaserver::findRootTool(this, qApp->applicationFilePath());

    store(new QnFileDeletor(this));

    m_p2pDownloader = store(new nx::vms::common::p2p::downloader::Downloader(
        downloadsDirectory(), commonModule(), nullptr, this));

    m_pluginManager = store(new PluginManager(this, &m_pluginContainer));
    m_pluginManager->loadPlugins(roSettings());

    m_eventRuleProcessor = store(new nx::mediaserver::event::ExtendedRuleProcessor(this));
    m_eventConnector = store(new nx::mediaserver::event::EventConnector(this));

    m_metadataRuleWatcher = store(new nx::mediaserver::metadata::EventRuleWatcher(this));
    m_metadataManagerPool = store(new nx::mediaserver::metadata::ManagerPool(this));

    m_sharedContextPool = store(new nx::mediaserver::resource::SharedContextPool(this));
    m_archiveIntegrityWatcher = store(new nx::mediaserver::ServerArchiveIntegrityWatcher(this));
    m_updateManager = store(new nx::mediaserver::ServerUpdateManager(this));
    m_serverUpdateTool = store(new QnServerUpdateTool(this));
    m_motionHelper = store(new QnMotionHelper(settings().dataDir(), this));

    m_resourceDataProviderFactory.reset(new QnDataProviderFactory());
    registerResourceDataProviders();
    m_resourceCommandProcessor.reset(new QnResourceCommandProcessor());

    store(new nx::mediaserver_core::recorder::WearableArchiveSynchronizer(this));

    store(new QnWearableLockManager(this));

    store(new QnWearableUploadManager(this));

    m_serverDb = store(new QnServerDb(this));
    auto auditManager = store(new QnMServerAuditManager(this));
    commonModule()->setAuditManager(auditManager);

    m_audioStreamPool = store(new QnAudioStreamerPool(this));

    m_videoCameraPool = store(new QnVideoCameraPool(
        settings(),
        dataProviderFactory(),
        commonModule()->resourcePool()));

    m_recordingManager = store(new QnRecordingManager(this, nullptr)); //< Mutex manager disabled

    m_hostSystemPasswordSynchronizer = store(new HostSystemPasswordSynchronizer(commonModule()));

    // Translations must be installed from the main application thread.
    executeDelayed(&installTranslations, kDefaultDelay, qApp->thread());
}

QDir QnMediaServerModule::downloadsDirectory() const
{
    static const QString kDownloads("downloads");
    QDir dir(settings().dataDir());
    dir.mkpath(kDownloads);
    dir.cd(kDownloads);
    return dir;
}

QnMediaServerModule::~QnMediaServerModule()
{
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

nx::mediaserver::UnusedWallpapersWatcher* QnMediaServerModule::unusedWallpapersWatcher() const
{
    return m_unusedWallpapersWatcher;
}

nx::mediaserver::LicenseWatcher* QnMediaServerModule::licenseWatcher() const
{
    return m_licenseWatcher;
}

nx::mediaserver::event::EventMessageBus* QnMediaServerModule::eventMessageBus() const
{
    return m_eventMessageBus;
}

PluginManager* QnMediaServerModule::pluginManager() const
{
    return m_pluginManager;
}

nx::mediaserver::metadata::ManagerPool* QnMediaServerModule::metadataManagerPool() const
{
    return m_metadataManagerPool;
}

nx::mediaserver::metadata::EventRuleWatcher* QnMediaServerModule::metadataRuleWatcher() const
{
    return m_metadataRuleWatcher;
}

nx::mediaserver::resource::SharedContextPool* QnMediaServerModule::sharedContextPool() const
{
    return m_sharedContextPool;
}

AbstractArchiveIntegrityWatcher* QnMediaServerModule::archiveIntegrityWatcher() const
{
    return m_archiveIntegrityWatcher;
}

nx::analytics::storage::AbstractEventsStorage* QnMediaServerModule::analyticsEventsStorage() const
{
    return m_analyticsEventsStorage.get();
}

nx::mediaserver::RootTool* QnMediaServerModule::rootTool() const
{
    return m_rootTool.get();
}

void QnMediaServerModule::registerResourceDataProviders()
{
    m_resourceDataProviderFactory->registerResourceType<QnAviResource>();

    m_resourceDataProviderFactory->registerResourceType(
        nx::mediaserver::resource::Camera::staticMetaObject,
        std::bind(&nx::mediaserver::resource::Camera::createDataProvider,
            this, std::placeholders::_1, std::placeholders::_2));
}

nx::CommonUpdateManager* QnMediaServerModule::updateManager() const
{
    return m_updateManager;
}

QnDataProviderFactory* QnMediaServerModule::dataProviderFactory() const
{
    return m_resourceDataProviderFactory.data();
}

QnResourceCommandProcessor* QnMediaServerModule::resourceCommandProcessor() const
{
    return m_resourceCommandProcessor.data();
}

QnResourcePool* QnMediaServerModule::resourcePool() const
{
    return commonModule()->resourcePool();
}

QnResourcePropertyDictionary* QnMediaServerModule::propertyDictionary() const
{
    return commonModule()->propertyDictionary();
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

event::EventConnector* QnMediaServerModule::eventConnector() const
{
    return m_eventConnector;
}

event::ExtendedRuleProcessor* QnMediaServerModule::eventRuleProcessor() const
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

QnServerUpdateTool* QnMediaServerModule::serverUpdateTool() const
{
    return m_serverUpdateTool;
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
