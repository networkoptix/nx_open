#pragma once

#ifdef _WIN32

#include <mfx/mfxvideo++.h>
#include <d3d9.h>
#include <atlbase.h>
#include <dxva2api.h>

namespace nx::media::quick_sync::windows {

class DeviceHandle
{
public:
    DeviceHandle(MFXVideoSession& session);
    IDirect3DDeviceManager9* getHandle()
    {
       return deviceManager9;
    }

private:
    bool createDeviceManager(int adapter);

private:
    CComPtr<IDirect3DDeviceManager9> deviceManager9;
    CComPtr<IDirect3DDevice9Ex> D3DD9;
    CComPtr<IDirect3D9Ex> D3D9;
};

} // namespace nx::media::quick_sync::windows

#endif // _WIN32


