#include <windows.h>
#include <shellapi.h>
#include <commdlg.h>
#include <d3d9.h>
#include <d3dx9.h>

#include "desktop_device_server.h"
#include "desktop_device.h"

#define ONLY_PRIMARY_DESKTOP

DesktopDeviceServer& DesktopDeviceServer::instance()
{
    static DesktopDeviceServer inst;
    return inst;
}

CLDeviceList DesktopDeviceServer::findDevices()
{
    CLDeviceList result;

    D3DDISPLAYMODE ddm;
    IDirect3D9*			pD3D;
    if((pD3D=Direct3DCreate9(D3D_SDK_VERSION))==NULL)
        return result;

    for(int i = 0;; i++)
    {
        bool needBreak = false;
#ifdef ONLY_PRIMARY_DESKTOP
        needBreak = i > 0;
#endif
        if(needBreak || FAILED(pD3D->GetAdapterDisplayMode(i,&ddm)))
        {
            pD3D->Release();
            return result;
        }

        CLDevice* dev = new CLDesktopDevice();
        dev->setUniqueId(QString("Desktop")+QString::number(i+1));
        result[dev->getUniqueId()] = dev;
    }    
}
