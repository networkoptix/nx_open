#pragma once

#ifdef _WIN32

#include <d3d9.h>
#include <dxva2api.h>
#include <dxva.h>
#include <windows.h>

#include <mfx/mfxvideo++.h>

namespace nx::media::quick_sync::windows {

class Renderer
{
public:
    ~Renderer();

    bool init(HWND window, IDirect3DDevice9Ex* device, IDirect3D9Ex* d3d, int width, int height);
    bool render(mfxFrameSurface1* mfxSurface, bool isNewTexture, GLuint textureId);

private:
    bool initRenderSurface();

private:
    IDirect3DDevice9Ex* m_device = nullptr;
    IDirect3D9Ex* m_d3d = nullptr;
    IDirect3DSurface9* m_renderTargetSurface = nullptr;
    IDirect3DSurface9* m_sharedSurface = nullptr;
    HANDLE m_sharedSurfaceHandle = 0;

    GLuint m_texture = 0;
    HANDLE m_textureHandle = 0;
    HANDLE m_renderDeviceHandle = 0;
    HWND m_window = 0;
    HDC m_dc = 0;
    HGLRC m_rc = 0;
    int m_width = 0;
    int m_height = 0;
};

} // namespace nx::media::quick_sync::windows

#endif // _WIN32

