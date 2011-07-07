#include "avigilon_resource.h"
#include "../dataprovider/avigilon_client_pull.h"


QnPlAvigilonResource::QnPlAvigilonResource()
{
 
}

QnPlAvigilonResource::~QnPlAvigilonResource()
{

}


QnResource::DeviceType QnPlAvigilonResource::getDeviceType() const
{
    return VIDEODEVICE;
}

QString QnPlAvigilonResource::toString() const
{
    return QString("live clearpix ") + getMAC();
}

QnStreamDataProvider* QnPlAvigilonResource::getDeviceStreamConnection()
{
    return new CLAvigilonStreamreader(this);
}

bool QnPlAvigilonResource::unknownDevice() const
{
    return false;
}

QnNetworkResourcePtr QnPlAvigilonResource::updateDevice()
{
    return QnNetworkResourcePtr(0);
}
