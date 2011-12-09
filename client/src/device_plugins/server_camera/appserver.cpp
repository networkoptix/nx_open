#include "appserver.h"

#include "api/AppServerConnection.h"
#include "core/resourcemanagment/asynch_seacher.h"
#include "device_plugins/server_camera/server_camera.h"
#include "settings.h"
#include "version.h"

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
    QnAppServerConnectionPtr appServerConnection = QnAppServerConnectionFactory::createConnection(QnServerCameraFactory::instance());

    QnResourceList resources;

    appServerConnection->getResources(resources);

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
