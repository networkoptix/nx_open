#ifndef android_h_2054_h
#define android_h_2054_h

#include "resource/network_resource.h"


class CLANdroidDevice : public CLNetworkDevice
{
public:
    CLANdroidDevice ();
    ~CLANdroidDevice ();

    DeviceType getDeviceType() const;

    QString toString() const;

    QnStreamDataProvider* getDeviceStreamConnection();

    virtual bool unknownDevice() const;
    CLNetworkDevice* updateDevice();

};



#endif //avigilon_h_16_43_h