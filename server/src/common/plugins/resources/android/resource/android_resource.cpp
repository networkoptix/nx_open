#include "android_resource.h"
#include "../dataprovider/android_stream_reader.h"


QnPlANdroidResource::QnPlANdroidResource()
{
    setAfterRouter(true); // wifi, huli
}

QnPlANdroidResource::~QnPlANdroidResource()
{

}


QnResource::DeviceType QnPlANdroidResource::getDeviceType() const
{
    return VIDEODEVICE;
}

QString QnPlANdroidResource::toString() const
{
    return QString("live android ") + getId().toString();
}

QnMediaStreamDataProvider* QnPlANdroidResource::getDeviceStreamConnection()
{
    return new CLAndroidStreamreader(this);
}

bool QnPlANdroidResource::unknownResource() const
{
    return false;
}

QnNetworkResourcePtr QnPlANdroidResource::updateResource()
{
    return QnNetworkResourcePtr(0);
}
