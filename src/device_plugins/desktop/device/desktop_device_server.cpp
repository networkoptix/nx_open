#ifdef Q_OS_WIN

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

DesktopDeviceServer::DesktopDeviceServer()
{
    m_pD3D = Direct3DCreate9(D3D_SDK_VERSION);
}

DesktopDeviceServer::~DesktopDeviceServer()
{
    m_pD3D->Release();
}

CLDeviceList DesktopDeviceServer::findDevices()
{
    CLDeviceList result;

    if (m_pD3D == 0)
        return result;

    D3DDISPLAYMODE ddm;

    for(int i = 0;; i++)
    {
        bool needBreak = false;
#ifdef ONLY_PRIMARY_DESKTOP
        needBreak = i > 0;
#endif
        if(needBreak || FAILED(m_pD3D->GetAdapterDisplayMode(i,&ddm)))
            break;

        CLDevice* dev = new CLDesktopDevice();
        dev->setUniqueId(QLatin1String("Desktop") + QString::number(i+1));
        result[dev->getUniqueId()] = dev;
    }
    return result;
}

#endif // Q_OS_WIN
