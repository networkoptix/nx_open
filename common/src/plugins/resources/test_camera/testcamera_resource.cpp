#include "testcamera_resource.h"
#include "testcamera_stream_reader.h"


const char* QnTestCameraResource::MANUFACTURE = "NetworkOptix";

QnTestCameraResource::QnTestCameraResource()
{
}

int QnTestCameraResource::getMaxFps()
{
    return 30;
}

bool QnTestCameraResource::isResourceAccessible()
{
    return true;
}

bool QnTestCameraResource::updateMACAddress()
{
    return true;
}

QString QnTestCameraResource::manufacture() const
{
    return QLatin1String(MANUFACTURE);
}

void QnTestCameraResource::setIframeDistance(int /*frames*/, int /*timems*/)
{

}

QnAbstractStreamDataProvider* QnTestCameraResource::createLiveDataProvider()
{
    return new QnTestCameraStreamReader(toSharedPointer());
}

void QnTestCameraResource::setCropingPhysical(QRect /*croping*/)
{

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

bool QnTestCameraResource::setHostAddress(const QString &ip, QnDomain /*domain*/)
{
    QUrl url(getUrl());
    url.setHost(ip);
    setUrl(url.toString());
    return true;
}

bool QnTestCameraResource::shoudResolveConflicts() const
{
    return false;
}

