#include "media_server_module.h"

#include <common/common_globals.h>
#include <common/common_module.h>

#include <media_server/new_system_flag_watcher.h>

#ifdef ENABLE_ONVIF
#include <soap/soapserver.h>
#endif

#include "server/server_globals.h"

#include <nx/network/socket_global.h>

#include <utils/media/ffmpeg_initializer.h>
#include <utils/common/buffered_file.h>
#include <utils/common/writer_pool.h>
#include "master_server_status_watcher.h"

QnMediaServerModule::QnMediaServerModule(const QString& enforcedMediatorEndpoint, QObject *parent):
    QObject(parent)
{
    Q_INIT_RESOURCE(mediaserver_core);
    Q_INIT_RESOURCE(appserver2);

    instance<QnWriterPool>();
#ifdef ENABLE_ONVIF
    QnSoapServer *soapServer = new QnSoapServer();
    store<QnSoapServer>(soapServer);
    soapServer->bind();
    soapServer->start();     //starting soap server to accept event notifications from onvif cameras
#endif //ENABLE_ONVIF

    m_common = new QnCommonModule(this);
    m_common->store<QnFfmpegInitializer>(new QnFfmpegInitializer());

    if (!enforcedMediatorEndpoint.isEmpty())
        nx::network::SocketGlobals::mediatorConnector().mockupAddress(enforcedMediatorEndpoint);
    nx::network::SocketGlobals::mediatorConnector().enable(true);

    m_common->store<QnNewSystemServerFlagWatcher>(new QnNewSystemServerFlagWatcher());
    m_common->store<MasterServerStatusWatcher>(new MasterServerStatusWatcher());
}

QnMediaServerModule::~QnMediaServerModule() {
}
