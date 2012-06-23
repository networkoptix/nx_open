#include "server_camera.h"
#include "plugins/resources/archive/archive_stream_reader.h"
#include "device_plugins/archive/rtsp/rtsp_client_archive_delegate.h"
#include "core/resourcemanagment/resource_pool.h"
#include "core/resource/video_server.h"
#include "api/app_server_connection.h"

void QnServerCameraProcessor::at_serverIfFound(QString)
{
    QnRtspClientArchiveDelegate::setProxyAddr(QString(), 0);
    QnVideoServerConnection::setProxyAddr(QString(), 0);
}

void QnServerCameraProcessor::processResources(const QnResourceList &resources)
{
    QnResourcePool::instance()->addResources(resources);

    foreach(QnResourcePtr res, resources)
    {
        QnVideoServerResourcePtr videoServer = qSharedPointerDynamicCast<QnVideoServerResource>(res);
        if (videoServer) 
        {
            // set proxy. If some media server IF will be found, proxy address will be cleared
            QString url = QnAppServerConnectionFactory::defaultUrl().host();
            if (url.isEmpty())
                url = "127.0.0.1";
            int port = QnAppServerConnectionFactory::defaultMediaProxyPort();
            QnRtspClientArchiveDelegate::setProxyAddr(url, port);
            QnVideoServerConnection::setProxyAddr(url, port);
            connect(videoServer.data(), SIGNAL(serverIFFound(QString)), this, SLOT(at_serverIfFound(QString)));
            videoServer->determineOptimalNetIF();
        }
    }
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
    QString id = getMAC().toString();
    QString urlStr = getUrl(); // some onvif cameras do not give us a mac without password; so as unique id we ses ws setion id 
    
    if (urlStr.contains("://")) {
        QUrl url(urlStr);
        QString tmp = url.queryItemValue("uniq-id");

        if (!tmp.isEmpty()) {
            id = tmp;
        } else {
            qCritical() << "QnServerCamera::getUniqueId: Unique Id is absent in ONVIF device URL: " << urlStr;
        }
    }

    //getUniqueId should never be changed 
	return id + getParentId().toString();
}

QHostAddress QnServerCamera::getHostAddress() const
{
    QString url = getUrl(); // some onvif cameras do not give us a mac without password; so as unique id we ses ws setion id 
    if (!url.contains("://"))
        return QnVirtualCameraResource::getHostAddress();

    return QHostAddress(QUrl(url).host());

}

// --------------------------- QnServerCameraFactory -----------------------------

QnResourcePtr QnServerCameraFactory::createResource(QnId resourceTypeId, const QnResourceParameters &parameters)
{
    QnResourcePtr resource;

    QnResourceTypePtr resourceType = qnResTypePool->getResourceType(resourceTypeId);

    if (resourceType.isNull())
        return resource;

    if (resourceType->getName() == "Storage")
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

