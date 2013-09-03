#include "desktop_camera_resource_searcher.h"
#include "desktop_camera_resource.h"

static const int KEEP_ALIVE_INTERVAL = 30 * 1000;

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
    connection->setWriteTimeOut(1);
    m_connections[userName] = ClientConnectionInfo(TCPSocketPtr(connection));
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

    QMap<QString, ClientConnectionInfo>::Iterator itr = m_connections.begin();
    
    for(; itr != m_connections.end(); ++itr)
    {
        QnDesktopCameraResourcePtr cam = QnDesktopCameraResourcePtr(new QnDesktopCameraResource);
        QString userName = itr.key();
        cam->setName(lit("desktop-") + userName);
        cam->setModel(lit("virtual desktop camera"));
        cam->setPhysicalId(cam->gePhysicalIdPrefix() + userName);
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
    QMutexLocker lock(&m_mutex);

    QMap<QString, ClientConnectionInfo>::Iterator itr = m_connections.begin();
    while(itr != m_connections.end())
    {
        if (!itr.value().socket->isConnected()) {
            itr = m_connections.erase(itr);
        } 
        else {
            ClientConnectionInfo& conn = itr.value();
            if (conn.useCount == 0 && conn.timer.elapsed() >= KEEP_ALIVE_INTERVAL)
            {
                conn.timer.restart();                
                QString request = QString(lit("KEEP_ALIVE %1 HTTP/1.0\r\n\r\n")).arg("*");
                if (conn.socket->send(request.toLocal8Bit()) < 1) {
                    conn.socket->close();
                    itr = m_connections.erase(itr);
                    continue;
                }
            }
            ++itr;
        }
    }
}

QnDesktopCameraResourceSearcher& QnDesktopCameraResourceSearcher::instance()
{
    static QnDesktopCameraResourceSearcher inst;
    return inst;
}

TCPSocketPtr QnDesktopCameraResourceSearcher::getConnection(const QString& userName)
{
    QMutexLocker lock(&m_mutex);
    QMap<QString, ClientConnectionInfo>::iterator itr = m_connections.find(userName);
    if (itr != m_connections.end()) {
        ClientConnectionInfo& conn = itr.value();
        if (conn.useCount > 0)
            return TCPSocketPtr();
        conn.useCount++;
        return conn.socket;
    }
    return TCPSocketPtr();
}

void QnDesktopCameraResourceSearcher::releaseConnection(const QString& userName)
{
    QMutexLocker lock(&m_mutex);
    QMap<QString, ClientConnectionInfo>::iterator itr = m_connections.find(userName);
    if (itr != m_connections.end()) {
        ClientConnectionInfo& conn = itr.value();
        conn.useCount--;
    }
}
