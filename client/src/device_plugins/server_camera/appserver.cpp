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

    QByteArray errorString;
    QnResourceList resources;

    if (appServerConnection->getResources(resources, errorString) != 0)
    {
        qDebug() << "QnAppServerResourceSearcher::findResources(): Can't get resources from appserver. Reason: " << errorString;
    }

    return resources;
}

QnResourcePtr QnAppServerResourceSearcher::createResource(const QnId& resourceTypeId, const QnResourceParameters& parameters)
{
    Q_UNUSED(resourceTypeId)
    Q_UNUSED(parameters)

    return QnResourcePtr(0);
}

bool QnAppServerResourceSearcher::isResourceTypeSupported(const QnId& resourceTypeId) const
{
    Q_UNUSED(resourceTypeId)

    return true;
}
