#include "iqeye_resource.h"
#include "../dataprovider/iqeye_sp_h264.h"
#include "../dataprovider/iqeye_sp_mjpeg.h"

QnPlQEyeResource::QnPlQEyeResource()
{
    
}

QnPlQEyeResource::~QnPlQEyeResource()
{

}


QnResource::DeviceType QnPlQEyeResource::getDeviceType() const
{
    return VIDEODEVICE;
}

QString QnPlQEyeResource::toString() const
{
    return QString("live iqeye ") + getMAC();
}

QnMediaStreamDataProvider* QnPlQEyeResource::getDeviceStreamConnection()
{
    if (getName()=="IQ042S")
        return new CLIQEyeH264treamreader(this);
    else
        return new CLIQEyeMJPEGtreamreader(this);
}

bool QnPlQEyeResource::unknownResource() const
{
    return false;
}

QnNetworkResourcePtr QnPlQEyeResource::updateResource()
{
    return QnNetworkResourcePtr(0);
}
