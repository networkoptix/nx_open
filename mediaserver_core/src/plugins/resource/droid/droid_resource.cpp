#ifdef ENABLE_DROID


#include "droid_resource.h"
#include "droid_stream_reader.h"


const QString QnDroidResource::MANUFACTURE(lit("NetworkOptixDroid"));

QnDroidResource::QnDroidResource()
{
}

int QnDroidResource::getMaxFps() const
{
    return 30;
}

QString QnDroidResource::getDriverName() const
{
    return MANUFACTURE;
}

void QnDroidResource::setIframeDistance(int /*frames*/, int /*timems*/)
{

}

nx::mediaserver::resource::StreamCapabilityMap QnDroidResource::getStreamCapabilityMapFromDrives(
    Qn::StreamIndex /*streamIndex*/)
{
    // TODO: implement me
    return nx::mediaserver::resource::StreamCapabilityMap();
}

CameraDiagnostics::Result QnDroidResource::initializeCameraDriver()
{
    return CameraDiagnostics::NoErrorResult();
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

void QnDroidResource::setHostAddress(const QString &/*ip*/)
{
}

#endif //#ifdef ENABLE_DROID
