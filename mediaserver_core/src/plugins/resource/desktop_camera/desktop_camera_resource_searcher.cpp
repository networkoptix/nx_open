#include "desktop_camera_resource_searcher.h"

#ifdef ENABLE_DESKTOP_CAMERA

#include <common/common_module.h>

#include <core/resource/network_resource.h>
#include <core/resource_management/mserver_resource_discovery_manager.h>
#include <core/resource_management/resource_pool.h>

#include <plugins/resource/desktop_camera/desktop_camera_resource.h>

#include <nx/utils/log/log.h>

namespace {

const int kKeepAliveIntervalMs = 5 * 1000;

} // namespace

struct QnDesktopCameraResourceSearcher::ClientConnectionInfo
{
    ClientConnectionInfo(
        const TCPSocketPtr& socket,
        const QString& userName,
        const QString& uniqueId)
        :
        socket(socket),
        userName(userName),
        uniqueId(uniqueId)
    {
        timer.restart();
    }

    TCPSocketPtr socket;
    int useCount = 0;
    quint32 cSeq = 0;
    QElapsedTimer timer;
    QString userName;

    // Combination of user name and module guid.
    QString uniqueId;
};

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

void QnDesktopCameraResourceSearcher::registerCamera(
    const QSharedPointer<AbstractStreamSocket>& connection,
    const QString& userName,
    const QString& uniqueId)
{
    connection->setSendTimeout(1);
    QnMutexLocker lock(&m_mutex);

    auto it = m_connections.begin();
    while (it != m_connections.end())
    {
        if (!it->socket->isConnected())
        {
            log("cleanup disconnected socket desktop camera", *it);
            it = m_connections.erase(it);
        }
        else if (it->uniqueId == uniqueId && it->useCount == 0)
        {
            log("cleanup connection on new register", *it);
            it->socket->close();
            it = m_connections.erase(it);
        }
        else if (it->uniqueId == uniqueId)
        {
            log("registerCamera skipped while in use", *it);
            return;
        }
        else
        {
            ++it;
        }
    }

    NX_ASSERT(!isClientConnectedInternal(uniqueId), Q_FUNC_INFO, "Camera should definitely be disconnected here");

    const ClientConnectionInfo info(connection, userName, uniqueId);
    m_connections << info;

    // Add camera to the pool immediately.
    QnResourceList resources;
    const auto desktopCamera = cameraFromConnection(info);
    resources << desktopCamera;

    auto discoveryManager = commonModule()->resourceDiscoveryManager();

    discoveryManager->addResourcesImmediatly(resources);
    // discovery manager could delay init a bit but this call for Desktop Camera does nothing anyway (empty function)
    // So, do init immediately as well
    if (auto addedCamera = resourcePool()->getResourceById(desktopCamera->getId()))
        addedCamera->init();
    log("register desktop camera", info);
}

QList<QnResourcePtr> QnDesktopCameraResourceSearcher::checkHostAddr(
    const nx::utils::Url& /*url*/,
    const QAuthenticator& /*auth*/,
    bool /*doMultichannelCheck*/)
{
    return QList<QnResourcePtr>();
}


bool QnDesktopCameraResourceSearcher::isClientConnectedInternal(const QString& uniqueId) const
{
    return std::find_if(m_connections.cbegin(), m_connections.cend(),
        [&uniqueId](const ClientConnectionInfo& info)
        {
            return info.uniqueId == uniqueId;
        }) != m_connections.end();
}


bool QnDesktopCameraResourceSearcher::isCameraConnected(const QnVirtualCameraResourcePtr& camera)
{
    cleanupConnections();

    const auto uniqueId = camera->getUniqueId();
    QnMutexLocker lock(&m_mutex);
    return isClientConnectedInternal(uniqueId);
}

QnSecurityCamResourcePtr QnDesktopCameraResourceSearcher::cameraFromConnection(const ClientConnectionInfo& info)
{
    auto cam = QnSecurityCamResourcePtr(new QnDesktopCameraResource(info.userName));
    cam->setModel(lit("virtual desktop camera"));   // TODO: #GDM globalize the constant
    cam->setTypeId(QnResourceTypePool::kDesktopCameraTypeUuid);
    cam->setPhysicalId(info.uniqueId);
    return cam;
}

QnResourceList QnDesktopCameraResourceSearcher::findResources()
{
    cleanupConnections();

    // We must provide all found cameras to avoid moving them to Offline status.
    QnResourceList result;

    QnMutexLocker lock(&m_mutex);
    for (const auto& info: m_connections)
    {
        if (const auto camera = cameraFromConnection(info))
            result << camera;
    }
    return result;
}

QnResourcePtr QnDesktopCameraResourceSearcher::createResource(const QnUuid& resourceTypeId,
    const QnResourceParams& /*params*/)
{
    QnNetworkResourcePtr result;

    auto resourceType = qnResTypePool->getResourceType(resourceTypeId);
    NX_ASSERT(resourceType, Q_FUNC_INFO, "Desktop camera resource type not found");
    if (!resourceType)
        return result;

    if (resourceType->getManufacture() != manufacture())
        return result;

    result = QnVirtualCameraResourcePtr(new QnDesktopCameraResource());
    result->setTypeId(resourceTypeId);

    return result;
}

void QnDesktopCameraResourceSearcher::cleanupConnections()
{
    QnMutexLocker lock(&m_mutex);

    auto itr = m_connections.begin();
    while (itr != m_connections.end())
    {
        if (!itr->socket->isConnected())
        {
            log("cleanup disconnected socket desktop camera", *itr);
            itr = m_connections.erase(itr);
        }
        else
        {
            auto& connection = *itr;
            if (connection.useCount == 0 && connection.timer.elapsed() >= kKeepAliveIntervalMs)
            {
                connection.timer.restart();
                QString request = lit("KEEP-ALIVE %1 RTSP/1.0\r\ncSeq: %2\r\n\r\n")
                    .arg("*")
                    .arg(++connection.cSeq);

                if (connection.socket->send(request.toLatin1()) < 1)
                {
                    log("cleanup camera connection could not send keepAlive", connection);
                    connection.socket->close();
                    itr = m_connections.erase(itr);
                    continue;
                }
            }
            ++itr;
        }
    }
}

TCPSocketPtr QnDesktopCameraResourceSearcher::acquireConnection(const QString& uniqueId)
{
    QnMutexLocker lock(&m_mutex);
    for (auto it = m_connections.begin(); it != m_connections.end(); ++it)
    {
        if (it->uniqueId != uniqueId)
            continue;
        it->useCount++;
        log("acquiring desktop camera connection", *it);
        return it->socket;
    }
    return TCPSocketPtr();
}

quint32 QnDesktopCameraResourceSearcher::incCSeq(const TCPSocketPtr& socket)
{
    QnMutexLocker lock(&m_mutex);

    for (auto& connection: m_connections)
    {
        if (connection.socket == socket)
            return ++connection.cSeq;
    }
    return 0;
}

void QnDesktopCameraResourceSearcher::releaseConnection(const TCPSocketPtr& socket)
{
    QnMutexLocker lock(&m_mutex);

    for (auto& connection: m_connections)
    {
        if (connection.socket == socket)
        {
            connection.useCount--;
            connection.timer.restart();
            log("releasing desktop camera connection", connection);
            break;
        }
    }
}

void QnDesktopCameraResourceSearcher::log(const QByteArray& message,
    const ClientConnectionInfo& info) const
{
    NX_VERBOSE(this) << message << info.userName << info.uniqueId << info.useCount;
}

#endif //ENABLE_DESKTOP_CAMERA
