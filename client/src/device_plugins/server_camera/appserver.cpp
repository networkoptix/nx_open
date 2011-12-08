#include "appserver.h"

#include "api/AppServerConnection.h"
#include "core/resourcemanagment/asynch_seacher.h"
#include "device_plugins/server_camera/server_camera.h"
#include "settings.h"

QnAppServerResourceSearcher& QnAppServerResourceSearcher::instance()
{
    static QnAppServerResourceSearcher _instance;

    return _instance;
}

QString QnAppServerResourceSearcher::manufacture() const
{
    return QLatin1String("Network Optix");
}

QnResourceList QnAppServerResourceSearcher::findResources()
{
    QUrl appserverUrl = QUrl(QSettings().value("appserverUrl", QLatin1String(DEFAULT_APPSERVER_URL)).toString());
    QString user = QLatin1String("appserver");
    QString password = QLatin1String("123");

    QHostAddress host(appserverUrl.host());
    int port = appserverUrl.port();

    QAuthenticator auth;
    auth.setUser(user);
    auth.setPassword(password);

    cl_log.log("Connection to application server ", appserverUrl.toString(), cl_logALWAYS);

    QnAppServerConnection appServerConnection(host, port, auth, QnServerCameraFactory::instance());

    QnResourceList resources;

    appServerConnection.getResources(resources);

    return resources;
}

QnResourcePtr QnAppServerResourceSearcher::createResource(const QnId& resourceTypeId, const QnResourceParameters& parameters)
{
    return QnResourcePtr();
}

bool QnAppServerResourceSearcher::isResourceTypeSupported(const QnId& resourceTypeId) const
{
    return true;
}
