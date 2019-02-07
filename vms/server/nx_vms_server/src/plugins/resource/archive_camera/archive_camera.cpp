#include "archive_camera.h"

const QString kArchiveCamName = QLatin1String("ARCHIVE_CAMERA");

QnArchiveCamResourceSearcher::QnArchiveCamResourceSearcher(QnMediaServerModule* serverModule):
    QnAbstractResourceSearcher(serverModule->commonModule()),
    QnAbstractNetworkResourceSearcher(serverModule->commonModule()),
    nx::vms::server::ServerModuleAware(serverModule)
{
    setDiscoveryMode(DiscoveryMode::disabled);
}

void QnArchiveCamResourceSearcher::pleaseStop()  {}

QnResourceList QnArchiveCamResourceSearcher::findResources() { return QnResourceList(); }

QnResourcePtr QnArchiveCamResourceSearcher::createResource(const QnUuid& resourceTypeId,
    const QnResourceParams& params)
{
    static auto archiveCamTypeId = qnResTypePool->getLikeResourceTypeId("",
        QnArchiveCamResource::cameraName());
    if (resourceTypeId == archiveCamTypeId)
        return QnArchiveCamResourcePtr(new QnArchiveCamResource(serverModule(), params));
    return QnArchiveCamResourcePtr();
}

QString QnArchiveCamResourceSearcher::manufacturer() const  { return kArchiveCamName; }

QList<QnResourcePtr> QnArchiveCamResourceSearcher::checkHostAddr(const nx::utils::Url & /*url*/,
    const QAuthenticator& /*auth*/, bool /*doMultichannelCheck*/)
{
    return QList<QnResourcePtr>();
}

QnArchiveCamResource::QnArchiveCamResource(QnMediaServerModule* serverModule, const QnResourceParams& params):
    nx::vms::server::resource::Camera(serverModule)
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

void QnArchiveCamResource::setMotionMaskPhysical(int /*channel*/)
{
}

CameraDiagnostics::Result QnArchiveCamResource::initializeCameraDriver()
{
    return CameraDiagnostics::LiveVideoIsNotSupportedResult();
}

QnAbstractStreamDataProvider* QnArchiveCamResource::createLiveDataProvider()
{
    return nullptr;
}
