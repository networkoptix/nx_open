#ifdef ENABLE_DROID


#include "droid_resource.h"
#include "droid_stream_reader.h"


const char* QnDroidResource::MANUFACTURE = "NetworkOptixDroid";

QnDroidResource::QnDroidResource()
{
}

int QnDroidResource::getMaxFps() const
{
    return 30;
}

bool QnDroidResource::isResourceAccessible()
{
    return true;
}

QString QnDroidResource::getDriverName() const
{
    return QLatin1String(MANUFACTURE);
}

void QnDroidResource::setIframeDistance(int /*frames*/, int /*timems*/)
{

}

QnAbstractStreamDataProvider* QnDroidResource::createLiveDataProvider()
{
    return new PlDroidStreamReader(toSharedPointer());
}

QString QnDroidResource::getHostAddress() const
{
    QString url = getUrl();
    int start = QString(QLatin1String("raw://")).length();
    int end = url.indexOf(QLatin1Char(':'), start);
    if (start >= 0 && end > start)
        return url.mid(start, end-start);
    else
        return QString();
}

bool QnDroidResource::setHostAddress(const QString &/*ip*/, QnDomain /*domain*/)
{
    return false;
}

#endif //#ifdef ENABLE_DROID
