// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "windows_desktop_resource_searcher_impl.h"

#include <windows.h>
//^ Windows header must be included first (actual for no-pch build only).

#include <commdlg.h>
#include <d3d9.h>
#include <d3dx9.h>
#include <shellapi.h>

#include <client/client_runtime_settings.h>

#include "windows_desktop_resource.h"

#define ONLY_PRIMARY_DESKTOP

namespace nx::vms::client::desktop {

WindowsDesktopResourceSearcherImpl::WindowsDesktopResourceSearcherImpl(QOpenGLWidget* mainWidget):
    m_mainWidget(mainWidget),
    m_pD3D(Direct3DCreate9(D3D_SDK_VERSION))
{
}

WindowsDesktopResourceSearcherImpl::~WindowsDesktopResourceSearcherImpl()
{
    if (m_pD3D)
        m_pD3D->Release();
}

QnResourceList WindowsDesktopResourceSearcherImpl::findResources()
{
    if (qnRuntime->isVideoWallMode() || qnRuntime->isAcsMode())
        return QnResourceList();

    QnResourceList result;
    if (!m_pD3D)
        return result;

    D3DDISPLAYMODE ddm;
    for (int i = 0;; ++i)
    {
#ifdef ONLY_PRIMARY_DESKTOP
        if (i > 0)
            break;
#endif
        if (FAILED(m_pD3D->GetAdapterDisplayMode(i, &ddm)))
            break;

        QnResourcePtr dev(new WindowsDesktopResource(m_mainWidget));
        result.push_back(dev);
    }

    return result;
}

} // namespace nx::vms::client::desktop
