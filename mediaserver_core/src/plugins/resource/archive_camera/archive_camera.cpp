#include "archive_camera.h"

const QString kArchiveCamName = QLatin1String("ARCHIVE_CAMERA");

QnArchiveCamResourceSearcher::QnArchiveCamResourceSearcher(QnCommonModule* commonModule):
    QnAbstractResourceSearcher(commonModule),
    QnAbstractNetworkResourceSearcher(commonModule)
{
    setDiscoveryMode(DiscoveryMode::disabled);
}

void QnArchiveCamResourceSearcher::pleaseStop()  {}

QnResourceList QnArchiveCamResourceSearcher::findResources() { return QnResourceList(); }

QnResourcePtr QnArchiveCamResourceSearcher::createResource(const QnUuid &resourceTypeId, const QnResourceParams& params)
{
    static auto archiveCamTypeId = qnResTypePool->getLikeResourceTypeId("", QnArchiveCamResource::cameraName());
    if (resourceTypeId == archiveCamTypeId)
        return QnArchiveCamResourcePtr(new QnArchiveCamResource(params));
    return QnArchiveCamResourcePtr();
}

QString QnArchiveCamResourceSearcher::manufacture() const  { return kArchiveCamName; }

QList<QnResourcePtr> QnArchiveCamResourceSearcher::checkHostAddr(const QUrl& /*url*/, const QAuthenticator& /*auth*/, bool /*doMultichannelCheck*/)
{
    return QList<QnResourcePtr>();
}

QnArchiveCamResource::QnArchiveCamResource(const QnResourceParams &params)
{
    setId(params.resID);
    setUrl(params.url);
    setVendor(params.vendor);
}

void QnArchiveCamResource::checkIfOnlineAsync(std::function<void(bool)> completionHandler)
{
    completionHandler(false);
}

QString QnArchiveCamResource::cameraName()
{
    return kArchiveCamName;
}

QString QnArchiveCamResource::getDriverName() const
{
    return kArchiveCamName;
}

void QnArchiveCamResource::setIframeDistance(int /*frames*/, int /*timems*/)
{
}

void QnArchiveCamResource::setMotionMaskPhysical(int /*channel*/)
{
}

nx::mediaserver::resource::StreamCapabilityMap QnArchiveCamResource::getStreamCapabilityMapFromDrives(
    Qn::StreamIndex /*streamIndex*/)
{
    // TODO: implement me
    return nx::mediaserver::resource::StreamCapabilityMap();
}

CameraDiagnostics::Result QnArchiveCamResource::initializeCameraDriver()
{
    return CameraDiagnostics::LiveVideoIsNotSupportedResult();
}

QnAbstractStreamDataProvider* QnArchiveCamResource::createLiveDataProvider()
{
    return nullptr;
}

