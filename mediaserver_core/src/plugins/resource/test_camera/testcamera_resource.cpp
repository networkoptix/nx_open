#ifdef ENABLE_TEST_CAMERA

#include "testcamera_resource.h"
#include "testcamera_stream_reader.h"

QnTestCameraResource::QnTestCameraResource()
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

void QnTestCameraResource::setIframeDistance(int /*frames*/, int /*timems*/)
{

}

nx::mediaserver::resource::StreamCapabilityMap QnTestCameraResource::getStreamCapabilityMapFromDrive(
    bool primaryStream)
{
    // TODO: implement me
    return nx::mediaserver::resource::StreamCapabilityMap();
}

CameraDiagnostics::Result QnTestCameraResource::initializeCameraDriver()
{
    return CameraDiagnostics::NoErrorResult();
}

QnAbstractStreamDataProvider* QnTestCameraResource::createLiveDataProvider()
{
    return new QnTestCameraStreamReader(toSharedPointer());
}

QString QnTestCameraResource::getHostAddress() const
{
    QString url = getUrl();
    int start = QString(QLatin1String("tcp://")).length();
    int end = url.indexOf(QLatin1Char(':'), start);
    if (start >= 0 && end > start)
        return url.mid(start, end-start);
    else
        return QString();
}

void QnTestCameraResource::setHostAddress(const QString &ip)
{
    QUrl url(getUrl());
    url.setHost(ip);
    setUrl(url.toString());
}

#endif // #ifdef ENABLE_TEST_CAMERA
