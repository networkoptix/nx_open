
#ifdef ENABLE_DESKTOP_CAMERA

#include "desktop_camera_resource_searcher.h"
#include "desktop_camera_resource.h"
#include <core/resource/network_resource.h>

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


void QnDesktopCameraResourceSearcher::registerCamera(QSharedPointer<AbstractStreamSocket> connection, const QString& userName)
{
    connection->setSendTimeout(1);
    QMutexLocker lock(&m_mutex);
    m_connections << ClientConnectionInfo(connection, userName);
}

QList<QnResourcePtr> QnDesktopCameraResourceSearcher::checkHostAddr(const QUrl& url, const QAuthenticator& auth, bool doMultichannelCheck)
{
    Q_UNUSED(url)
    Q_UNUSED(auth)
    Q_UNUSED(doMultichannelCheck)
    return QList<QnResourcePtr>();
}

QnResourceList QnDesktopCameraResourceSearcher::findResources(void)
{
    cleanupConnections();

    QnResourceList result;
    QnId rt = qnResTypePool->getResourceTypeId(manufacture(), QLatin1String("SERVER_DESKTOP_CAMERA"));
    if (!rt.isValid())
        return result;

    QMutexLocker lock(&m_mutex);

    QQueue<ClientConnectionInfo>::Iterator itr = m_connections.begin();
    
    for(; itr != m_connections.end(); ++itr)
    {
        QnDesktopCameraResourcePtr cam = QnDesktopCameraResourcePtr(new QnDesktopCameraResource);
        QString userName = itr->userName;
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

    QQueue<ClientConnectionInfo>::Iterator itr = m_connections.begin();
    while(itr != m_connections.end())
    {
        if (!itr->socket->isConnected()) {
            itr = m_connections.erase(itr);
        } 
        else {
            ClientConnectionInfo& conn = *itr;
            if (conn.useCount == 0 && conn.timer.elapsed() >= KEEP_ALIVE_INTERVAL)
            {
                conn.timer.restart();                
                QString request = QString(lit("KEEP-ALIVE %1 RTSP/1.0\r\ncSeq: %2\r\n\r\n")).arg("*").arg(++conn.cSeq);
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
    for (int i = 0; i < m_connections.size(); ++i)
    {
        ClientConnectionInfo& conn = m_connections[i];
        if (conn.useCount == 0 && conn.userName == userName) {
            conn.useCount++;
            return conn.socket;
        }
    }
    return TCPSocketPtr();
}

quint32 QnDesktopCameraResourceSearcher::incCSeq(TCPSocketPtr socket)
{
    QMutexLocker lock(&m_mutex);

    for (int i = 0; i < m_connections.size(); ++i)
    {
        ClientConnectionInfo& conn = m_connections[i];
        if (conn.socket == socket) {
            return ++conn.cSeq;
        }
    }
    return 0;
}

void QnDesktopCameraResourceSearcher::releaseConnection(TCPSocketPtr socket)
{
    QMutexLocker lock(&m_mutex);

    for (int i = 0; i < m_connections.size(); ++i)
    {
        ClientConnectionInfo& conn = m_connections[i];
        if (conn.socket == socket) {
            conn.useCount--;
            conn.timer.restart();
            break;
        }
    }
}

#endif //ENABLE_DESKTOP_CAMERA
