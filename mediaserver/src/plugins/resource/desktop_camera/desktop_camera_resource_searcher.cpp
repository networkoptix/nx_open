
#ifdef ENABLE_DESKTOP_CAMERA

#include "desktop_camera_resource_searcher.h"
#include "desktop_camera_resource.h"
#include <core/resource/network_resource.h>

namespace {
    const int keepAliveInterval = 30 * 1000;
}

QnDesktopCameraResourceSearcher::QnDesktopCameraResourceSearcher():
    base_type()
{ }

QnDesktopCameraResourceSearcher::~QnDesktopCameraResourceSearcher()
{

}

QString QnDesktopCameraResourceSearcher::manufacture() const
{
    return QnDesktopCameraResource::MANUFACTURE;
}


void QnDesktopCameraResourceSearcher::registerCamera(const QSharedPointer<AbstractStreamSocket>& connection, const QString& userName, const QString &userId)
{
    connection->setSendTimeout(1);
    QMutexLocker lock(&m_mutex);
    m_connections << ClientConnectionInfo(connection, userName, userId);
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
    auto rt = qnResTypePool->desktopCameraResourceType();
    if (rt->getId().isNull())
        return result;

    QMutexLocker lock(&m_mutex);

    QQueue<ClientConnectionInfo>::Iterator itr = m_connections.begin();
    
    for(; itr != m_connections.end(); ++itr)
    {
        QnSecurityCamResourcePtr cam = QnSecurityCamResourcePtr(new QnDesktopCameraResource(itr->userName));
        cam->setModel(lit("virtual desktop camera"));   //TODO: #GDM globalize the constant
        cam->setTypeId(rt->getId());
        cam->setPhysicalId(itr->userId);
        result << cam;
    }
    return result;
}

QnResourcePtr QnDesktopCameraResourceSearcher::createResource(const QUuid &resourceTypeId, const QnResourceParams& params)
{
    Q_UNUSED(params);
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

    qDebug() << "Create Desktop camera resource. TypeID" << resourceTypeId.toString(); // << ", Parameters: " << parameters;
    //result->deserialize(parameters);
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
            if (conn.useCount == 0 && conn.timer.elapsed() >= keepAliveInterval)
            {
                conn.timer.restart();                
                QString request = QString(lit("KEEP-ALIVE %1 RTSP/1.0\r\ncSeq: %2\r\n\r\n")).arg("*").arg(++conn.cSeq);
                if (conn.socket->send(request.toLatin1()) < 1) {
                    conn.socket->close();
                    itr = m_connections.erase(itr);
                    continue;
                }
            }
            ++itr;
        }
    }
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

quint32 QnDesktopCameraResourceSearcher::incCSeq(const TCPSocketPtr& socket)
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

void QnDesktopCameraResourceSearcher::releaseConnection(const TCPSocketPtr& socket)
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
