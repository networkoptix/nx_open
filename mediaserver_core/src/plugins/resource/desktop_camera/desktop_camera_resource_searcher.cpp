
#ifdef ENABLE_DESKTOP_CAMERA

//#define DESKTOP_CAMERA_DEBUG

#include "desktop_camera_resource_searcher.h"
#include "desktop_camera_resource.h"
#include <core/resource/network_resource.h>
#include <core/resource_management/mserver_resource_discovery_manager.h>
#include <core/resource_management/resource_pool.h>
#include <common/common_module.h>

namespace {
    const int keepAliveInterval = 5 * 1000;
}

QnDesktopCameraResourceSearcher::QnDesktopCameraResourceSearcher(QnCommonModule* commonModule):
    QnAbstractResourceSearcher(commonModule),
    base_type(commonModule)
{
}

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
    QnMutexLocker lock( &m_mutex );

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

    NX_ASSERT(!isClientConnectedInternal(userId), Q_FUNC_INFO, "Camera should definitely be disconnected here");

    ClientConnectionInfo info(connection, userName, userId);
    m_connections << info;

    // add camera to the pool immediately
    QnResourceList resources;
    auto desktopCamera = cameraFromConnection(info);
    resources << desktopCamera;

    auto discoveryManager = commonModule()->resourceDiscoveryManager();

    discoveryManager->addResourcesImmediatly(resources);
    // discovery manager could delay init a bit but this call for Desktop Camera does nothing anyway (empty function)
    // So, do init immediately as well
    if (auto addedCamera = resourcePool()->getResourceById(desktopCamera->getId()))
        addedCamera->init();
    log("register desktop camera", info);
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
    QnMutexLocker lock( &m_mutex );
    return isClientConnectedInternal(userId);
}

QnSecurityCamResourcePtr QnDesktopCameraResourceSearcher::cameraFromConnection(const ClientConnectionInfo& info)
{
    QnSecurityCamResourcePtr cam = QnSecurityCamResourcePtr(new QnDesktopCameraResource(info.userName));
    cam->setModel(lit("virtual desktop camera"));   // TODO: #GDM globalize the constant
    cam->setTypeId(QnResourceTypePool::kDesktopCameraTypeUuid);
    cam->setPhysicalId(info.userId);
    return cam;
}

QnResourceList QnDesktopCameraResourceSearcher::findResources(void)
{
#ifdef DESKTOP_CAMERA_DEBUG
    qDebug() << "QnDesktopCameraResourceSearcher::findResources cycle entered";
#endif

    cleanupConnections();

    QnResourceList result;

    QnMutexLocker lock( &m_mutex );

    for(const auto &info: m_connections)
    {
        if (auto camera = cameraFromConnection(info))
            result << camera;
    }
    return result;
}

QnResourcePtr QnDesktopCameraResourceSearcher::createResource(const QnUuid &resourceTypeId, const QnResourceParams& params)
{
    Q_UNUSED(params);
    QnNetworkResourcePtr result;

    QnResourceTypePtr resourceType = qnResTypePool->getResourceType(resourceTypeId);
    NX_ASSERT(resourceType, Q_FUNC_INFO, "Desktop camera resource type not found");
    if (!resourceType)
        return result;

    if (resourceType->getManufacture() != manufacture())
        return result;

    result = QnVirtualCameraResourcePtr( new QnDesktopCameraResource() );
    result->setTypeId(resourceTypeId);

    return result;
}

void QnDesktopCameraResourceSearcher::cleanupConnections()
{
    QnMutexLocker lock( &m_mutex );

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
    QnMutexLocker lock( &m_mutex );
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
    QnMutexLocker lock( &m_mutex );

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
    QnMutexLocker lock( &m_mutex );

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
