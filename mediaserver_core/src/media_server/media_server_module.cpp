#include "media_server_module.h"

#include <QtCore/QCoreApplication>

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
#include <business/business_message_bus.h>
#include <plugins/storage/dts/vmax480/vmax480_tcp_server.h>
#include <streaming/streaming_chunk_cache.h>
#include <recorder/file_deletor.h>
#include <core/ptz/server_ptz_controller_pool.h>
#include <recorder/storage_db_pool.h>
#include <recorder/storage_manager.h>
#include <common/static_common_module.h>
#include <utils/common/app_info.h>

#include <nx/vms/common/distributed_file_downloader.h>

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

    m_commonModule = store(new QnCommonModule(/*clientMode*/ false));

    instance<QnLongRunnablePool>();
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
        nx::network::SocketGlobals::mediatorConnector().mockupAddress(enforcedMediatorEndpoint);
    nx::network::SocketGlobals::mediatorConnector().enable(true);

    store(new QnNewSystemServerFlagWatcher(commonModule()));
    store(new QnMasterServerStatusWatcher(commonModule()));

    store(new QnBusinessMessageBus(commonModule()));
#ifdef ENABLE_VMAX
    store(new QnVMax480Server(commonModule()));
#endif

    store(new QnServerPtzControllerPool(commonModule()));

    store(new QnStorageDbPool(commonModule()));

    m_streamingChunkCache = store(new StreamingChunkCache(commonModule()));

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

    store(new nx::vms::common::distributed_file_downloader::Downloader(
        downloadsDirectory(), commonModule()));

    // Translations must be installed from the main applicaition thread.
    executeDelayed(&installTranslations, kDefaultDelay, qApp->thread());
}

QnMediaServerModule::~QnMediaServerModule()
{
    m_context.reset();
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

QSettings* QnMediaServerModule::runTimeSettings() const
{
    return m_settings->runTimeSettings();
}
