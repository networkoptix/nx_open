#include "droid_resource.h"
#include "droid_stream_reader.h"


extern const char* DROID_MANUFACTURER;
const char* QnDroidResource::MANUFACTURE = DROID_MANUFACTURER;

QnDroidResource::QnDroidResource()
{
}

int QnDroidResource::getMaxFps()
{
    return 30;
}

bool QnDroidResource::isResourceAccessible()
{
    return true;
}

bool QnDroidResource::updateMACAddress()
{
    return true;
}

QString QnDroidResource::manufacture() const
{
    return MANUFACTURE;
}

void QnDroidResource::setIframeDistance(int /*frames*/, int /*timems*/)
{

}

QnAbstractStreamDataProvider* QnDroidResource::createLiveDataProvider()
{
    return new PlDroidStreamReader(toSharedPointer());
}

void QnDroidResource::setCropingPhysical(QRect /*croping*/)
{

}

bool QnDroidResource::hasDualStreaming() const
{
    return false;
}

QHostAddress QnDroidResource::getHostAddress() const
{
    QString url = getUrl();
    int start = QString("raw://").length();
    int end = url.indexOf(':', start);
    if (start >= 0 && end > start)
        return QHostAddress(url.mid(start, end-start));
    else
        return QHostAddress();
}

bool QnDroidResource::setHostAddress(const QHostAddress &/*ip*/, QnDomain /*domain*/)
{
    return false;
}
