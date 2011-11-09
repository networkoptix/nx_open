#include "api/AppServerConnection.h"
#include "core/resourcemanagment/asynch_seacher.h"
#include "device_plugins/server_camera/server_camera.h"
#include "appserver.h"

QnAppServerResourceSearcher& QnAppServerResourceSearcher::instance()
{
    static QnAppServerResourceSearcher _instance;

    return _instance;
}

QString QnAppServerResourceSearcher::manufacture() const
{
    return "Network Optix";
}

QnResourceList QnAppServerResourceSearcher::findResources()
{
    QHostAddress host(QSettings().value("appserverAddress", "10.0.2.3").toString());
    int port = 8000;
    QAuthenticator auth;
    auth.setUser("appserver");
    auth.setPassword("123");
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
