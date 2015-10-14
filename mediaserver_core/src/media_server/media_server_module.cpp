#include "media_server_module.h"

#include <common/common_globals.h>
#include <common/common_module.h>

#include <database/server_db.h>

#ifdef ENABLE_ONVIF
#include <soap/soapserver.h>
#endif

#include "version.h"

QnMediaServerModule::QnMediaServerModule(QObject *parent):
    QObject(parent) 
{
    Q_INIT_RESOURCE(mediaserver_core);
    Q_INIT_RESOURCE(mediaserver_core_additional);
    Q_INIT_RESOURCE(appserver2);

    QCoreApplication::setOrganizationName(QnAppInfo::organizationName());
    QCoreApplication::setApplicationName(lit(QN_APPLICATION_NAME));
    if (QCoreApplication::applicationVersion().isEmpty())
        QCoreApplication::setApplicationVersion(QnAppInfo::applicationVersion());

#ifdef ENABLE_ONVIF
    QnSoapServer *soapServer = new QnSoapServer();
    store<QnSoapServer>(soapServer);
    soapServer->bind();
    soapServer->start();     //starting soap server to accept event notifications from onvif cameras
#endif //ENABLE_ONVIF

    m_common = new QnCommonModule(this);

    m_common->store<QnServerDb>(new QnServerDb());

}

QnMediaServerModule::~QnMediaServerModule() {
    QCoreApplication::setOrganizationName(QString());
    QCoreApplication::setApplicationName(QString());
    QCoreApplication::setApplicationVersion(QString());   
}

