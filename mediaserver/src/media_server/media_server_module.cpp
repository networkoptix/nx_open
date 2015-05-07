#include "media_server_module.h"

#include <common/common_module.h>

#ifdef ENABLE_ONVIF
#include <soap/soapserver.h>
#endif

QnMediaServerModule::QnMediaServerModule(int &argc, char **argv, QObject *parent):
    QObject(parent) 
{
    Q_INIT_RESOURCE(mediaserver);
    Q_INIT_RESOURCE(appserver2);


#ifdef ENABLE_ONVIF
    QnSoapServer *soapServer = new QnSoapServer();
    store<QnSoapServer>(soapServer);
    soapServer->bind();
    soapServer->start();     //starting soap server to accept event notifications from onvif cameras
#endif //ENABLE_ONVIF

    m_common = new QnCommonModule(argc, argv, this);
}

QnMediaServerModule::~QnMediaServerModule() {
    return;
}

