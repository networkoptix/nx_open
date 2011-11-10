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

QString DesktopDeviceServer::manufacture() const
{
    return "NetworkOptix";
}

QnResourceList DesktopDeviceServer::findResources()
{
    QnResourceList result;

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

        QnResourcePtr dev(new CLDesktopDevice(i));
        result.push_back(dev);
    }
    return result;
}

bool DesktopDeviceServer::isResourceTypeSupported(const QnId& resourceTypeId) const
{
    return false;
}

QnResourcePtr DesktopDeviceServer::createResource(const QnId& resourceTypeId, const QnResourceParameters& parameters)
{
    return QnResourcePtr(0);
}

#endif // Q_OS_WIN
