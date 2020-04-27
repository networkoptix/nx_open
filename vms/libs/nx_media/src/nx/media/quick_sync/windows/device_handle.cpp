#ifdef _WIN32

#include <nx/utils/log/log.h>

#include "device_handle.h"

namespace nx::media::quick_sync::windows {

bool DeviceHandle::createDeviceManager(int adapterNumber)
{
    HWND window = nullptr;
    if (window == nullptr)
    {
        POINT point = {0, 0};
        window = WindowFromPoint(point);
    }

    HRESULT hr = Direct3DCreate9Ex(D3D_SDK_VERSION, &D3D9);
    if (!D3D9 || FAILED(hr))
    {
        NX_DEBUG(this, "Failed to create device, error code: %1", std::system_category().message(hr));
        return false;
    }

    RECT rc;
    GetClientRect(window, &rc);

    D3DPRESENT_PARAMETERS D3DPP;
    memset(&D3DPP, 0, sizeof(D3DPP));
    D3DPP.Windowed = true;
    D3DPP.hDeviceWindow = window;
    D3DPP.Flags = D3DPRESENTFLAG_VIDEO;
    D3DPP.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;
    D3DPP.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
    D3DPP.BackBufferCount = 1;
    D3DPP.BackBufferFormat = D3DFMT_A8R8G8B8;
    D3DPP.BackBufferWidth = rc.right - rc.left;
    D3DPP.BackBufferHeight = rc.bottom - rc.top;
    D3DPP.Flags |= D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
    D3DPP.SwapEffect = D3DSWAPEFFECT_DISCARD;

    hr = D3D9->CreateDeviceEx(adapterNumber,
        D3DDEVTYPE_HAL,
        window,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED | D3DCREATE_FPU_PRESERVE,
        &D3DPP,
        nullptr,
        &D3DD9);
    if (FAILED(hr))
    {
        NX_DEBUG(this, "Failed to create device for adapter %1, error code: %2", adapterNumber,
            std::system_category().message(hr));
        return false;
    }

    hr = D3DD9->ResetEx(&D3DPP, nullptr);
    if (FAILED(hr))
    {
        NX_DEBUG(this, "Failed to reset device: %1", std::system_category().message(hr));
        return false;
    }

    hr = D3DD9->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
    if (FAILED(hr))
    {
        NX_DEBUG(this, "Failed to clear device, error code: %1",
            std::system_category().message(hr));
        return false;
    }

    UINT resetToken = 0;
    hr = DXVA2CreateDirect3DDeviceManager9(&resetToken, &deviceManager9);
    if (FAILED(hr))
    {
        deviceManager9 = nullptr;
        NX_DEBUG(this, "Failed to create device manager, error code: %1",
            std::system_category().message(hr));
        return false;
    }

    hr = deviceManager9->ResetDevice(D3DD9, resetToken);
    if (FAILED(hr))
    {
        NX_DEBUG(this, "Failed to reset device manager, error code: %1",
            std::system_category().message(hr));
        return false;
    }
    return true;
}

DeviceHandle::DeviceHandle(MFXVideoSession& /*session*/)
{
   // int adapter = getIntelDeviceAdapter(session);
    //createDeviceManager(adapter);
}

}  // namespace nx::media::quick_sync::windows

#endif // _WIN32

