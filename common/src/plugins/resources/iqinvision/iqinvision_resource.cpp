#include "../onvif/dataprovider/onvif_mjpeg.h"
#include "iqinvision_resource.h"
#include "../onvif/dataprovider/rtp_stream_provider.h"

const char* QnPlIqResource::MANUFACTURE = "IqEye";


QnPlIqResource::QnPlIqResource()
{
    setAuth(QLatin1String("root"), QLatin1String("system"));
}

bool QnPlIqResource::isResourceAccessible()
{
    return updateMACAddress();
}

bool QnPlIqResource::updateMACAddress()
{
    return true;
}

QString QnPlIqResource::manufacture() const
{
    return QLatin1String(MANUFACTURE);
}

void QnPlIqResource::setIframeDistance(int /*frames*/, int /*timems*/)
{

}

QnAbstractStreamDataProvider* QnPlIqResource::createLiveDataProvider()
{
    
    //return new MJPEGtreamreader(toSharedPointer(), "mjpg/video.mjpg");
    
    QString name = getName();
    if (name == QLatin1String("IQA35") ||
        name == QLatin1String("IQA33N") ||
        //name == QLatin1String("IQA32N") ||
        name == QLatin1String("IQA31") ||
        name == QLatin1String("IQ732N") ||
        name == QLatin1String("IQ732S") ||
        name == QLatin1String("IQ832N") ||
        name == QLatin1String("IQ832S") ||
        name == QLatin1String("IQD30S") ||
        name == QLatin1String("IQD31S") ||
        name == QLatin1String("IQD32S") ||
        name == QLatin1String("IQM30S") ||
        name == QLatin1String("IQM31S") ||
        name == QLatin1String("IQM32N") ||
        //name == QLatin1String("IQ765N") ||
        name == QLatin1String("IQM32S"))
        return new QnRtpStreamReader(toSharedPointer());
        /**/

    return new MJPEGtreamreader(toSharedPointer(), QLatin1String("now.jpg?snap=spush"));
}

void QnPlIqResource::setCropingPhysical(QRect /*croping*/)
{

}

bool QnPlIqResource::initInternal() 
{
    return (setOID(QLatin1String("1.2.6.5"), QLatin1String("1")) == CL_HTTP_SUCCESS); // Reset crop to maximum size
}

CLHttpStatus QnPlIqResource::readOID(const QString& oid, QString& result)
{
    QString request = QLatin1String("get.oid?") + oid;

    CLHttpStatus status;
    
    result = QLatin1String(downloadFile(status, request,  getHostAddress(), 80, 1000, getAuth()));

    if (status == CL_HTTP_AUTH_REQUIRED)
    {
        setStatus(Unauthorized);
    }
    
    return status;
}



CLHttpStatus QnPlIqResource::readOID(const QString& oid, int& result)
{
    QString sresult;
    CLHttpStatus status = readOID(oid, sresult);

    result = sresult.toInt();

    return status;
}

CLHttpStatus QnPlIqResource::setOID(const QString& oid, const QString& val)
{
    QString request = QLatin1String("set.oid?OidTR") + oid + QLatin1String("=") + val;
    CLHttpStatus status;

    downloadFile(status, request,  getHostAddress(), 80, 1000, getAuth());

    if (status == CL_HTTP_AUTH_REQUIRED)
    {
        setStatus(Unauthorized);
    }

    return status;

}

QSize QnPlIqResource::getMaxResolution() const
{
    QSize s;
    return s;
}

