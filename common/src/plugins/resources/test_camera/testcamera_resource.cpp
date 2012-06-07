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
    return MANUFACTURE;
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

bool QnTestCameraResource::hasDualStreaming() const
{
    return true;
}

QHostAddress QnTestCameraResource::getHostAddress() const
{
    QString url = getUrl();
    int start = QString("tcp://").length();
    int end = url.indexOf(':', start);
    if (start >= 0 && end > start)
        return QHostAddress(url.mid(start, end-start));
    else
        return QHostAddress();
}

bool QnTestCameraResource::setHostAddress(const QHostAddress &ip, QnDomain domain)
{
    QUrl url(getUrl());
    url.setHost(ip.toString());
    setUrl(url.toString());
    return true;
}

bool QnTestCameraResource::shoudResolveConflicts() const
{
    return false;
}

