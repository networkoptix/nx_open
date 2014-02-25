#include "server_camera.h"
#include "plugins/resources/archive/archive_stream_reader.h"
#include "device_plugins/archive/rtsp/rtsp_client_archive_delegate.h"
#include "core/resource_management/resource_pool.h"
#include "core/resource/media_server_resource.h"
#include "api/app_server_connection.h"


void QnLocalFileProcessor::processResources(const QnResourceList &resources)
{
    QnResourcePool::instance()->addResources(resources);
}


QnServerCamera::QnServerCamera()
{
    addFlags(server_live_cam);
}

bool QnServerCamera::isResourceAccessible()
{
    return true;
}

bool QnServerCamera::updateMACAddress()
{
    return true;
}

QString QnServerCamera::getDriverName() const
{
    return QLatin1String("Server camera"); //all other manufacture are also untranslated and should not be translated
}

void QnServerCamera::setIframeDistance(int frames, int timems)
{
    Q_UNUSED(frames)
    Q_UNUSED(timems)
}

QnConstResourceVideoLayoutPtr QnServerCamera::getVideoLayout(const QnAbstractStreamDataProvider* dataProvider)
{
    Q_UNUSED(dataProvider)
    // todo: layout must be loaded in resourceParams
    return QnMediaResource::getVideoLayout();
}

QnConstResourceAudioLayoutPtr QnServerCamera::getAudioLayout(const QnAbstractStreamDataProvider* dataProvider)
{
    const QnArchiveStreamReader* archive = dynamic_cast<const QnArchiveStreamReader*> (dataProvider);
    if (archive)
        return archive->getDPAudioLayout();
    else
        return QnMediaResource::getAudioLayout();
}


QnAbstractStreamDataProvider* QnServerCamera::createLiveDataProvider()
{
    QnArchiveStreamReader* result = new QnArchiveStreamReader(toSharedPointer());
    result->setArchiveDelegate(new QnRtspClientArchiveDelegate(result));
    return result;
}

QString QnServerCamera::getUniqueId() const
{
    return getPhysicalId() + getParentId().toString();
}

QString QnServerCamera::getUniqueIdForServer(const QnResourcePtr mServer) const
{
    return getPhysicalId() + mServer->getId().toString();
}

QnServerCameraPtr QnServerCamera::findEnabledSibling()
{
    if (!isDisabled())
        return toSharedPointer().dynamicCast<QnServerCamera>();

    {
        QMutexLocker lock(&m_mutex);
        if (m_activeCamera && !m_activeCamera->isDisabled())
            return m_activeCamera;
    }

    QnNetworkResourceList resList = qnResPool->getAllNetResourceByPhysicalId(getPhysicalId());
    foreach(const QnNetworkResourcePtr& netRes, resList)
    {
        if (!netRes->isDisabled()) {
            QnServerCameraPtr cam = netRes.dynamicCast<QnServerCamera>();
            if (cam) {
                QMutexLocker lock(&m_mutex);
                m_activeCamera = cam;
                return m_activeCamera;
            }
        }
    }

    return QnServerCameraPtr();
}


// --------------------------- QnServerCameraFactory -----------------------------

QnResourcePtr QnServerCameraFactory::createResource(QnId resourceTypeId, const QString& url)
{
    QnResourcePtr resource;

    QnResourceTypePtr resourceType = qnResTypePool->getResourceType(resourceTypeId);

    if (resourceType.isNull())
        return resource;

    if (resourceType->getName() == QLatin1String("Storage"))
    {
        //  storage = QnAbstractStorageResourcePtr(QnStoragePluginFactory::instance()->createStorage(pb_storage.url().c_str()));
        resource =  QnAbstractStorageResourcePtr(new QnAbstractStorageResource());
    }
    else {
        // Currently we support only cameras.
        if (!resourceType->isCamera())
            return resource;

        resource = QnResourcePtr(new QnServerCamera());
        resource->setTypeId(resourceTypeId);
    }
    //resource->deserialize(parameters);
    return resource;
}

QnServerCameraFactory& QnServerCameraFactory::instance()
{
    static QnServerCameraFactory _instance;

    return _instance;
}

