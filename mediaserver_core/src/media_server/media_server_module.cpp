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

namespace {

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
    QObject* parent)
:
    QnCommonModuleAware(new QnCommonModule(/*clientMode*/ false, this))
{
    instance<QnLongRunnablePool>();

    Q_INIT_RESOURCE(mediaserver_core);
    Q_INIT_RESOURCE(appserver2);

    instance<QnWriterPool>();
#ifdef ENABLE_ONVIF
    store<PasswordHelper>(new PasswordHelper());

    const bool isDiscoveryDisabled =
        MSSettings::roSettings()->value(QnServer::kNoResourceDiscovery, false).toBool();
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
    store(new QnBusinessMessageBus(commonModule()));
#ifdef ENABLE_VMAX
    store(new QnVMax480Server(commonModule()));
#endif
    m_streamingChunkCache = new StreamingChunkCache(commonModule());

    store(new QnServerPtzControllerPool(commonModule()));

    store(new QnStorageDbPool(commonModule()));

    m_normalStorageManager =
        new QnStorageManager(
            this,
            QnServer::StoragePool::Normal
        );

    m_backupStorageManager =
        new QnStorageManager(
            this,
            QnServer::StoragePool::Backup
        );

    store(new QnFileDeletor(commonModule()));

    // Translations must be installed from the main applicaition thread.
    executeDelayed(&installTranslations, kDefaultDelay, qApp->thread());
}

QnMediaServerModule::~QnMediaServerModule()
{
}

StreamingChunkCache* QnMediaServerModule::streamingChunkCache() const
{
    return m_streamingChunkCache;
}
