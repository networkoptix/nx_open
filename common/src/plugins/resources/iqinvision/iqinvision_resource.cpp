#include "../onvif/dataprovider/onvif_mjpeg.h"
#include "iqinvision_resource.h"
#include "../onvif/dataprovider/rtp264_stream_provider.h"

const char* QnPlIqResource::MANUFACTURE = "IqEye";


QnPlIqResource::QnPlIqResource()
{
    setAuth("root", "system");
    
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
    return MANUFACTURE;
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
        name == QLatin1String("IQM32S"))
        return new RTP264StreamReader(toSharedPointer());
        /**/

    return new MJPEGtreamreader(toSharedPointer(), "now.jpg?snap=spush");
}

void QnPlIqResource::setCropingPhysical(QRect /*croping*/)
{

}
