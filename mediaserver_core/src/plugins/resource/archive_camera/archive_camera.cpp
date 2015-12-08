#include "archive_camera.h"

const QString kArchiveCamName = QLatin1String("ARCHIVE_CAMERA");

QnResourcePtr QnArchiveCamResourceSearcher::createResource(
    const QnUuid            &resourceTypeId, 
    const QnResourceParams  &params
) 
{
    return QnResourcePtr();
}

QList<QnResourcePtr> 
QnArchiveCamResourceSearcher::checkHostAddr(
    const QUrl              &url, 
    const QAuthenticator    &auth, 
    bool                    doMultichannelCheck
) 
{
    return QList<QnResourcePtr>();
}

QString QnArchiveCamResourceSearcher::manufacture() const 
{
    return kArchiveCamName;
}


QnArchiveCamResource::QnArchiveCamResource()
{
    //setMAC(QnMacAddress(mac));
    //setHostAddress(remoteEndpoint.address.toString());
}

bool QnArchiveCamResource::checkIfOnlineAsync(
    std::function<void(bool)>&& completionHandler
) 
{
    completionHandler(false);
    return false;
}

QString QnArchiveCamResource::getDriverName() const 
{
    return kArchiveCamName;
}

void QnArchiveCamResource::setIframeDistance(int frames, int timems) 
{
}

void QnArchiveCamResource::setMotionMaskPhysical(int channel) 
{
}

CameraDiagnostics::Result QnArchiveCamResource::initInternal() 
{
    return CameraDiagnostics::BadMediaStreamResult();
}

QnAbstractStreamDataProvider* QnArchiveCamResource::createLiveDataProvider() 
{
    return nullptr;
}

