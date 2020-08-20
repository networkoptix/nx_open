#pragma once

#ifdef _WIN32

#include <mfx/mfxvideo++.h>
#include <d3d9.h>
#include <atlbase.h>
#include <dxva2api.h>

#include "renderer.h"
#include "window_cache.h"

namespace nx::media::quick_sync::windows {

class DeviceHandle
{
public:
    ~DeviceHandle();
    bool createDevice(int width, int height, mfxU32 adapterNumber);

    IDirect3DDeviceManager9* getDeviceManager() { return m_deviceManager; }
    Renderer& getRenderer() { return m_renderer; }

private:
    CComPtr<IDirect3DDeviceManager9> m_deviceManager;
    CComPtr<IDirect3D9Ex> m_d3d;
    CComPtr<IDirect3DDevice9Ex> m_device;

    HWND m_hWnd = 0;
    Renderer m_renderer;
    static thread_local WindowCache m_windowCache;
};

} // namespace nx::media::quick_sync::windows

#endif // _WIN32
