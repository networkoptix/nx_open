#include "server_camera.h"
#include "plugins/resources/archive/archive_stream_reader.h"
#include "device_plugins/archive/rtsp/rtsp_client_archive_delegate.h"
#include "core/resource_managment/resource_pool.h"
#include "core/resource/media_server_resource.h"
#include "api/app_server_connection.h"

void QnServerCameraProcessor::at_serverIfFound(const QnMediaServerResource &resource, const QString &)
{
    resource->apiConnection()->setProxyAddr(QString(), 0);
}

void QnServerCameraProcessor::processResources(const QnResourceList &resources)
{
    QnResourcePool::instance()->addResources(resources);

    foreach(QnResourcePtr res, resources)
    {
        QnMediaServerResourcePtr mediaServer = qSharedPointerDynamicCast<QnMediaServerResource>(res);
        if (mediaServer)
            determineOptimalIF(mediaServer.data());
    }
}

void QnServerCameraProcessor::determineOptimalIF(QnMediaServerResource* mediaServer)
{
    // set proxy. If some media server IF will be found, proxy address will be cleared
    QString url = QnAppServerConnectionFactory::defaultUrl().host();
    if (url.isEmpty())
        url = QLatin1String("127.0.0.1");
    int port = QnAppServerConnectionFactory::defaultMediaProxyPort();
    mediaServer->apiConnection()->setProxyAddr(url, port);
    disconnect(mediaServer, NULL, this, NULL);
    connect(mediaServer, SIGNAL(serverIFFound(const QnMediaServerResource &, const QString &)), this, SLOT(at_serverIfFound(const QnMediaServerResource &, const QString &)));
    mediaServer->determineOptimalNetIF();
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

QString QnServerCamera::manufacture() const
{
    return QLatin1String("Server camera"); //all other manufacture are also untranslated and should not be translated
}

void QnServerCamera::setIframeDistance(int frames, int timems)
{
    Q_UNUSED(frames)
    Q_UNUSED(timems)
}

void QnServerCamera::setCropingPhysical(QRect croping)
{
    Q_UNUSED(croping)
}

const QnResourceVideoLayout* QnServerCamera::getVideoLayout(const QnAbstractMediaStreamDataProvider* dataProvider)
{
    Q_UNUSED(dataProvider)
    // todo: layout must be loaded in resourceParams
    return QnMediaResource::getVideoLayout();
}

const QnResourceAudioLayout* QnServerCamera::getAudioLayout(const QnAbstractMediaStreamDataProvider* dataProvider)
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

// --------------------------- QnServerCameraFactory -----------------------------

QnResourcePtr QnServerCameraFactory::createResource(QnId resourceTypeId, const QnResourceParameters &parameters)
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
    resource->deserialize(parameters);
    return resource;
}

QnServerCameraFactory& QnServerCameraFactory::instance()
{
    static QnServerCameraFactory _instance;

    return _instance;
}

