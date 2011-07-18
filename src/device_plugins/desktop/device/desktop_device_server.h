#ifndef __DESKTOP_DEVICE_SERVER_H
#define __DESKTOP_DEVICE_SERVER_H

#include "../../../device/deviceserver.h"

class DesktopDeviceServer : public CLDeviceServer
{
    DesktopDeviceServer() {}
public:

    ~DesktopDeviceServer() {}

    static DesktopDeviceServer& instance();

    virtual bool isProxy() const { return false; }
    // return the name of the server 
    virtual QString name() const { return "Desktop";}

    // returns all available devices 
    virtual CLDeviceList findDevices();
private:
};

#endif // __DESKTOP_DEVICE_SERVER_H
