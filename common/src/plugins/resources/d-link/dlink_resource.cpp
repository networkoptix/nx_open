#include "dlink_resource.h"
#include "../onvif/dataprovider/onvif_mjpeg.h"

const char* QnPlDlinkResource::MANUFACTURE = "Dlink";


QnPlDlinkResource::QnPlDlinkResource()
{
    setAuth("admin", "");
}

bool QnPlDlinkResource::isResourceAccessible()
{
    return updateMACAddress();
}

bool QnPlDlinkResource::updateMACAddress()
{
    return true;
}

QString QnPlDlinkResource::manufacture() const
{
    return MANUFACTURE;
}

void QnPlDlinkResource::setIframeDistance(int frames, int timems)
{

}

QnAbstractStreamDataProvider* QnPlDlinkResource::createLiveDataProvider()
{
    //return new MJPEGtreamreader(toSharedPointer(), "ipcam/stream.cgi?nowprofileid=2&audiostream=0");
    //return new MJPEGtreamreader(toSharedPointer(), "video/mjpg.cgi");
    return new MJPEGtreamreader(toSharedPointer(), "video/mjpg.cgi?profileid=2");
}

void QnPlDlinkResource::setCropingPhysical(QRect croping)
{

}
