#include "archive_camera.h"

const QString kArchiveCamName = QLatin1String("ARCHIVE_CAMERA");

QnArchiveCamResource::QnArchiveCamResource()
{}

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

