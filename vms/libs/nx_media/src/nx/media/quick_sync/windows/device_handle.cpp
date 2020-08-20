#ifdef _WIN32

#include <vector>

#include <windowsx.h>
#include <stdio.h>
#include <system_error>

#include "device_handle.h"

#include <nx/utils/log/log.h>

namespace nx::media::quick_sync::windows {

thread_local WindowCache DeviceHandle::m_windowCache;

DeviceHandle::~DeviceHandle()
{
    m_renderer.close();
    if (m_hWnd)
        m_windowCache.releaseWindow(m_hWnd);
}

bool DeviceHandle::createDevice(int width, int height, mfxU32 adapterNumber)
{
    NX_DEBUG(this, "Create directx device, resolution: %1x%2", width, height);
    m_hWnd = m_windowCache.getWindow(width, height);

    D3DPRESENT_PARAMETERS d3dpp;
    ZeroMemory(&d3dpp, sizeof(d3dpp));
    d3dpp.Windowed = TRUE;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.hDeviceWindow = m_hWnd;
    d3dpp.BackBufferFormat = D3DFMT_X8R8G8B8;

    // A D3D9EX device is required to create the sharedSurface
    HRESULT hr = Direct3DCreate9Ex(D3D_SDK_VERSION, &m_d3d);
    if (!m_d3d || FAILED(hr))
    {
        NX_DEBUG(this, "Failed to create device, error code: %1", std::system_category().message(hr));
        return false;
    }

    // The interop definition states D3DCREATE_MULTITHREADED is required, but it may vary by vendor
    hr = m_d3d->CreateDeviceEx(adapterNumber,
        D3DDEVTYPE_HAL,
        m_hWnd,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED,
        &d3dpp,
        NULL,
        &m_device);
    if (FAILED(hr))
    {
        NX_DEBUG(this, "Failed to create device for adapter %1, error code: %2", adapterNumber,
            std::system_category().message(hr));
        return false;
    }

    // Verify that the hardware can do NV12->RGB conversion
    hr = m_d3d->CheckDeviceFormatConversion(
        D3DADAPTER_DEFAULT,
        D3DDEVTYPE_HAL,
        (D3DFORMAT) MAKEFOURCC('N', 'V', '1', '2'),
        D3DFMT_X8R8G8B8);
    if (FAILED(hr))
    {
        NX_DEBUG(this, "Failed to check format converison nv12->rgb, error code %1",
            std::system_category().message(hr));
        return false;
    }

    hr = m_device->SetRenderState(D3DRS_LIGHTING, FALSE);
    if (FAILED(hr))
    {
        NX_DEBUG(this, "Failed to set render state D3DRS_LIGHTING, adapter %1, error code: %2",
            adapterNumber, std::system_category().message(hr));
        return false;
    }

    hr = m_device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    if (FAILED(hr))
    {
        NX_DEBUG(this, "Failed to set render state D3DRS_CULLMODE for adapter %1, error code: %2",
            adapterNumber, std::system_category().message(hr));
        return false;
    }

    hr = m_device->ResetEx(&d3dpp, NULL);
    if (FAILED(hr))
    {
        NX_DEBUG(this, "Failed to reset device: %1", std::system_category().message(hr));
        return false;
    }

    hr = m_device->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
    if (FAILED(hr))
    {
        NX_DEBUG(this, "Failed to clear device, error code: %1",
            std::system_category().message(hr));
        return false;
    }

    UINT resetToken = 0;
    hr = DXVA2CreateDirect3DDeviceManager9(&resetToken, &m_deviceManager);
    if (FAILED(hr))
    {
        NX_DEBUG(this, "Failed to create device manager, error code: %1",
            std::system_category().message(hr));
        return false;
    }

    hr = m_deviceManager->ResetDevice(m_device, resetToken);
    if (FAILED(hr))
    {
        NX_DEBUG(this, "Failed to reset device manager, error code: %1",
            std::system_category().message(hr));
        return false;
    }

    if (!m_renderer.init(m_hWnd, m_device, m_d3d))
       return false;

    return true;
}

} // namespace nx::media::quick_sync::windows

#endif // _WIN32
