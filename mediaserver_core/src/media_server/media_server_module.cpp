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
    QnCommonModule(/*clientMode*/ false, parent)
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

    store(new QnNewSystemServerFlagWatcher());

    // Translations must be installed from the main applicaition thread.
    executeDelayed(&installTranslations, kDefaultDelay, qApp->thread());
}

QnMediaServerModule::~QnMediaServerModule() {
}
