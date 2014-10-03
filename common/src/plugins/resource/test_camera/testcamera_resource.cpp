#ifdef ENABLE_TEST_CAMERA

#include "testcamera_resource.h"
#include "testcamera_stream_reader.h"


const QString QnTestCameraResource::MANUFACTURE(lit("NetworkOptix"));

QnTestCameraResource::QnTestCameraResource()
{
}

int QnTestCameraResource::getMaxFps() const
{
    return 30;
}

QString QnTestCameraResource::getDriverName() const
{
    return MANUFACTURE;
}

void QnTestCameraResource::setIframeDistance(int /*frames*/, int /*timems*/)
{

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
