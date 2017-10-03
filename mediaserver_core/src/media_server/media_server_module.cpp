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

#include <nx/network/socket_global.h>

#include <translation/translation_manager.h>

#include <utils/media/ffmpeg_initializer.h>
#include <utils/common/buffered_file.h>
#include <utils/common/writer_pool.h>
#include "master_server_status_watcher.h"
#include "settings.h"

#include <utils/common/delayed.h>
#include <plugins/storage/dts/vmax480/vmax480_tcp_server.h>
#include <streaming/streaming_chunk_cache.h>
#include "streaming/streaming_chunk_transcoder.h"
#include <recorder/file_deletor.h>
#include <core/ptz/server_ptz_controller_pool.h>
#include <recorder/storage_db_pool.h>
#include <recorder/storage_manager.h>
#include <common/static_common_module.h>
#include <utils/common/app_info.h>
#include <nx/mediaserver/event/event_message_bus.h>
#include <nx/mediaserver/unused_wallpapers_watcher.h>
#include <nx/mediaserver/license_watcher.h>
#include <nx/mediaserver/metadata/manager_pool.h>
#include <nx/mediaserver/metadata/event_rule_watcher.h>
#include <nx/mediaserver/resource/shared_context_pool.h>

#include <nx/core/access/access_types.h>
#include <core/resource_management/resource_pool.h>

#include <nx/vms/common/p2p/downloader/downloader.h>
#include <plugins/plugin_manager.h>

namespace {

void installTranslations()
{
    static const QString kDefaultPath(":/translations/common_en_US.qm");

    QnTranslationManager translationManager;
    QnTranslation defaultTranslation = translationManager.loadTranslation(kDefaultPath);
    QnTranslationManager::installTranslation(defaultTranslation);
}

QDir downloadsDirectory()
{
    const QString varDir = qnServerModule->roSettings()->value("varDir").toString();
    if (varDir.isEmpty())
        return QDir();

    const QDir dir(varDir + lit("/downloads"));
    if (!dir.exists())
        QDir().mkpath(dir.absolutePath());

    return dir;
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

    store(new QnStaticCommonModule(
        Qn::PT_Server,
        QnAppInfo::productNameShort(),
        QnAppInfo::customizationName()));

    m_settings = store(new MSSettings(roSettingsPath, rwSettingsPath));

    m_commonModule = store(new QnCommonModule(/*clientMode*/ false, nx::core::access::Mode::direct));
#ifdef ENABLE_VMAX
    // It depend on Vmax480Resources in the pool. Pool should be cleared before QnVMax480Server destructor.
    store(new QnVMax480Server(commonModule()));
#endif

    instance<QnWriterPool>();
#ifdef ENABLE_ONVIF
    store<PasswordHelper>(new PasswordHelper());

    const bool isDiscoveryDisabled =
        m_settings->roSettings()->value(QnServer::kNoResourceDiscovery, false).toBool();
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
        nx::network::SocketGlobals::mediatorConnector().mockupMediatorUrl(enforcedMediatorEndpoint);
    nx::network::SocketGlobals::mediatorConnector().enable(true);

    store(new QnNewSystemServerFlagWatcher(commonModule()));
    store(new QnMasterServerStatusWatcher(
        commonModule(),
        m_settings->delayBeforeSettingMasterFlag()));
    m_unusedWallpapersWatcher = store(new nx::mediaserver::UnusedWallpapersWatcher(commonModule()));
    m_licenseWatcher = store(new nx::mediaserver::LicenseWatcher(commonModule()));

    store(new nx::mediaserver::event::EventMessageBus(commonModule()));

    store(new QnServerPtzControllerPool(commonModule()));

    store(new QnStorageDbPool(commonModule()));

    auto streamingChunkTranscoder = store(
        new StreamingChunkTranscoder(
            commonModule()->resourcePool(),
            StreamingChunkTranscoder::fBeginOfRangeInclusive));

    m_streamingChunkCache = store(new StreamingChunkCache(
        commonModule()->resourcePool(),
        streamingChunkTranscoder,
        std::chrono::seconds(
            m_settings->roSettings()->value(
                nx_ms_conf::HLS_CHUNK_CACHE_SIZE_SEC,
                nx_ms_conf::DEFAULT_MAX_CACHE_COST_SEC).toUInt())));

    // std::shared_pointer based singletones should be placed after InstanceStorage singletones

    m_context.reset(new UniquePtrContext());

    m_context->normalStorageManager.reset(
        new QnStorageManager(
            commonModule(),
            QnServer::StoragePool::Normal
        ));

    m_context->backupStorageManager.reset(
        new QnStorageManager(
            commonModule(),
            QnServer::StoragePool::Backup
        ));

    store(new QnFileDeletor(commonModule()));

    store(new nx::vms::common::p2p::downloader::Downloader(
        downloadsDirectory(), commonModule(), nullptr, this));

    m_pluginManager = store(new PluginManager(this, QString(), &m_pluginContainer));
    m_pluginManager->loadPlugins(roSettings());

    m_metadataRuleWatcher = store(
        new nx::mediaserver::metadata::EventRuleWatcher(
            commonModule()->eventRuleManager()));

    m_metadataManagerPoolThread = new QThread(this);
    m_metadataManagerPool = store(new nx::mediaserver::metadata::ManagerPool(this));
    m_metadataManagerPool->moveToThread(m_metadataManagerPoolThread);
    m_metadataManagerPoolThread->start();

    m_sharedContextPool = store(new nx::mediaserver::resource::SharedContextPool(this));

    // Translations must be installed from the main applicaition thread.
    executeDelayed(&installTranslations, kDefaultDelay, qApp->thread());
}

QnMediaServerModule::~QnMediaServerModule()
{
    m_commonModule->resourcePool()->clear();
    m_context.reset();
    m_metadataManagerPoolThread->exit();
    m_metadataManagerPoolThread->wait();
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

MSSettings* QnMediaServerModule::settings() const
{
    return m_settings;
}

QSettings* QnMediaServerModule::runTimeSettings() const
{
    return m_settings->runTimeSettings();
}

nx::mediaserver::UnusedWallpapersWatcher* QnMediaServerModule::unusedWallpapersWatcher() const
{
    return m_unusedWallpapersWatcher;
}

nx::mediaserver::LicenseWatcher* QnMediaServerModule::licenseWatcher() const
{
    return m_licenseWatcher;
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
