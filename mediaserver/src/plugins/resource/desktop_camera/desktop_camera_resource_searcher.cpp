
#ifdef ENABLE_DESKTOP_CAMERA

//#define DESKTOP_CAMERA_DEBUG

#include "desktop_camera_resource_searcher.h"
#include "desktop_camera_resource.h"
#include <core/resource/network_resource.h>

namespace {
    const int keepAliveInterval = 5 * 1000;
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


void QnDesktopCameraResourceSearcher::registerCamera(const QSharedPointer<AbstractStreamSocket>& connection, 
													 const QString& userName, const QString &userId)
{
    connection->setSendTimeout(1);
    QMutexLocker lock(&m_mutex);

    auto it = m_connections.begin();
    while (it != m_connections.end()) {
        if (!it->socket->isConnected()) {
            log("cleanup disconnected socket desktop camera", *it);
            it = m_connections.erase(it);
        } else if (it->userId == userId && it->useCount == 0) {
            log("cleanup connection on new register", *it);
            it->socket->close();
            it = m_connections.erase(it);
        } else if (it->userId == userId) {
            log("registerCamera skipped while in use", *it);
            return;
        } else {
            ++it;
        }
    }

    Q_ASSERT_X(!isClientConnectedInternal(userId), Q_FUNC_INFO, "Camera should definitely be disconnected here");

    ClientConnectionInfo info(connection, userName, userId);
    m_connections << info;
    log("registerCamera", info);
}

QList<QnResourcePtr> QnDesktopCameraResourceSearcher::checkHostAddr(const QUrl& url, const QAuthenticator& auth, bool doMultichannelCheck)
{
    Q_UNUSED(url)
    Q_UNUSED(auth)
    Q_UNUSED(doMultichannelCheck)
    return QList<QnResourcePtr>();
}


bool QnDesktopCameraResourceSearcher::isClientConnectedInternal(const QString &uniqueId) const {
    return std::find_if(m_connections.cbegin(), m_connections.cend(), [&uniqueId](const ClientConnectionInfo &info) {
        return info.userId == uniqueId;
    }) != m_connections.end();
}


bool QnDesktopCameraResourceSearcher::isCameraConnected(const QnVirtualCameraResourcePtr &camera) {
    cleanupConnections();

    QString userId = camera->getUniqueId();
    QMutexLocker lock(&m_mutex);
    return isClientConnectedInternal(userId);
}

QnResourceList QnDesktopCameraResourceSearcher::findResources(void)
{
#ifdef DESKTOP_CAMERA_DEBUG
    qDebug() << "QnDesktopCameraResourceSearcher::findResources cycle entered";
#endif

    cleanupConnections();

    QnResourceList result;
    auto rt = qnResTypePool->desktopCameraResourceType();
    if (rt->getId().isNull())
        return result;

    QMutexLocker lock(&m_mutex);

    for(const auto &info: m_connections)
    {
        QnSecurityCamResourcePtr cam = QnSecurityCamResourcePtr(new QnDesktopCameraResource(info.userName));
        cam->setModel(lit("virtual desktop camera"));   //TODO: #GDM globalize the constant
        cam->setTypeId(rt->getId());
        cam->setPhysicalId(info.userId);
        result << cam;
        log("findResources camera", info);
    }
    return result;
}

QnResourcePtr QnDesktopCameraResourceSearcher::createResource(const QnUuid &resourceTypeId, const QnResourceParams& params)
{
    Q_UNUSED(params);
    QnNetworkResourcePtr result;

    QnResourceTypePtr resourceType = qnResTypePool->getResourceType(resourceTypeId);
    Q_ASSERT_X(resourceType, Q_FUNC_INFO, "Desktop camera resource type not found");
    if (!resourceType)
        return result;

    if (resourceType->getManufacture() != manufacture())
        return result;

    result = QnVirtualCameraResourcePtr( new QnDesktopCameraResource() );
    result->setTypeId(resourceTypeId);

    qDebug() << "Create Desktop camera resource. TypeID" << resourceTypeId.toString();
    return result;
}

void QnDesktopCameraResourceSearcher::cleanupConnections()
{
    QMutexLocker lock(&m_mutex);

    QQueue<ClientConnectionInfo>::Iterator itr = m_connections.begin();
    while(itr != m_connections.end())
    {
        if (!itr->socket->isConnected()) {
            log("cleanup disconnected socket desktop camera", *itr);
            itr = m_connections.erase(itr);
        } 
        else {
            ClientConnectionInfo& conn = *itr;
            if (conn.useCount == 0 && conn.timer.elapsed() >= keepAliveInterval)
            {
                conn.timer.restart();                
                QString request = QString(lit("KEEP-ALIVE %1 RTSP/1.0\r\ncSeq: %2\r\n\r\n")).arg("*").arg(++conn.cSeq);
                if (conn.socket->send(request.toLatin1()) < 1) {
                    log("cleanup camera connection could not send keepAlive", conn);
                    conn.socket->close();
                    itr = m_connections.erase(itr);
                    continue;
                }
            }
            ++itr;
        }
    }
}

TCPSocketPtr QnDesktopCameraResourceSearcher::acquireConnection(const QString& userId) {
    QMutexLocker lock(&m_mutex);
    for (auto it = m_connections.begin(); it != m_connections.end(); ++it) {
        if (it->userId != userId)
            continue;
        it->useCount++;
        log("acquiring desktop camera connection", *it);
        return it->socket;
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
            log("releasing desktop camera connection", conn);
            break;
        }
    }
}

void QnDesktopCameraResourceSearcher::log(const QByteArray &message, const ClientConnectionInfo &info) const {
#ifdef DESKTOP_CAMERA_DEBUG
    qDebug() << "QnDesktopCameraResourceSearcher::" << message << info.userName << info.userId << info.useCount;
#else
    QN_UNUSED(message, info);
#endif
}

#endif //ENABLE_DESKTOP_CAMERA
