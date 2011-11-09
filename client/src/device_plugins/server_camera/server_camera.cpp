#include "server_camera.h"
#include "plugins/resources/archive/archive_stream_reader.h"
#include "device_plugins/archive/rtsp/rtsp_client_archive_delegate.h"
#include "core/resourcemanagment/resource_pool.h"

void QnServerCameraProcessor::processResources(const QnResourceList &resources)
{
    QnResourcePool::instance()->addResources(resources);
}

QnServerCamera::QnServerCamera()
{
    addFlag(server_live_cam);
}

bool QnServerCamera::isResourceAccessible()
{
    return true;
}

bool QnServerCamera::updateMACAddress()
{
    return true;
}

QString QnServerCamera::manufacture() const
{
    return "Server camera";
}

void QnServerCamera::setIframeDistance(int frames, int timems)
{

}

void QnServerCamera::setCropingPhysical(QRect croping)
{

}

const QnVideoResourceLayout* QnServerCamera::getVideoLayout(const QnAbstractMediaStreamDataProvider* dataProvider)
{
    // todo: layout must be loaded in resourceParams
    return QnMediaResource::getVideoLayout();
}

const QnResourceAudioLayout* QnServerCamera::getAudioLayout(const QnAbstractMediaStreamDataProvider* dataProvider)
{
    // todo: layout must be loaded in resourceParams
    return QnMediaResource::getAudioLayout();
}


QnAbstractStreamDataProvider* QnServerCamera::createLiveDataProvider()
{
    QnArchiveStreamReader* result = new QnArchiveStreamReader(toSharedPointer());
    result->setArchiveDelegate(new QnRtspClientArchiveDelegate());
    return result;
}

QString QnServerCamera::getUniqueId() const
{
    QnResourcePtr server = qnResPool->getResourceById(getParentId());
    if (server)
        return server->getUniqueId() + QString('-') + getUrl();
    else
        return QString("Server camera ") + getUrl();
}

// --------------------------- QnServerCameraFactory -----------------------------

QnResourcePtr QnServerCameraFactory::createResource(const QnId& resourceTypeId, const QnResourceParameters& parameters)
{
    QnResourcePtr resource;

    QnResourceTypePtr resourceType = qnResTypePool->getResourceType(resourceTypeId);

    if (resourceType.isNull())
        return resource;

    resource = QnResourcePtr(new QnServerCamera());
    resource->setTypeId(resourceTypeId);
    resource->deserialize(parameters);

    return resource;
}

QnServerCameraFactory& QnServerCameraFactory::instance()
{
    static QnServerCameraFactory _instance;

    return _instance;
}

