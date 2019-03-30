#ifdef ENABLE_TEST_CAMERA

#include "testcamera_resource.h"
#include "testcamera_stream_reader.h"

QnTestCameraResource::QnTestCameraResource(QnMediaServerModule* serverModule):
    nx::vms::server::resource::Camera(serverModule)
{
}

int QnTestCameraResource::getMaxFps() const
{
    return 30;
}

QString QnTestCameraResource::getDriverName() const
{
    return QLatin1String(kManufacturer);
}

CameraDiagnostics::Result QnTestCameraResource::initializeCameraDriver()
{
    return CameraDiagnostics::NoErrorResult();
}

QnAbstractStreamDataProvider* QnTestCameraResource::createLiveDataProvider()
{
    return new QnTestCameraStreamReader(toSharedPointer(this));
}

QString QnTestCameraResource::getHostAddress() const
{
    nx::utils::Url url(getUrl());
    NX_ASSERT(url.isValid(), lm("invald URL: %1").args(url));
    return url.host();
}

void QnTestCameraResource::setHostAddress(const QString &ip)
{
    nx::utils::Url url(getUrl());
    url.setHost(ip);
    NX_ASSERT(url.isValid(), lm("invald URL: %1").args(url));
    setUrl(url.toString());
}

#endif // #ifdef ENABLE_TEST_CAMERA
