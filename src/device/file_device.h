#ifndef file_device_h_227
#define file_device_h_227

#include "device.h"

class CLFileDevice : public CLDevice
{
public:
    CLFileDevice(const QString &filename);

    inline DeviceType getDeviceType() const
    { return VIDEODEVICE; }

    inline QString toString() const
    { return getName(); }
    inline QString getFileName() const
    { return getUniqueId(); }

    virtual CLStreamreader* getDeviceStreamConnection();
};

#endif // file_device_h_227
