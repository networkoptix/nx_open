#ifndef __DESKTOP_DEVICE_SERVER_H
#define __DESKTOP_DEVICE_SERVER_H

#include "../../../device/deviceserver.h"

class IDirect3D9;

class DesktopDeviceServer : public CLDeviceServer
{
    DesktopDeviceServer();
public:

    ~DesktopDeviceServer();

    static DesktopDeviceServer& instance();

    virtual bool isProxy() const { return false; }
    // return the name of the server 
    virtual QString name() const { return "Desktop";}

    // returns all available devices 
    virtual CLDeviceList findDevices();
private:
    IDirect3D9*			m_pD3D;
};

#endif // __DESKTOP_DEVICE_SERVER_H
