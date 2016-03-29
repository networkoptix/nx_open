#include "media_server_module.h"

#include <common/common_globals.h>
#include <common/common_module.h>

#include <media_server/new_system_flag_watcher.h>

#ifdef ENABLE_ONVIF
#include <soap/soapserver.h>
#endif

#include "server/server_globals.h"

#include <nx/network/socket_global.h>

QnMediaServerModule::QnMediaServerModule(const QString& enforcedMediatorEndpoint, QObject *parent):
    QObject(parent)
{
    Q_INIT_RESOURCE(mediaserver_core);
    Q_INIT_RESOURCE(mediaserver_core_additional);
    Q_INIT_RESOURCE(appserver2);

#ifdef ENABLE_ONVIF
    QnSoapServer *soapServer = new QnSoapServer();
    store<QnSoapServer>(soapServer);
    soapServer->bind();
    soapServer->start();     //starting soap server to accept event notifications from onvif cameras
#endif //ENABLE_ONVIF


    m_common = new QnCommonModule(this);
    initServerMetaTypes();

    if (!enforcedMediatorEndpoint.isEmpty())
        nx::network::SocketGlobals::mediatorConnector().mockupAddress(enforcedMediatorEndpoint);
    nx::network::SocketGlobals::mediatorConnector().enable(true);

    m_common->store<QnNewSystemServerFlagWatcher>(new QnNewSystemServerFlagWatcher());
}

QnMediaServerModule::~QnMediaServerModule() {
}

void QnMediaServerModule::initServerMetaTypes()
{
}
