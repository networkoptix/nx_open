#include "appserver.h"

#include "api/app_server_connection.h"
#include "core/resource_management/resource_discovery_manager.h"
#include "device_plugins/server_camera/server_camera.h"
#include "client/client_settings.h"
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
    ec2::AbstractECConnectionPtr appServerConnection = QnAppServerConnectionFactory::getConnection2();

    QByteArray errorString;
    QnResourceList resources;
    ec2::ErrorCode errorCode = ec2::ErrorCode::ok;
    if( (errorCode = appServerConnection->getResourceManager()->getResourcesSync(&resources)) != ec2::ErrorCode::ok)
    {
        qWarning() << "QnAppServerResourceSearcher::findResources(): Can't get resources from appserver. Reason: " << ec2::toString(errorCode);
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
