#include "appserver.h"

#include "api/app_server_connection.h"
#include "core/resource_managment/resource_discovery_manager.h"
#include "device_plugins/server_camera/server_camera.h"
#include "utils/settings.h"
#include "version.h"

QnAppServerResourceSearcher::QnAppServerResourceSearcher()
{
}

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

    QnAppServerConnectionPtr appServerConnection = QnAppServerConnectionFactory::createConnection();

    QByteArray errorString;
    QnResourceList resources;

    if (appServerConnection->getResources(resources) != 0)
    {
        qDebug() << "QnAppServerResourceSearcher::findResources(): Can't get resources from appserver. Reason: " << appServerConnection->getLastError();
    } else
    {
        setShouldBeUsed(false);
    }

    return resources;
}

bool QnAppServerResourceSearcher::isResourceTypeSupported(QnId resourceTypeId) const
{
    Q_UNUSED(resourceTypeId)

    return true;
}

QnResourcePtr QnAppServerResourceSearcher::createResource(QnId resourceTypeId, const QnResourceParameters &parameters)
{
    Q_UNUSED(resourceTypeId)
    Q_UNUSED(parameters)

    return QnResourcePtr(0);
}
