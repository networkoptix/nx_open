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
    return QString("live android ") + getUniqueId().toString();
}

QnStreamDataProvider* QnPlANdroidResource::getDeviceStreamConnection()
{
    return new CLAndroidStreamreader(this);
}

bool QnPlANdroidResource::unknownDevice() const
{
    return false;
}

QnNetworkResourcePtr QnPlANdroidResource::updateDevice()
{
    return QnNetworkResourcePtr(0);
}
