#include "desktop_camera_resource_searcher.h"
#include "desktop_camera_resource.h"

QnDesktopCameraResourceSearcher::QnDesktopCameraResourceSearcher()
{

}

QnDesktopCameraResourceSearcher::~QnDesktopCameraResourceSearcher()
{

}

QString QnDesktopCameraResourceSearcher::manufacture() const
{
    return QnDesktopCameraResource::MANUFACTURE;
}


void QnDesktopCameraResourceSearcher::registerCamera(TCPSocket* connection, const QString& userName)
{
    m_connections[userName] = TCPSocketPtr(connection);
}

QList<QnResourcePtr> QnDesktopCameraResourceSearcher::checkHostAddr(const QUrl& url, const QAuthenticator& auth, bool doMultichannelCheck)
{
    return QList<QnResourcePtr>();
}

QnResourceList QnDesktopCameraResourceSearcher::findResources(void)
{
    cleanupConnections();

    QnResourceList result;
    QnId rt = qnResTypePool->getResourceTypeId(manufacture(), QLatin1String("SERVER_DESKTOP_CAMERA"));
    if (!rt.isValid())
        return result;

    QMap<QString, TCPSocketPtr>::Iterator itr = m_connections.begin();
    
    for(; itr != m_connections.end(); ++itr)
    {
        QnDesktopCameraResourcePtr cam = QnDesktopCameraResourcePtr(new QnDesktopCameraResource);
        QString userName = itr.key();
        cam->setName(lit("desktop-") + userName);
        cam->setModel(lit("virtual desktop camera"));
        cam->setPhysicalId(QString(lit("desktop camera %1")).arg(userName));
        cam->setTypeId(rt);
        result << cam;
    }
    return result;
}

QnResourcePtr QnDesktopCameraResourceSearcher::createResource(QnId resourceTypeId, const QnResourceParameters &parameters)
{
    QnNetworkResourcePtr result;

    QnResourceTypePtr resourceType = qnResTypePool->getResourceType(resourceTypeId);

    if (resourceType.isNull())
    {
        qDebug() << "No resource type for ID = " << resourceTypeId;
        return result;
    }

    if (resourceType->getManufacture() != manufacture())
    {
        //qDebug() << "Manufature " << resourceType->getManufacture() << " != " << manufacture();
        return result;
    }

    result = QnVirtualCameraResourcePtr( new QnDesktopCameraResource() );
    result->setTypeId(resourceTypeId);

    qDebug() << "Create Desktop camera resource. TypeID" << resourceTypeId.toString() << ", Parameters: " << parameters;
    result->deserialize(parameters);
    return result;
}

void QnDesktopCameraResourceSearcher::cleanupConnections()
{
    QMap<QString, TCPSocketPtr>::Iterator itr = m_connections.begin();
    while(itr != m_connections.end())
    {
        if (!itr.value()->isConnected())
            itr = m_connections.erase(itr);
        else
            ++itr;
    }
}

QnDesktopCameraResourceSearcher& QnDesktopCameraResourceSearcher::instance()
{
    static QnDesktopCameraResourceSearcher inst;
    return inst;
}
